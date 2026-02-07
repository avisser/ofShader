#pragma once

#include "ofMain.h"

#include <array>
#include <string>
#include <vector>

class VisionHandPoseDetector {
public:
    enum class Finger {
        Thumb = 0,
        Index,
        Middle,
        Ring,
        Pinky,
        Count
    };

    bool setup(float scale = 0.5f, float minConfidence = 0.35f, int maxHands = 2);
    void setScale(float scale);
    void setMinConfidence(float minConfidence);
    void setMaxHands(int maxHands);
    void setFingerEnabled(Finger finger, bool enabled);
    void setEnabledFingers(const std::array<bool, 5> &enabled);
    struct HandPoint {
        ofVec2f tip;
        ofVec2f dir;
        float confidence = 0.0f;
    };

    bool detect(const ofPixels &pixels, std::vector<HandPoint> &outPoints);
    const std::string &getLastError() const { return lastError; }

private:
    float scale = 0.5f;
    float minConfidence = 0.35f;
    int maxHands = 2;
    std::array<bool, 5> fingerEnabled = {true, true, true, true, true};
    std::string lastError;
};
