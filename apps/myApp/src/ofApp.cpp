#include "ofApp.h"
#include "KeyShaderSource.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>

namespace {
constexpr std::array<float, 6> kKaleidoModes = {0.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f};
constexpr std::array<float, 4> kHalftoneModes = {0.0f, 10.0f, 14.0f, 22.0f};
constexpr std::array<float, 5> kSaturationModes = {-1.0f, 0.2f, 0.45f, 0.7f, 0.9f};
constexpr std::array<float, 3> kKaleidoZoomModes = {0.9f, 0.7f, 0.5f};
constexpr std::array<float, 4> kTempoPresets = {60.0f, 80.0f, 100.0f, 120.0f};
constexpr std::array<float, 4> kWetMixPresets = {0.2f, 0.4f, 0.6f, 0.8f};
}

ofApp::ofApp(const AppConfig &config)
: config(config) {}

void ofApp::setup() {
    ofSetVerticalSync(true);
    ofSetFrameRate(config.camFps);
    ofSetFullscreen(true);
    setupKeyShader();
    midi.setup();
    setupControls();
    faceDetector.setup(faceDetectScale);
    handDetector.setup(handDetectScale);
    helpFont.load("Helvetica", 24, true, true);
    handDetector.setEnabledFingers(handSparkleFingers);

    listCameras();
    if (!devices.empty()) {
        int startIndex = config.camIndex;
        if (startIndex < 0 || startIndex >= static_cast<int>(devices.size())) {
            ofLogWarning() << "Camera index " << startIndex << " out of range, using 0.";
            startIndex = 0;
        }
        startCamera(startIndex);
    } else {
        ofLogWarning() << "No camera devices detected.";
    }

    bgLoaded = bgImage.load(config.bgPath);
    if (!bgLoaded) {
        ofLogWarning() << "Background image not found at "
                       << ofToDataPath(config.bgPath, true);
    }

    printSettings();
}

void ofApp::setupKeyShader() {
    const std::string vertex = R"(
#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
in vec2 texcoord;
out vec2 vTexCoord;
void main() {
    vTexCoord = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}
)";

    shaderReady = keyShader.setupShaderFromSource(GL_VERTEX_SHADER, vertex);
    shaderReady = shaderReady && keyShader.setupShaderFromSource(GL_FRAGMENT_SHADER, getKeyFragmentShaderSource());
    if (shaderReady) {
        keyShader.bindDefaults();
        shaderReady = keyShader.linkProgram();
    }

    if (!shaderReady) {
        ofLogWarning() << "Failed to compile keying shader.";
    }
}

void ofApp::update() {
    grabber.update();
    if (grabber.isFrameNew()) {
        updateMotion(grabber.getPixels());
        if (enableFaceDetect) {
            faceDetectFrame++;
            if (faceDetectInterval <= 0 || (faceDetectFrame % faceDetectInterval) == 0) {
                faceDetector.setScale(faceDetectScale);
                if (!faceDetector.detect(grabber.getPixels(), faceRects)) {
                    const std::string &err = faceDetector.getLastError();
                    if (!err.empty()) {
                        ofLogWarning() << "Face detect: " << err;
                    }
                }
            }
        }
        if (enableHandSparkles) {
            handDetectFrame++;
            if (handDetectInterval <= 0 || (handDetectFrame % handDetectInterval) == 0) {
                handDetector.setScale(handDetectScale);
                handDetector.setEnabledFingers(handSparkleFingers);
                if (!handDetector.detect(grabber.getPixels(), handPoints)) {
                    const std::string &err = handDetector.getLastError();
                    if (!err.empty()) {
                        ofLogWarning() << "Hand detect: " << err;
                    }
                }
            }
        }
        if (!useShaderKey) {
            updateComposite();
        }
    }

    midi.update();
    handleMidiControls();

    float dt = ofGetLastFrameTime();
    emitHandSparks(dt);
    updateSparkParticles(dt);
    updateTrail(dt);
}

void ofApp::draw() {
    ofClear(0);
    ofSetColor(255);

    if (bgLoaded) {
        drawTextureCover(bgImage.getTexture(), ofGetWidth(), ofGetHeight(), false);
    } else {
        ofSetColor(30);
        ofDrawRectangle(0, 0, ofGetWidth(), ofGetHeight());
        ofSetColor(255);
    }

    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    if (useShaderKey && shaderReady && grabber.isInitialized() && grabber.getTexture().isAllocated()) {
        keyShader.begin();
        keyShader.setUniformTexture("tex0", grabber.getTexture(), 0);
        keyShader.setUniform2f("texSize", grabber.getWidth(), grabber.getHeight());
        keyShader.setUniform1f("keyHue", keyHueDeg / 360.0f);
        keyShader.setUniform1f("keyHueRange", keyHueRangeDeg / 360.0f);
        keyShader.setUniform1f("keyMinSat", keyMinSat);
        keyShader.setUniform1f("keyMinVal", keyMinVal);
        keyShader.setUniform1f("levels", posterizeLevels);
        keyShader.setUniform1f("edgeStrength", edgeStrength);
        keyShader.setUniform1f("time", ofGetElapsedTimef());
        keyShader.setUniform1f("bpm", pulseBpm);
        keyShader.setUniform1f("pulseAmount", pulseAmount);
        keyShader.setUniform1f("pulseColorize", pulseColorize);
        keyShader.setUniform1f("pulseHueMode", static_cast<float>(pulseHueMode));
        keyShader.setUniform1f("pulseHueShift", pulseHueShiftDeg);
        keyShader.setUniform1f("pulseAttack", pulseAttack);
        keyShader.setUniform1f("pulseDecay", pulseDecay);
        keyShader.setUniform1f("pulseHueBoost", pulseHueBoost);
        keyShader.setUniform1f("wooferOn", enableWoofer ? 1.0f : 0.0f);
        keyShader.setUniform1f("wooferStrength", wooferStrength);
        keyShader.setUniform1f("wooferFalloff", wooferFalloff);
        keyShader.setUniform1f("satOn", enableSaturation ? 1.0f : 0.0f);
        keyShader.setUniform1f("satScale", saturationScale);
        keyShader.setUniform1f("kaleidoOn", enableKaleido ? 1.0f : 0.0f);
        keyShader.setUniform1f("kaleidoSegments", kaleidoSegments);
        keyShader.setUniform1f("kaleidoSpin", kaleidoSpin);
        keyShader.setUniform1f("kaleidoZoom", kaleidoZoom);
        keyShader.setUniform1f("halftoneOn", enableHalftone ? 1.0f : 0.0f);
        keyShader.setUniform1f("halftoneScale", halftoneScale);
        keyShader.setUniform1f("halftoneEdge", halftoneEdge);
        keyShader.setUniform1f("wetMix", wetMix);
        drawTextureCover(grabber.getTexture(), ofGetWidth(), ofGetHeight(), true);
        keyShader.end();
    } else if (compositeReady) {
        drawTextureCover(rgbaTexture, ofGetWidth(), ofGetHeight(), true);
    }

    if (enableHandSparkles) {
        drawTrail();
    }

    if (showHelpOverlay) {
        drawHelpOverlay();
    }

    if (showFaceDebug && !faceRects.empty() && grabber.isInitialized()) {
        ofPushStyle();
        ofNoFill();
        ofSetColor(0, 255, 255);
        ofSetLineWidth(2.0f);
        float camW = grabber.getWidth();
        float camH = grabber.getHeight();
        for (const auto &rect : faceRects) {
            ofVec2f tl = mapCameraToScreen({rect.x, rect.y}, camW, camH, true);
            ofVec2f br = mapCameraToScreen({rect.x + rect.width, rect.y + rect.height}, camW, camH, true);
            float x = std::min(tl.x, br.x);
            float y = std::min(tl.y, br.y);
            float w = std::abs(br.x - tl.x);
            float h = std::abs(br.y - tl.y);
            ofDrawRectangle(x, y, w, h);
        }
        ofPopStyle();
    }

    if (showHandDebug && !handPoints.empty() && grabber.isInitialized()) {
        ofPushStyle();
        ofSetColor(255, 0, 255);
        ofFill();
        float camW = grabber.getWidth();
        float camH = grabber.getHeight();
        for (const auto &pt : handPoints) {
            ofVec2f pos = mapCameraToScreen(pt.tip, camW, camH, true);
            ofDrawCircle(pos, 6.0f);
        }
        ofPopStyle();
    }

    float beatsPerSecond = pulseBpm / 60.0f;
    if (beatsPerSecond > 0.0f) {
        float beatTime = ofGetElapsedTimef() * beatsPerSecond;
        float beatPhase = beatTime - std::floor(beatTime);
        float flashBeats = beatFlashSeconds * beatsPerSecond;
        if (beatPhase < flashBeats) {
            int beatIndex = static_cast<int>(std::floor(beatTime)) % 4;
            float radius = (beatIndex == 0) ? beatDownbeatRadius : beatDotRadius;
            ofPushStyle();
            ofSetColor(0);
            ofDrawCircle(20.0f, 20.0f, radius);
            ofPopStyle();
        }
    }
}

void ofApp::keyPressed(ofKeyEventArgs &event) {
    int actionKey = event.key;
    int controlKey = actionKey;
    if (controlKey < 32 || controlKey > 126) {
        if (event.keycode >= 32 && event.keycode <= 126) {
            controlKey = event.keycode;
        }
    }

    bool shiftDown = event.hasModifier(OF_KEY_SHIFT);
    bool cmdDown = event.hasModifier(OF_KEY_COMMAND);
    bool altDown = event.hasModifier(OF_KEY_ALT);
    bool ctrlDown = event.hasModifier(OF_KEY_CONTROL);

    bool helpKey = (actionKey == '?' || controlKey == '?' || controlKey == '/');
    if (helpKey) {
        showHelpOverlay = !showHelpOverlay;
        return;
    }
    if (showHelpOverlay) {
        showHelpOverlay = false;
    }

    if (handleControlKey(controlKey, shiftDown, cmdDown, altDown, ctrlDown)) {
        printSettings();
        return;
    }

    keyPressed(actionKey);
}

void ofApp::keyPressed(int key) {
    static const std::array<int, 3> kWooferModes = {0, 1, 1};
    if (key == 'f') {
        ofToggleFullscreen();
    } else if (key == 'r') {
        resetBackgroundSubtractor();
        ofLogNotice() << "Background model reset.";
    } else if (key == '1') {
        useShaderKey = true;
        printSettings();
    } else if (key == '2') {
        useShaderKey = false;
        resetBackgroundSubtractor();
        printSettings();
    } else if (key == 'p') {
        midi.cyclePort();
    } else if (key == 'o') {
        midi.toggleOutputTest();
    } else if (key == '+') {
        maskThreshold = std::min(255, maskThreshold + 5);
        printSettings();
    } else if (key == '-') {
        maskThreshold = std::max(0, maskThreshold - 5);
        printSettings();
    } else if (key == 'e') {
        enableMorph = !enableMorph;
        printSettings();
    } else if (key == 'b') {
        wooferModeIndex = (wooferModeIndex + 1) % static_cast<int>(kWooferModes.size());
        enableWoofer = kWooferModes[static_cast<size_t>(wooferModeIndex)] != 0;
        printSettings();
    } else if (key == 's') {
        detectShadows = !detectShadows;
        resetBackgroundSubtractor();
        printSettings();
    } else if (key == '[') {
        if (!devices.empty()) {
            int nextIndex = currentDevice - 1;
            if (nextIndex < 0) {
                nextIndex = static_cast<int>(devices.size()) - 1;
            }
            startCamera(nextIndex);
        }
    } else if (key == ']') {
        if (!devices.empty()) {
            int nextIndex = currentDevice + 1;
            if (nextIndex >= static_cast<int>(devices.size())) {
                nextIndex = 0;
            }
            startCamera(nextIndex);
        }
    } else if (key == OF_KEY_ESC) {
        ofExit();
    }
}

void ofApp::keyReleased(int key) {
    (void)key;
}

void ofApp::exit() {
    if (grabber.isInitialized()) {
        grabber.close();
    }
    midi.close();
}

void ofApp::listCameras() {
    devices = grabber.listDevices();
    ofLogNotice() << "Available cameras:";
    for (size_t i = 0; i < devices.size(); ++i) {
        const auto &device = devices[i];
        ofLogNotice() << "  [" << i << "] " << device.deviceName
                      << (device.bAvailable ? "" : " (unavailable)");
    }
}

void ofApp::startCamera(int index) {
    if (devices.empty()) {
        return;
    }

    if (index < 0) {
        index = 0;
    } else if (index >= static_cast<int>(devices.size())) {
        index = static_cast<int>(devices.size()) - 1;
    }
    currentDevice = index;

    if (grabber.isInitialized()) {
        grabber.close();
    }

    grabber.setDeviceID(devices[currentDevice].id);
    grabber.setDesiredFrameRate(config.camFps);
    grabber.setPixelFormat(OF_PIXELS_RGB);

    if (!grabber.setup(config.camWidth, config.camHeight)) {
        ofLogWarning() << "Failed to start camera " << currentDevice;
    } else {
        ofLogNotice() << "Using camera [" << currentDevice << "] "
                      << devices[currentDevice].deviceName;
    }

    resetBackgroundSubtractor();
    compositeReady = false;
}

void ofApp::resetBackgroundSubtractor() {
    bgSub = cv::createBackgroundSubtractorMOG2();
    if (bgSub) {
        bgSub->setDetectShadows(detectShadows);
    }
    mask.release();
}

void ofApp::updateComposite() {
    if (!grabber.isInitialized()) {
        return;
    }

    ofPixels &camPixels = grabber.getPixels();
    if (!camPixels.isAllocated()) {
        return;
    }

    if (!bgSub) {
        resetBackgroundSubtractor();
    }

    cv::Mat frame;
    cv::Mat frameConverted;
    if (camPixels.getNumChannels() == 3) {
        frame = cv::Mat(camPixels.getHeight(),
                        camPixels.getWidth(),
                        CV_8UC3,
                        camPixels.getData(),
                        camPixels.getBytesStride());
    } else if (camPixels.getNumChannels() == 4) {
        cv::Mat rgba(camPixels.getHeight(),
                     camPixels.getWidth(),
                     CV_8UC4,
                     camPixels.getData(),
                     camPixels.getBytesStride());
        cv::cvtColor(rgba, frameConverted, cv::COLOR_RGBA2RGB);
        frame = frameConverted;
    } else {
        ofLogWarning() << "Unsupported camera pixel format ("
                       << camPixels.getNumChannels() << " channels).";
        return;
    }

    bgSub->apply(frame, mask);

    if (mask.empty()) {
        return;
    }

    cv::threshold(mask, mask, maskThreshold, 255, cv::THRESH_BINARY);

    if (enableMorph) {
        cv::erode(mask, mask, cv::Mat(), cv::Point(-1, -1), 1);
        cv::dilate(mask, mask, cv::Mat(), cv::Point(-1, -1), 2);
    }

    if (enableBlur) {
        cv::medianBlur(mask, mask, 5);
        cv::threshold(mask, mask, maskThreshold, 255, cv::THRESH_BINARY);
    }

    int w = camPixels.getWidth();
    int h = camPixels.getHeight();
    if (rgbaPixels.getWidth() != w || rgbaPixels.getHeight() != h) {
        rgbaPixels.allocate(w, h, OF_PIXELS_RGBA);
        rgbaTexture.allocate(w, h, GL_RGBA);
    }

    const unsigned char *srcBase = frame.data;
    size_t srcStep = frame.step;
    cv::Mat maskContinuous = mask;
    if (!maskContinuous.isContinuous()) {
        maskContinuous = mask.clone();
    }
    const unsigned char *maskPtr = maskContinuous.data;
    unsigned char *dst = rgbaPixels.getData();

    for (int y = 0; y < h; ++y) {
        const unsigned char *src = srcBase + (srcStep * y);
        const unsigned char *maskRow = maskPtr + (w * y);
        unsigned char *dstRow = dst + (w * 4 * y);
        for (int x = 0; x < w; ++x) {
            int srcIndex = x * 3;
            int dstIndex = x * 4;
            dstRow[dstIndex + 0] = src[srcIndex + 0];
            dstRow[dstIndex + 1] = src[srcIndex + 1];
            dstRow[dstIndex + 2] = src[srcIndex + 2];
            dstRow[dstIndex + 3] = maskRow[x];
        }
    }

    rgbaTexture.loadData(rgbaPixels);
    compositeReady = true;
}

void ofApp::drawTextureCover(ofTexture &tex, float dstW, float dstH, bool mirrorX) {
    if (mirrorX) {
        ofPushMatrix();
        ofTranslate(dstW, 0);
        ofScale(-1.0f, 1.0f);
    }

    float scale = std::max(dstW / tex.getWidth(), dstH / tex.getHeight());
    float drawW = tex.getWidth() * scale;
    float drawH = tex.getHeight() * scale;
    float x = (dstW - drawW) * 0.5f;
    float y = (dstH - drawH) * 0.5f;
    tex.draw(x, y, drawW, drawH);

    if (mirrorX) {
        ofPopMatrix();
    }
}

void ofApp::setupControls() {
    controls.clear();

    auto addControl = [&](ControlSpec control) {
        if (!control.presets.empty()) {
            control.value = control.presets[control.presetIndex % static_cast<int>(control.presets.size())];
        } else {
            control.value = ofLerp(control.knobMin, control.knobMax, 0.5f);
        }

        if (control.id == "saturation" && control.value < 0.0f) {
            control.enabled = false;
            control.value = 1.0f;
        } else if (control.hasOff) {
            control.enabled = control.value > 0.5f;
        } else {
            control.enabled = true;
        }

        controls.push_back(control);
        midi.registerControl(control.id);
    };

    addControl({
        "kaleido",
        'k',
        'K',
        std::vector<float>(kKaleidoModes.begin(), kKaleidoModes.end()),
        0.0f,
        16.0f,
        true,
        2
    });

    addControl({
        "kaleidoZoom",
        'z',
        'Z',
        std::vector<float>(kKaleidoZoomModes.begin(), kKaleidoZoomModes.end()),
        kaleidoZoomKnobMax,
        kaleidoZoomKnobMin,
        false,
        1
    });

    addControl({
        "halftone",
        'd',
        'D',
        std::vector<float>(kHalftoneModes.begin(), kHalftoneModes.end()),
        halftoneKnobMin,
        halftoneKnobMax,
        true,
        0
    });

    addControl({
        "tempo",
        't',
        'T',
        std::vector<float>(kTempoPresets.begin(), kTempoPresets.end()),
        60.0f,
        120.0f,
        false,
        0
    });

    addControl({
        "saturation",
        'v',
        'V',
        std::vector<float>(kSaturationModes.begin(), kSaturationModes.end()),
        0.0f,
        1.0f,
        true,
        0
    });

    addControl({
        "wetMix",
        'w',
        'W',
        std::vector<float>(kWetMixPresets.begin(), kWetMixPresets.end()),
        0.0f,
        1.0f,
        false,
        2
    });

    for (const auto &control : controls) {
        applyControl(control);
    }
}

void ofApp::handleMidiControls() {
    bool changed = false;
    float value01 = 0.0f;
    for (auto &control : controls) {
        bool muteActive = midi.isMuteActive(control.id);
        if (muteActive) {
            bool startedMute = false;
            if (!control.muteHeld) {
                control.muteHeld = true;
                control.preMuteValue = control.value;
                control.preMuteEnabled = control.enabled;
                startedMute = true;
            }
            float minVal = std::min(control.knobMin, control.knobMax);
            control.value = minVal;
            if (control.id == "saturation") {
                control.enabled = true;
            } else if (control.hasOff) {
                control.enabled = control.value > 0.5f;
            } else {
                control.enabled = true;
            }
            applyControl(control);
            if (startedMute) {
                changed = true;
            }
            continue;
        }

        if (control.muteHeld) {
            control.muteHeld = false;
            control.value = control.preMuteValue;
            control.enabled = control.preMuteEnabled;
            changed = true;
        }

        if (midi.consumeOscPadHit(control.id)) {
            control.oscEnabled = !control.oscEnabled;
            changed = true;
        }
        if (midi.consumeOscKnobValue(control.id, value01)) {
            control.oscSpeed01 = ofClamp(value01, 0.0f, 1.0f);
            changed = true;
        }

        if (midi.consumePadHit(control.id)) {
            cycleControlPreset(control);
            changed = true;
        }
        if (midi.consumeKnobValue(control.id, value01)) {
            float clamped = ofClamp(value01, 0.0f, 1.0f);
            control.value = ofLerp(control.knobMin, control.knobMax, clamped);
            if (control.id == "saturation") {
                control.enabled = true;
            } else if (control.hasOff) {
                float minVal = std::min(control.knobMin, control.knobMax);
                float maxVal = std::max(control.knobMin, control.knobMax);
                float eps = std::max(0.01f, (maxVal - minVal) * 0.01f);
                control.enabled = control.value > minVal + eps;
            } else {
                control.enabled = true;
            }
            changed = true;
        }
        applyControl(control);
    }

    if (changed) {
        printSettings();
    }
}

bool ofApp::handleControlKey(int key,
                             bool shiftDown,
                             bool cmdDown,
                             bool altDown,
                             bool ctrlDown) {
    {
        std::string label;
        if (cmdDown) {
            label += "Cmd+";
        }
        if (altDown) {
            label += "Opt+";
        }
        if (ctrlDown) {
            label += "Ctrl+";
        }
        if (shiftDown) {
            label += "Shift+";
        }
        std::string keyName;
        if (key >= 32 && key <= 126) {
            char c = static_cast<char>(key);
            if (std::isalpha(static_cast<unsigned char>(c))) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            keyName.assign(1, c);
        } else {
            keyName = "Key(" + ofToString(key) + ")";
        }
        ofLogNotice() << "Key debug: " << label << keyName;
    }
    for (auto &control : controls) {
        if (cmdDown && altDown && (key == control.key || key == control.learnKey)) {
            midi.beginLearnOsc(control.id);
            return true;
        }
        if (ctrlDown && shiftDown && (cmdDown || altDown) &&
            (key == control.key || key == control.learnKey)) {
            midi.beginLearnOsc(control.id);
            return true;
        }
        if (cmdDown && shiftDown && (key == control.key || key == control.learnKey)) {
            midi.beginLearnMute(control.id);
            return true;
        }
        if (shiftDown && (key == control.key || key == control.learnKey)) {
            midi.beginLearn(control.id);
            return true;
        }
    }

    for (auto &control : controls) {
        if (key == control.key) {
            cycleControlPreset(control);
            applyControl(control);
            return true;
        }
    }
    return false;
}

ofApp::ControlSpec *ofApp::findControlByKey(char key) {
    for (auto &control : controls) {
        if (control.key == key) {
            return &control;
        }
    }
    return nullptr;
}

ofApp::ControlSpec *ofApp::findControlById(const std::string &id) {
    for (auto &control : controls) {
        if (control.id == id) {
            return &control;
        }
    }
    return nullptr;
}

void ofApp::cycleControlPreset(ControlSpec &control) {
    if (control.presets.empty()) {
        return;
    }
    control.presetIndex = (control.presetIndex + 1) % static_cast<int>(control.presets.size());
    float value = control.presets[control.presetIndex];
    if (control.id == "saturation" && value < 0.0f) {
        control.enabled = false;
        control.value = 1.0f;
        return;
    }
    control.value = value;
    if (control.hasOff) {
        control.enabled = control.value > 0.5f;
    } else {
        control.enabled = true;
    }
}

void ofApp::applyControl(const ControlSpec &control) {
    float value = resolveControlValue(control);
    if (control.id == "kaleido") {
        kaleidoSegments = value;
        enableKaleido = control.enabled;
        float minVal = std::min(control.knobMin, control.knobMax);
        float maxVal = std::max(control.knobMin, control.knobMax);
        float eps = std::max(0.01f, (maxVal - minVal) * 0.01f);
        int newState = 0;
        if (value <= minVal + eps) {
            newState = -1;
        } else if (value >= maxVal - eps) {
            newState = 1;
        }
        if (newState != 0 && newState != kaleidoExtremeState) {
            kaleidoSpinFlip = !kaleidoSpinFlip;
            kaleidoSpin = kaleidoSpinBase * (kaleidoSpinFlip ? -1.0f : 1.0f);
        }
        kaleidoExtremeState = newState;
        return;
    }
    if (control.id == "kaleidoZoom") {
        kaleidoZoom = value;
        return;
    }
    if (control.id == "halftone") {
        halftoneScale = value;
        enableHalftone = control.enabled;
        return;
    }
    if (control.id == "tempo") {
        pulseBpm = value;
        return;
    }
    if (control.id == "saturation") {
        saturationScale = value;
        enableSaturation = control.enabled;
        return;
    }
    if (control.id == "wetMix") {
        wetMix = value;
        return;
    }
}

float ofApp::resolveControlValue(const ControlSpec &control) const {
    if (!control.oscEnabled) {
        return control.value;
    }

    float bpm = pulseBpm;
    if (bpm <= 0.0f) {
        return control.value;
    }

    float midiValue = control.oscSpeed01 * 127.0f;
    if (midiValue < 1.0f) {
        return control.value;
    }

    float t = ofClamp((midiValue - 1.0f) / 126.0f, 0.0f, 1.0f);
    float beatsPerCycle = ofLerp(16.0f, 1.0f, t);
    if (beatsPerCycle <= 0.0f) {
        return control.value;
    }

    float beatTime = ofGetElapsedTimef() * (bpm / 60.0f);
    float phase = std::fmod(beatTime / beatsPerCycle, 1.0f);
    float lfo = 0.5f - 0.5f * std::cos(phase * TWO_PI);
    return ofLerp(control.knobMin, control.knobMax, lfo);
}

void ofApp::updateMotion(const ofPixels &camPixels) {
    if (!camPixels.isAllocated()) {
        return;
    }

    cv::Mat frame;
    cv::Mat frameConverted;
    if (camPixels.getNumChannels() == 3) {
        frame = cv::Mat(camPixels.getHeight(),
                        camPixels.getWidth(),
                        CV_8UC3,
                        const_cast<unsigned char *>(camPixels.getData()),
                        camPixels.getBytesStride());
    } else if (camPixels.getNumChannels() == 4) {
        cv::Mat rgba(camPixels.getHeight(),
                     camPixels.getWidth(),
                     CV_8UC4,
                     const_cast<unsigned char *>(camPixels.getData()),
                     camPixels.getBytesStride());
        cv::cvtColor(rgba, frameConverted, cv::COLOR_RGBA2RGB);
        frame = frameConverted;
    } else {
        return;
    }

    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_RGB2GRAY);

    if (prevGray.empty() || prevGray.size() != gray.size()) {
        prevGray = gray.clone();
        motionLevel = 0.0f;
        int px = camPixels.getWidth() / 2;
        int py = camPixels.getHeight() / 2;
        ofColor sample = camPixels.getColor(px, py);
        float hue = sample.getHue() / 255.0f;
        float sat = ofClamp((sample.getSaturation() / 255.0f) * 1.2f, 0.6f, 1.0f);
        float bri = ofClamp((sample.getBrightness() / 255.0f) * 1.2f, 0.6f, 1.0f);
        motionColor = ofFloatColor::fromHsb(hue, sat, bri, 1.0f);
        return;
    }

    cv::Mat diff;
    cv::absdiff(gray, prevGray, diff);
    cv::Scalar meanDiff = cv::mean(diff);
    motionLevel = static_cast<float>(meanDiff[0] / 255.0);

    int px = camPixels.getWidth() / 2;
    int py = camPixels.getHeight() / 2;
    ofColor sample = camPixels.getColor(px, py);
    float hue = sample.getHue() / 255.0f;
    float sat = ofClamp((sample.getSaturation() / 255.0f) * 1.2f, 0.6f, 1.0f);
    float bri = ofClamp((sample.getBrightness() / 255.0f) * 1.2f, 0.6f, 1.0f);
    motionColor = ofFloatColor::fromHsb(hue, sat, bri, 1.0f);

    prevGray = gray.clone();
}

void ofApp::updateTrail(float dt) {
    if (!enableHandSparkles) {
        return;
    }

    int width = ofGetWidth();
    int height = ofGetHeight();
    if (width <= 0 || height <= 0) {
        return;
    }

    if (!trailFbo.isAllocated() ||
        static_cast<int>(trailFbo.getWidth()) != width ||
        static_cast<int>(trailFbo.getHeight()) != height) {
        trailFbo.allocate(width, height, GL_RGBA);
        trailFbo.begin();
        ofClear(0, 0, 0, 0);
        trailFbo.end();
    }

    trailFbo.begin();
    ofPushStyle();
    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofSetColor(0, 0, 0, static_cast<int>(trailFade * 255.0f));
    ofDrawRectangle(0, 0, width, height);

    if (enableHandSparkles && !sparkParticles.empty()) {
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        for (const auto &particle : sparkParticles) {
            float t = ofClamp(1.0f - (particle.age / particle.life), 0.0f, 1.0f);
            float alpha = t * t * handSparkleOpacity;
            ofFloatColor c = particle.color;
            c.a = alpha;
            ofSetColor(c);
            float size = particle.size * (0.5f + 0.5f * t);
            ofDrawCircle(particle.pos, size);
            ofSetLineWidth(std::max(1.0f, size * 0.4f));
            ofDrawLine(particle.prev, particle.pos);
        }
    }

    ofPopStyle();
    trailFbo.end();
}

void ofApp::emitHandSparks(float dt) {
    if (!enableHandSparkles || handPoints.empty() || !grabber.isInitialized()) {
        return;
    }

    float camW = grabber.getWidth();
    float camH = grabber.getHeight();
    float sizeScale = handSparkleSize / 18.0f;

    for (const auto &hand : handPoints) {
        ofVec2f tipScreen = mapCameraToScreen(hand.tip, camW, camH, true);
        ofVec2f baseScreen = mapCameraToScreen(hand.tip - hand.dir, camW, camH, true);
        ofVec2f dir = tipScreen - baseScreen;
        if (dir.lengthSquared() < 4.0f) {
            continue;
        }
        dir.normalize();

        float emit = sparkEmitRate * dt;
        int count = static_cast<int>(emit);
        if (ofRandom(1.0f) < (emit - static_cast<float>(count))) {
            count += 1;
        }

        for (int i = 0; i < count; ++i) {
            if (sparkParticles.size() >= static_cast<size_t>(maxSparkParticles)) {
                sparkParticles.erase(sparkParticles.begin());
            }

            float angle = std::atan2(dir.y, dir.x) + ofRandom(-sparkSpread, sparkSpread);
            float speed = sparkSpeed * ofRandom(0.4f, 1.0f);
            ofVec2f vel(std::cos(angle), std::sin(angle));
            vel *= speed;
            vel += ofVec2f(ofRandom(-sparkJitter, sparkJitter),
                           ofRandom(-sparkJitter, sparkJitter)) * 0.1f;

            ofFloatColor c = motionColor;
            c = c.getLerped(ofFloatColor(1.0f, 0.8f, 0.4f), 0.4f);
            float brightness = ofRandom(0.6f, 1.0f);
            c.r *= brightness;
            c.g *= brightness;
            c.b *= brightness;

            SparkParticle particle;
            particle.pos = tipScreen;
            particle.prev = tipScreen;
            particle.vel = vel;
            particle.color = c;
            particle.life = sparkLife * ofRandom(0.6f, 1.2f);
            particle.size = ofRandom(1.5f, 4.5f) * sizeScale;
            sparkParticles.push_back(particle);
        }
    }
}

void ofApp::updateSparkParticles(float dt) {
    if (sparkParticles.empty()) {
        return;
    }

    float drag = std::pow(sparkDrag, dt * 60.0f);
    for (auto &particle : sparkParticles) {
        particle.prev = particle.pos;
        particle.age += dt;
        particle.vel *= drag;
        particle.vel.y += sparkGravity * dt;
        particle.pos += particle.vel * dt;
    }

    sparkParticles.erase(std::remove_if(sparkParticles.begin(),
                                        sparkParticles.end(),
                                        [](const SparkParticle &p) {
                                            return p.age >= p.life;
                                        }),
                         sparkParticles.end());
}

void ofApp::drawTrail() {
    if (!trailFbo.isAllocated()) {
        return;
    }

    ofPushStyle();
    ofEnableBlendMode(OF_BLENDMODE_ADD);
    ofSetColor(255);
    trailFbo.draw(0, 0);
    ofPopStyle();
}

ofVec2f ofApp::mapCameraToScreen(const ofVec2f &camPos, float camW, float camH, bool mirrorX) {
    float dstW = ofGetWidth();
    float dstH = ofGetHeight();
    float scale = std::max(dstW / camW, dstH / camH);
    float drawW = camW * scale;
    float drawH = camH * scale;
    float xOffset = (dstW - drawW) * 0.5f;
    float yOffset = (dstH - drawH) * 0.5f;
    float screenX = xOffset + camPos.x * scale;
    float screenY = yOffset + camPos.y * scale;
    if (mirrorX) {
        screenX = dstW - screenX;
    }
    return {screenX, screenY};
}

void ofApp::printSettings() {
    ofLogNotice() << "Settings: mode=" << (useShaderKey ? "shader-key" : "bg-sub");
    if (useShaderKey) {
        ofLogNotice() << "Key: hue=" << keyHueDeg
                      << " range=" << keyHueRangeDeg
                      << " minSat=" << keyMinSat
                      << " minVal=" << keyMinVal
                      << " posterize=" << posterizeLevels
                      << " edge=" << edgeStrength
                      << " sat=" << (enableSaturation ? "on" : "off")
                      << " satScale=" << saturationScale
                      << " kaleido=" << (enableKaleido ? "on" : "off")
                      << " segments=" << kaleidoSegments
                      << " spin=" << kaleidoSpin
                      << " zoom=" << kaleidoZoom
                      << " halftone=" << (enableHalftone ? "on" : "off")
                      << " dots=" << halftoneScale
                      << " wet=" << wetMix;
    } else {
        ofLogNotice() << "BG: threshold=" << maskThreshold
                      << " morph=" << (enableMorph ? "on" : "off")
                      << " blur=" << (enableBlur ? "on" : "off")
                      << " shadows=" << (detectShadows ? "on" : "off");
    }

    ofLogNotice() << "PulseHue: mode=" << pulseHueMode
                  << " shift=" << pulseHueShiftDeg
                  << " bpm=" << pulseBpm;
    ofLogNotice() << "Woofer: " << (enableWoofer ? "on" : "off")
                  << " strength=" << wooferStrength
                  << " falloff=" << wooferFalloff;
    ofLogNotice() << "Sparkles: " << (enableHandSparkles ? "on" : "off")
                  << " particles=" << sparkParticles.size()
                  << " motion=" << motionLevel;
}

void ofApp::drawHelpOverlay() {
    std::vector<std::string> lines = {
        "Help / Controls (? to hide)",
        "",
        "Modes:",
        "  1  Shader key mode",
        "  2  Background subtractor (MOG2)",
        "",
        "Effects:",
        "  k  Kaleidoscope modes",
        "  z  Kaleido zoom",
        "  d  Halftone dots",
        "  v  Saturation",
        "  t  Tempo",
        "  w  Wet mix",
        "  b  Woofer distortion",
        "",
        "System:",
        "  f  Fullscreen",
        "  r  Reset background model",
        "  p  Cycle MIDI input ports",
        "  o  MIDI test output",
        "  + / -  Mask threshold (bg-sub)",
        "  e  Morph (bg-sub)",
        "  s  Shadow detection (bg-sub)",
        "  [ / ]  Camera prev/next",
        "  Esc  Quit",
        "",
        "MIDI learn:",
        "  Shift+[key]   learn pad/knob",
        "  Cmd+Shift+[key]   learn mute pad (hold to min)",
        "  Cmd+Opt+[key] or Ctrl+Shift+Cmd/Opt+[key]",
        "    learn oscillator (pad toggles, knob speed)",
        "",
        "Vision:",
        "  Face detect (cyan boxes)",
        "  Hand sparkles (directional sparks)",
    };

    float w = ofGetWidth();
    float h = ofGetHeight();
    float boxW = w * 0.86f;
    float boxH = h * 0.86f;
    float x = (w - boxW) * 0.5f;
    float y = (h - boxH) * 0.5f;
    float padding = 32.0f;
    float lineHeight = 30.0f;

    ofPushStyle();
    ofSetColor(0, 0, 0, 200);
    ofDrawRectangle(x, y, boxW, boxH);
    ofSetColor(255);

    float textX = x + padding;
    float textY = y + padding + lineHeight;
    for (const auto &line : lines) {
        if (helpFont.isLoaded()) {
            helpFont.drawString(line, textX, textY);
        } else {
            ofDrawBitmapString(line, textX, textY);
        }
        textY += lineHeight;
        if (textY > y + boxH - padding) {
            break;
        }
    }
    ofPopStyle();
}
