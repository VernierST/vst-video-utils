//
//  VSTVideoTracker.cpp
//  Video Physics
//
//  Created by Joseph Kelly on 6/24/20.
//  Copyright © 2020 Vernier Software & Technology. All rights reserved.
//

#include "VSTVideoTracker.hpp"
#include "Deferral.hpp"

#include <math.h>
#include <chrono>

using namespace vst;
using namespace cv;

cv::Point CenterOf(const cv::Rect& r) {
    return cv::Point(r.x + nearbyintf((float)r.width / 2), r.y + nearbyintf((float)r.height / 2));
}

cv::Rect Expand(const cv::Rect& r, float scalar) {
    float dw = r.width * scalar;
    float dh = r.height * scalar;
    return cv::Rect(nearbyintf(r.x - dw), nearbyintf(r.y - dh), nearbyintf(r.width + dw), nearbyintf(r.height + dh));
}

cv::Rect FitRect(const cv::Rect& rect, const cv::Rect& frame) {
    float x = rect.x;
    float y = rect.y;
    float w = rect.width;
    float h = rect.height;

    if (y < frame.y) // top
        y = frame.y;

    if ((y + h) > (frame.y + frame.height)) // bottom
    {
        if (h > frame.height) // height needs to change
        {
            h = frame.height;
            y = frame.y;
        }
        else
        {
            y = (frame.y + frame.height) - h;
        }
    }

    if (x < frame.x) // left
        x = frame.x;

    if ((x + w) > (frame.x + frame.width)) // right
    {
        if (w > frame.width) // width needs to change
        {
            w = frame.width;
            x = frame.x;
        }
        else
        {
            x = (frame.x + frame.width) - w;
        }
    }

    return cv::Rect(nearbyintf(x), nearbyintf(y), nearbyintf(w), nearbyintf(h));
}

TrackerResult VSTVideoTracker::TrackObjectInFrame(const cv::Mat& frame, double timeStamp) {
    // Bump the frame count whenever we return from this function
    DEFER([this] {
        _frameCount++;
        _frameCountInSubtractionPeriod++;
    });

    try {
        Mat mask;
        Mat searchFrame;

        size_t width = frame.cols;
        size_t height = frame.rows;

        cv::Point ctrOfObj;
        cv::Rect searchRect;

        auto startTime = std::chrono::steady_clock::now();
        //    double trackingTimeSubset = 0.0;
        //    int framesReadInCurrentSubtractionPeriod = 0;

        cv::Rect objLoc;
        cv::Rect objLocInFrame = _templateArea;

        // Special handling if this is the first frame in the tracking series:
        if (_frameCount == 0)
        {
            // Calculate initial template and histogram:
            // Note: Mat::operator() in use:
            _lastObjectLocation = _templateArea;
            _template = frame(_templateArea).clone();
            DrawDetectedEdges(_template, Mat());
            CalculateHistogram(_template, _histogram);

            // Set foreground baseline:
            _subtractor->apply(frame, _foreground, .01); // TODO: figure out the constant to use for the initial frame.

            // Prime the suspicion engine by passing in the initial starting template center:
            cv::Point center = CenterOf(_templateArea);
            _engine->PointIsValid(center, timeStamp);

            // Return the initial starting template center:
            return TrackerResult(center, timeStamp);
        }

        // 1. Figure out where in the frame we want to search which is about ~1/3 of the frame
        const float kSearchFactor = 0.33333333;
#define Multiply(s1, s2) (int) ((float) s1 * s2)
        searchRect = cv::Rect(_lastObjectLocation.x + _delta.x - Multiply(width, kSearchFactor / 2),
                              _lastObjectLocation.y + _delta.y - Multiply(height, kSearchFactor / 2),
                              _lastObjectLocation.width + Multiply(width, kSearchFactor),
                              _lastObjectLocation.height + Multiply(height, kSearchFactor));

        // cv:Mat will assert if you go outside of it's width and height
        searchRect = FitRect(searchRect, cv::Rect(0, 0, (int)width, (int)height));

        // 2. Crop the frame and only draw it's edges. Note: Mat overrides operator() to take a
        // rectangle which returns a Mat of the subsection. We clone it because we'd like to draw
        // edges on top of it.
        searchFrame = frame(searchRect).clone();

        // Get the previous objects location in the new search rect
        // This is needed because the search rect has changed.
        objLocInFrame.x -= searchRect.x;
        objLocInFrame.y -= searchRect.y;

        if (_frameCountInSubtractionPeriod % _subtractionPeriod == 0)
        {
            _subtractor->apply(frame, _foreground, .01);
            mask = SubtractBackground(_foreground, searchRect, objLocInFrame);
            DrawDetectedEdges(searchFrame, mask);
        }
        else
        {
            DrawDetectedEdges(searchFrame, Mat());
        }

        // 3. Find the object in the cropped frame and update the objects offsets
        // The search frame algorithm is about a ~ 78% decrease in processing time
        objLoc = FindObjectUsing(_template, searchFrame);

        objLocInFrame = cv::Rect(objLoc.x + searchRect.x,
                                 objLoc.y + searchRect.y,
                                 objLoc.width,
                                 objLoc.height);
        ctrOfObj = CenterOf(objLocInFrame);

        cv::Point ctrOfPrevObj = CenterOf(_lastObjectLocation);
        _delta = cv::Point(ctrOfObj.x - ctrOfPrevObj.x, ctrOfObj.y - ctrOfPrevObj.y);

        // Ask suspicion engine if this point looks valid
        if (!_engine->PointIsValid(ctrOfObj, timeStamp)) {
            return TrackerResult(TrackerResult::suspicionFailure);
        }

        Mat hist;
        _lastObjectLocation = objLocInFrame;

        Mat newObject = searchFrame(objLoc).clone();
        CalculateHistogram(newObject, hist);

        // Update tracking image & histogram if Correlation is 90% or greater
        auto comp = compareHist(_histogram, hist, HISTCMP_CORREL); // CV_COMP_CORREL
        if (comp >= .9)
        {
            _template = newObject;
            _histogram = hist;
        }

        // Adjust subtraction period based on performance.
        auto endTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsedTime = endTime-startTime;
        _subtractionPeriodAdjustDuration += elapsedTime.count();

        if ((_frameCount-1) % kCheckFramesTimeEveryNumberOfFrames == 0) {
            if (_subtractionPeriodAdjustDuration / kCheckFramesTimeEveryNumberOfFrames > kReadFramesTimeThreshold)
            {
                if (_subtractionPeriod < 10) {
                    _subtractionPeriod += 1;
                }
            } else {
                if (_subtractionPeriod > 1) {
                    _subtractionPeriod -= 1;
                }
            }

            _subtractionPeriodAdjustDuration = 0;
            _frameCountInSubtractionPeriod = 0;
        }

        return TrackerResult(ctrOfObj, timeStamp);
    } catch (const cv::Exception& exp) {
        return TrackerResult(TrackerResult::openCVError);
    }
    return TrackerResult(TrackerResult::otherFailure);
}

void VSTVideoTracker::Reset(const cv::Rect& templateArea) {
    _engine->Reset();
    _frameCount = 0;
    _templateArea = templateArea;
    _delta = cv::Point();
    _frameCount = 0;
    _subtractionPeriod = 1;
    _frameCountInSubtractionPeriod = 0;
    _subtractionPeriodAdjustDuration = 0;
}

void VSTVideoTracker::CalculateHistogram(const cv::Mat& matrix, cv::Mat& historgramOut) {
    int histSize = 256;
    float range[] = {0, 256};
    const float* histRange = {range};

    calcHist(&matrix, 1, 0, noArray(), historgramOut, 1, &histSize, &histRange);
}

cv::Mat VSTVideoTracker::SubtractBackground(const cv::Mat& fore, const cv::Rect& searchRect, const cv::Rect& objLoc) {
    int frameX;
    int frameY;
    int frameWidth;
    int frameHeight;

    if (searchRect.width == 0 && searchRect.height == 0)
    {
        frameX = 0;
        frameY = 0;
        frameWidth = fore.cols;
        frameHeight = fore.rows;
    }
    else
    {
        frameX = searchRect.x;
        frameY = searchRect.y;
        frameWidth = searchRect.width;
        frameHeight = searchRect.height;
    }

    if (fore.empty())
        return Mat();

    // Only get a mask of the part we care about
    Mat mask = fore(cv::Rect(frameX, frameY, frameWidth, frameHeight)).clone();

    // Open up the mask
    erode(mask, mask, noArray());
    dilate(mask, mask, noArray());

    if (objLoc.width == 0 && objLoc.height == 0)
        return mask;

    // Contain the objLoc in the searchRect or else openCV asserts
    // X and Y are zero because ObjLoc is already in searchFrame coordinates
    cv::Rect ObjLocInFrame = FitRect(objLoc, cv::Rect(0, 0, frameWidth, frameHeight));

    // Unmask the objects previous location in case object is currently not in motion
    Range objWidthRange = Range(ObjLocInFrame.x, ObjLocInFrame.x+ObjLocInFrame.width);
    Range objHeightRange = Range(ObjLocInFrame.y, ObjLocInFrame.y+ObjLocInFrame.height);
    // Note: the following line combines overridden operator() and operator=, and its net-effect
    // is to set the area of the mask defined by the two ranges to 1, aka opaque.
    mask(objHeightRange, objWidthRange) = 1;
    return mask;
}

void VSTVideoTracker::DrawDetectedEdges(cv::Mat& matInOut, const cv::Mat& mask) {
    // Grayscale and blur
    cvtColor(matInOut, matInOut, COLOR_BGRA2GRAY);
    GaussianBlur(matInOut, matInOut, cv::Size(3,3), 0);

    // Sobel
    Mat gradX, gradY;
    Mat absGradX, absGradY;
    Sobel(matInOut, gradX, CV_16S, 1, 0); // x
    Sobel(matInOut, gradY, CV_16S, 0, 1); // y
    convertScaleAbs(gradX, absGradX);
    convertScaleAbs(gradY, absGradY);
    addWeighted(absGradX, 0.5, absGradY, 0.5, 0, matInOut);

    if (mask.empty())
        return;

    // Apply mask if needed.
    Mat maskedMat;
    matInOut.copyTo(maskedMat, mask);
    // Note: operator= override. Also, sigh.
    matInOut = maskedMat;
}

cv::Rect VSTVideoTracker::FindObjectUsing(const cv::Mat& imgObject, const cv::Mat& frame) {
    cv::Point minLoc;
    cv::Point maxLoc;
    cv::Point matchLoc;

    // The best matching method choices are:
    // TM_SQDIFF, TM_SQDIFF_NORMED, TM_CCORR_NORMED
    int matchMethod = TM_SQDIFF;

    // Create a matrix to store our template matching results
    Mat result(frame.cols - imgObject.cols + 1, frame.rows - imgObject.rows + 1, CV_32FC1);
    matchTemplate(frame, imgObject, result, matchMethod);

    // Find the highest & lowest values and their points
    minMaxLoc(result, 0, 0, &minLoc, &maxLoc);

    // For SQDIFF and SQDIFF_NORMED, the best matches are lower values.
    // For all the other methods, the higher the better
    if( matchMethod  == TM_SQDIFF || matchMethod == TM_SQDIFF_NORMED )
        matchLoc = minLoc;
    else
        matchLoc = maxLoc;

    return cv::Rect(matchLoc.x, matchLoc.y, imgObject.cols, imgObject.rows);
}
