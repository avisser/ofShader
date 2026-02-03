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
    void updateMotion(const ofPixels &camPixels);
    void updateTrail(float dt);
    void drawTrail();
    ofVec2f mapCameraToScreen(const ofVec2f &camPos, float camW, float camH, bool mirrorX);
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
    float keyHueRangeDeg = 60.0f;
    float keyMinSat = 0.25f;
    float keyMinVal = 0.2f;
    float posterizeLevels = 6.0f;
    float edgeStrength = 1.1f;
    float pulseBpm = 110.0f;
    float pulseAmount = 0.0f;
    float pulseColorize = 0.0f;
    float pulseHueShiftDeg = 18.0f;
    float pulseAttack = 0.08f;
    float pulseDecay = 1.8f;
    float pulseHueBoost = 2.0f;
    int pulseHueMode = 0;
    bool enableWoofer = true;
    float wooferStrength = 0.22f;
    float wooferFalloff = 1.5f;
    int wooferModeIndex = 0;
    bool enableKaleido = true;
    float kaleidoSegments = 6.0f;
    float kaleidoSpin = 0.0f;
    
    int kaleidoModeIndex = 2;
    bool enableHalftone = false;
    float halftoneScale = 14.0f;
    float halftoneEdge = 0.3f;
    int halftoneModeIndex = 0;

    bool enableTrail = true;
    ofFbo trailFbo;
    float trailFade = 0.04f;
    float trailSize = 38.0f;
    float trailOpacity = 0.8f;
    float motionLevel = 0.0f;
    ofVec2f motionCenter = {0.0f, 0.0f};
    ofFloatColor motionColor = ofFloatColor(1.0f, 1.0f, 1.0f, 1.0f);
    float motionThreshold = 0.02f;
    float motionCamW = 0.0f;
    float motionCamH = 0.0f;
    ofVec2f lastTrailPos = {0.0f, 0.0f};
    bool hasTrailPos = false;
    cv::Mat prevGray;
};
