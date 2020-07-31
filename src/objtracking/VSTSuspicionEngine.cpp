//
//  VSTSuspicionEngine.cpp
//  Video Physics
//
//  Created by Joseph Kelly on 6/24/20.
//  Copyright © 2020 Vernier Software & Technology. All rights reserved.
//

#include "VSTSuspicionEngine.hpp"
#include "Deferral.hpp"

#include <math.h>

using namespace vst;

bool VSTSuspicionEngine::PointIsValid(cv::Point p, float timeStamp) {

    ++_instanceCount;

    if (_instanceCount == 1) {
        _lastX = p.x;
        _lastY = p.y;
        _lastTime = timeStamp;
        return true;
    }

    // We need a large data set before averages are any good as a comparison
    if (_instanceCount < 5) {
        return true;
    }
    float deltaX = p.x - _lastX, deltaY = p.y - _lastY;
    float scalarLocation = sqrtf(deltaX * deltaX + deltaY * deltaY);

    float velo = scalarLocation * (1.0 / _frameRate);
    float deltaVelo = abs(velo - _lastVelo);
    float deltaTime = timeStamp - _lastTime;

    float accel = deltaVelo / deltaTime;
    _accelerationSum += accel;
    _lastX = p.x;
    _lastY = p.y;
    _lastTime = timeStamp;
    _lastVelo = velo;

    float averageAccel = _accelerationSum / _instanceCount;
    if (accel > averageAccel * _suspicionFactor)
        return false;

    return true;

}

void VSTSuspicionEngine::Reset() {
    _instanceCount = 0;
    _accelerationSum = 0.0;
    _frameRate = 30.0;
    _lastX = 0;
    _lastY = 0;
    _lastTime = 0;
    _lastVelo = 0;
}

