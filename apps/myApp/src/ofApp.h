#pragma once

#include "ofMain.h"

#include <opencv2/core.hpp>
#include <opencv2/video/background_segm.hpp>

#include <string>
#include <vector>

struct AppConfig {
    std::string bgPath = "bg.jpg";
    int camIndex = 0;
    int camWidth = 1280;
    int camHeight = 720;
    int camFps = 30;
};

class ofApp : public ofBaseApp {
public:
    explicit ofApp(const AppConfig &config);

    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void exit() override;

private:
    void listCameras();
    void startCamera(int index);
    void resetBackgroundSubtractor();
    void updateComposite();
    void drawTextureCover(ofTexture &tex, float dstW, float dstH, bool mirrorX);
    void printSettings();
    void setupKeyShader();

    AppConfig config;

    ofVideoGrabber grabber;
    std::vector<ofVideoDevice> devices;
    int currentDevice = 0;

    cv::Ptr<cv::BackgroundSubtractorMOG2> bgSub;
    cv::Mat mask;

    bool enableMorph = true;
    bool enableBlur = true;
    bool detectShadows = true;
    int maskThreshold = 200;

    ofImage bgImage;
    bool bgLoaded = false;

    ofPixels rgbaPixels;
    ofTexture rgbaTexture;
    bool compositeReady = false;

    ofShader keyShader;
    bool shaderReady = false;
    bool useShaderKey = true;
    float keyHueDeg = 120.0f;
    float keyHueRangeDeg = 20.0f;
    float keyMinSat = 0.25f;
    float keyMinVal = 0.2f;
    float posterizeLevels = 6.0f;
    float edgeStrength = 1.1f;
};
