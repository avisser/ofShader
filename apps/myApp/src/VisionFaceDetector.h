#pragma once

#include "ofMain.h"

#include <string>
#include <vector>

class VisionFaceDetector {
public:
    bool setup(float scale = 0.5f);
    void setScale(float scale);
    bool detect(const ofPixels &pixels, std::vector<ofRectangle> &outFaces);
    const std::string &getLastError() const { return lastError; }

private:
    float scale = 0.5f;
    std::string lastError;
};
