//
//  VSTSuspicionEngine.hpp
//  Video Physics
//
//  Created by Joseph Kelly on 6/24/20.
//  Copyright © 2020 Vernier Software & Technology. All rights reserved.
//

#ifndef VSTSuspicionEngine_hpp
#define VSTSuspicionEngine_hpp

#include <opencv2/opencv.hpp>

namespace vst {

class VSTSuspicionEngine {
public:
    VSTSuspicionEngine(float frameRate = 30.0, float suspicionFactor = 5.0) : _frameRate(frameRate), _suspicionFactor(suspicionFactor) {}
    bool PointIsValid(cv::Point point, float timeStamp);
    void Reset();

private:
    int _instanceCount = 0;
    float _accelerationSum = 0.0;
    float _frameRate = 30.0;
    float _suspicionFactor = 5.0;
    float _lastX = 0;
    float _lastY = 0;
    float _lastTime = 0;
    float _lastVelo = 0;
};

};

#endif /* VSTSuspicionEngine_hpp */
