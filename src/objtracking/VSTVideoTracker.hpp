//
//  VSTVideoTracker.hpp
//  Video Physics
//
//  Created by Joseph Kelly on 6/24/20.
//  Copyright © 2020 Vernier Software & Technology. All rights reserved.
//

#ifndef VSTVideoTracker_hpp
#define VSTVideoTracker_hpp

#include <opencv2/opencv.hpp>
#include <opencv2/video.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/features2d.hpp>

#include "VSTSuspicionEngine.hpp"

#include <memory>

namespace vst {

class VSTVideoTracker;

/// Struct which is a result of calls to `VSTVideoTracker::TrackObjectInFrame()`.
/// encapuslates a status code and, upon successful completion, a new object center
/// and timestamp.
class TrackerResult {
public:

    typedef enum {
        success = 0,
        suspicionFailure,
        openCVError,
        otherFailure,
    } OpStatus;

    /// Returns status; any status code besides `success` indicates a failure to track the
    /// object, hence `TimeStamp()` and `ObjectCenter()` will return invalid results.
    OpStatus    Status() const { return _status; }
    /// Returns time stamp in seconds of current found object in frame. Invalid if `Status() != success`.
    /// This will return the same value as passed into `VSTVideoTracker::TrackObjectInFrame()`.
    double      TimeStamp() const { return _timeStamp; }
    /// Returns center point of object found in frame. Invalid if `Status() != success`.
    cv::Point   ObjectCenter() const { return _foundCenter; }

private:
    friend class VSTVideoTracker;

    TrackerResult(OpStatus error) {
        _status = error;
    }

    TrackerResult(cv::Point center, double time) {
        _status = success;
        _foundCenter = center;
        _timeStamp = time;
    }

    OpStatus _status = success;
    cv::Point _foundCenter = cv::Point();
    double _timeStamp = 0;
};

/// Class that uses a handful of object tracking methods to track an object in successive
/// frames of a video.
///
/// To use, initialize or reset with a template area -- a rectangle indicating
/// the starting position of the object to track -- and successively call `TrackObjectInFrame()` for
/// all frames in a video.
///
/// This object will keep track of necessary state between successive calls to `TrackObjectInFrame()`
/// until you call Reset() which will set the state to its initial form.
class VSTVideoTracker {

private:
    const double kReadFramesTimeThreshold = 0.4;
    const int kCheckFramesTimeEveryNumberOfFrames = 4;
    const int kTrackingTemplatePaddingPercentage = 10;


public:

    /// Initializes an instance of VSTVideoTracker
    /// @param templateArea user-selected area indicating the starting position of object to track.
    /// @param subtractionPeriod number of frames to process before we reset the background. Use a number > 1 if performance is compromised.
    VSTVideoTracker(const cv::Rect& templateArea, int subtractionPeriod): _subtractionPeriod(subtractionPeriod) {
        Reset(templateArea);
    }

    /// Call this method with successive frames from the source video you are tracking.
    /// @param frame a matrix containing a successive frame of video from the source video
    /// @param timeStamp a value in seconds representing the corresponding time stamp of the passed in frame.
    /// @return TrackerResult object which contains position and (redundant) time of object OR an error code.
    TrackerResult
                TrackObjectInFrame(const cv::Mat& frame, double timeStamp);

    /// Reset the object tracking state with new starting template area. Call this to re-start object
    /// tracking from new position after encountering a tracking error.
    /// @param template user-selected area indicating the starting position of object to track.
    void        Reset(const cv::Rect& templateArea);

    cv::Mat&    Foreground() { return _foreground; }

private:
    static void CalculateHistogram(const cv::Mat& matrix, cv::Mat& historgramOut);
    cv::Mat     SubtractBackground(const cv::Mat& foreground, const cv::Rect& searchRect, const cv::Rect& objLoc);
    void        DrawDetectedEdges(cv::Mat& mat, const cv::Mat& mask);
    cv::Rect    FindObjectUsing(const cv::Mat& objTemplate, const cv::Mat& search);

private:
    cv::Rect    _templateArea;
    cv::Rect    _lastObjectLocation;
    cv::Point   _delta;
    cv::Mat     _template;
    cv::Mat     _histogram;
    cv::Mat     _foreground;
    cv::Ptr<cv::BackgroundSubtractorMOG2>
                _subtractor = cv::createBackgroundSubtractorMOG2(7, 3, false);
    std::unique_ptr<vst::VSTSuspicionEngine>
                _engine = std::unique_ptr<vst::VSTSuspicionEngine>(new vst::VSTSuspicionEngine());

    int         _frameCount = 0;
    int         _subtractionPeriod = 1;
    int         _frameCountInSubtractionPeriod = 0;
    double      _subtractionPeriodAdjustDuration = 0;
};

};

#endif /* VSTVideoTracker_hpp */
