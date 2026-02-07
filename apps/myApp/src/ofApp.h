#pragma once

#include "ofMain.h"

#include <opencv2/core.hpp>
#include <opencv2/video/background_segm.hpp>

#include <array>
#include <string>
#include <vector>

#include "MidiControl.h"
#include "VisionFaceDetector.h"
#include "VisionHandPoseDetector.h"

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
    void keyPressed(ofKeyEventArgs &key) override;
    void keyReleased(int key) override;
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
    void setupControls();
    void handleMidiControls();
    bool handleControlKey(int key,
                          bool shiftDown,
                          bool cmdDown,
                          bool altDown,
                          bool ctrlDown);
    void drawHelpOverlay();

    struct ControlSpec {
        std::string id;
        char key = 0;
        char learnKey = 0;
        std::vector<float> presets;
        float knobMin = 0.0f;
        float knobMax = 1.0f;
        bool hasOff = false;
        int presetIndex = 0;
        float value = 0.0f;
        bool enabled = true;
        bool muteHeld = false;
        float preMuteValue = 0.0f;
        bool preMuteEnabled = true;
        bool oscEnabled = false;
        float oscSpeed01 = 0.0f;
    };

    ControlSpec *findControlByKey(char key);
    ControlSpec *findControlById(const std::string &id);
    void cycleControlPreset(ControlSpec &control);
    void applyControl(const ControlSpec &control);
    float resolveControlValue(const ControlSpec &control) const;
    void emitHandSparks(float dt);
    void updateSparkParticles(float dt);

    AppConfig config;

    ofVideoGrabber grabber;
    std::vector<ofVideoDevice> devices;
    int currentDevice = 0;

    MidiControl midi;

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
    float pulseBpm = 60.0f;
    float pulseAmount = 0.0f;
    float pulseColorize = 0.0f;
    float pulseHueShiftDeg = 18.0f;
    float pulseAttack = 0.08f;
    float pulseDecay = 1.8f;
    float pulseHueBoost = 2.0f;
    int pulseHueMode = 0;
    bool enableWoofer = false;
    float wooferStrength = 0.22f;
    float wooferFalloff = 1.5f;
    int wooferModeIndex = 0;
    bool enableKaleido = true;
    float kaleidoSegments = 6.0f;
    float kaleidoSpin = 0.25f;
    float kaleidoSpinBase = 0.25f;
    bool kaleidoSpinFlip = false;
    int kaleidoExtremeState = 0;
    float kaleidoZoom = 0.7f;
    float kaleidoZoomKnobMin = 0.3f;
    float kaleidoZoomKnobMax = 1.0f;
    bool enableHalftone = false;
    float halftoneScale = 14.0f;
    float halftoneEdge = 0.3f;
    float halftoneKnobMin = 6.0f;
    float halftoneKnobMax = 30.0f;

    bool enableSaturation = false;
    float saturationScale = 1.0f;
    std::vector<ControlSpec> controls;
    float wetMix = 0.6f;
    float beatFlashSeconds = 0.12f;
    float beatDotRadius = 10.0f;
    float beatDownbeatRadius = 20.0f;

    ofFbo trailFbo;
    float trailFade = 0.04f;
    float motionLevel = 0.0f;
    ofFloatColor motionColor = ofFloatColor(1.0f, 1.0f, 1.0f, 1.0f);
    cv::Mat prevGray;

    VisionFaceDetector faceDetector;
    std::vector<ofRectangle> faceRects;
    bool enableFaceDetect = true;
    bool showFaceDebug = true;
    int faceDetectFrame = 0;
    int faceDetectInterval = 3;
    float faceDetectScale = 0.5f;

    struct SparkParticle {
        ofVec2f pos;
        ofVec2f prev;
        ofVec2f vel;
        ofFloatColor color;
        float age = 0.0f;
        float life = 1.0f;
        float size = 2.0f;
    };

    VisionHandPoseDetector handDetector;
    std::vector<VisionHandPoseDetector::HandPoint> handPoints;
    std::vector<SparkParticle> sparkParticles;
    bool enableHandSparkles = true;
    bool showHandDebug = false;
    bool showHelpOverlay = false;
    ofTrueTypeFont helpFont;
    int handDetectFrame = 0;
    int handDetectInterval = 2;
    float handDetectScale = 0.5f;
    float handSparkleSize = 18.0f;
    float handSparkleOpacity = 0.85f;
    std::array<bool, 5> handSparkleFingers = {false, true, true, false, false};
    
    float sparkEmitRate = 140.0f;
    float sparkSpeed = 2400.0f;
    float sparkSpread = 0.45f;
    float sparkLife = 1.4f;
    float sparkDrag = 0.93f;
    float sparkGravity = 220.0f;
    float sparkJitter = 40.0f;
    int maxSparkParticles = 2400;
};
