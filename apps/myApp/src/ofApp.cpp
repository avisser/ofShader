#include "ofApp.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr std::array<float, 6> kKaleidoModes = {0.0f, 4.0f, 6.0f, 8.0f, 10.0f, 12.0f};
constexpr std::array<float, 4> kHalftoneModes = {0.0f, 10.0f, 14.0f, 22.0f};
constexpr std::array<float, 5> kSaturationModes = {-1.0f, 0.2f, 0.45f, 0.7f, 0.9f};
}

ofApp::ofApp(const AppConfig &config)
: config(config) {}

void ofApp::setup() {
    ofSetVerticalSync(true);
    ofSetFrameRate(config.camFps);
    ofSetFullscreen(true);
    setupKeyShader();
    midi.setup();

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

    const std::string fragment = R"(
#version 150
uniform sampler2DRect tex0;
uniform float keyHue;
uniform float keyHueRange;
uniform float keyMinSat;
uniform float keyMinVal;
uniform float levels;
uniform float edgeStrength;
uniform float time;
uniform float bpm;
uniform float pulseAmount;
uniform float pulseColorize;
uniform float pulseHueMode;
uniform float pulseHueShift;
uniform float pulseAttack;
uniform float pulseDecay;
uniform float pulseHueBoost;
uniform float wooferOn;
uniform float wooferStrength;
uniform float wooferFalloff;
uniform float satOn;
uniform float satScale;
uniform float kaleidoOn;
uniform float kaleidoSegments;
uniform float kaleidoSpin;
uniform vec2 texSize;
uniform float halftoneOn;
uniform float halftoneScale;
uniform float halftoneEdge;

in vec2 vTexCoord;
out vec4 outputColor;

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
    float d = q.x - min(q.w, q.y);
    float e = 1e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)),
                d / (q.x + e),
                q.x);
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

float luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    float phase = fract(time * (bpm / 60.0));
    float attack = max(0.001, pulseAttack);
    float decay = max(0.001, pulseDecay);
    float ramp = smoothstep(0.0, attack, phase);
    float fall = (phase <= attack) ? 1.0 : exp(-(phase - attack) * decay);
    float kick = ramp * fall;
    float boostedKick = min(1.0, kick * pulseHueBoost);
    float pulse = 1.0 + (pulseAmount * boostedKick);

    vec2 coord = vTexCoord;
    if (kaleidoOn > 0.5) {
        vec2 center = texSize * 0.5;
        vec2 p = coord - center;
        float r = length(p);
        float angle = atan(p.y, p.x) + kaleidoSpin * time;
        float segments = max(1.0, kaleidoSegments);
        float sector = 6.2831853 / segments;
        angle = mod(angle, sector);
        angle = abs(angle - sector * 0.5);
        vec2 newP = vec2(cos(angle), sin(angle)) * r;
        coord = newP + center;
    }

    if (wooferOn > 0.5) {
        vec2 center = texSize * 0.5;
        vec2 p = coord - center;
        float maxR = max(1.0, min(texSize.x, texSize.y) * 0.5);
        float rNorm = length(p) / maxR;
        float falloff = pow(clamp(1.0 - rNorm, 0.0, 1.0), wooferFalloff);
        float bulge = 1.0 + (wooferStrength * boostedKick * falloff);
        coord = center + (p * bulge);
    }

    coord = clamp(coord, vec2(0.0), texSize - 1.0);

    vec3 rgb = texture(tex0, coord).rgb;
    float dotMask = 1.0;
    if (halftoneOn > 0.5) {
        float cell = max(2.0, halftoneScale);
        vec2 cellSize = vec2(cell);
        vec2 cellCenter = (floor(coord / cellSize) + 0.5) * cellSize;
        vec2 offset = coord - cellCenter;
        float lum = luma(rgb);
        float radius = (1.0 - lum) * 0.5 * cell;
        float edge = max(0.001, radius * halftoneEdge);
        float dist = length(offset);
        dotMask = 1.0 - smoothstep(radius - edge, radius + edge, dist);
    }
    vec3 hsv = rgb2hsv(rgb);

    float hueDist = abs(hsv.x - keyHue);
    hueDist = min(hueDist, 1.0 - hueDist);

    float hueOk = 1.0 - smoothstep(keyHueRange, keyHueRange + 0.02, hueDist);
    float satOk = smoothstep(keyMinSat, keyMinSat + 0.05, hsv.y);
    float valOk = smoothstep(keyMinVal, keyMinVal + 0.05, hsv.z);

    float keyMask = hueOk * satOk * valOk;
    float alpha = 1.0 - keyMask;

    float safeLevels = max(levels, 2.0);
    vec3 poster = floor(rgb * safeLevels) / (safeLevels - 1.0);

    float lumC = luma(rgb);
    float lumR = luma(texture(tex0, coord + vec2(1.0, 0.0)).rgb);
    float lumU = luma(texture(tex0, coord + vec2(0.0, 1.0)).rgb);
    float edge = abs(lumC - lumR) + abs(lumC - lumU);

    vec3 color = mix(rgb, poster, 0.85);
    color += edgeStrength * edge;
    color *= pulse;
    color = mix(color, color * vec3(1.1, 0.85, 1.2), pulseColorize * boostedKick);
    if (abs(pulseHueMode) > 0.5) {
        vec3 hsvOut = rgb2hsv(color);
        float shift = (pulseHueShift / 360.0) * boostedKick;
        if (pulseHueMode > 0.0) {
            hsvOut.x = fract(hsvOut.x - shift);
        } else {
            hsvOut.x = fract(hsvOut.x + shift);
        }
        color = hsv2rgb(hsvOut);
    }
    if (satOn > 0.5) {
        vec3 hsvSat = rgb2hsv(color);
        hsvSat.y = clamp(hsvSat.y * satScale, 0.0, 1.0);
        color = hsv2rgb(hsvSat);
    }
    color = clamp(color, 0.0, 1.0);

    color *= dotMask;
    outputColor = vec4(color * alpha, alpha * dotMask);
}
)";

    shaderReady = keyShader.setupShaderFromSource(GL_VERTEX_SHADER, vertex);
    shaderReady = shaderReady && keyShader.setupShaderFromSource(GL_FRAGMENT_SHADER, fragment);
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
        if (!useShaderKey) {
            updateComposite();
        }
    }

    midi.update();
    if (midi.consumeKaleidoPadHit()) {
        cycleKaleidoMode();
        printSettings();
    }
    if (midi.hasKaleidoKnobBinding()) {
        kaleidoSegments = ofClamp(midi.getKaleidoKnobValue01(), 0.0f, 1.0f) * 16.0f;
        enableKaleido = kaleidoSegments > 0.5f;
    }
    if (midi.consumeHalftonePadHit()) {
        halftoneModeIndex = (halftoneModeIndex + 1) % static_cast<int>(kHalftoneModes.size());
        float nextScale = kHalftoneModes[static_cast<size_t>(halftoneModeIndex)];
        enableHalftone = nextScale > 0.5f;
        if (enableHalftone) {
            halftoneScale = nextScale;
        }
        printSettings();
    }
    if (midi.hasHalftoneKnobBinding()) {
        float value01 = ofClamp(midi.getHalftoneKnobValue01(), 0.0f, 1.0f);
        halftoneScale = ofLerp(halftoneKnobMin, halftoneKnobMax, value01);
        enableHalftone = halftoneScale > 0.5f;
    }
    if (midi.consumeSaturationPadHit()) {
        saturationModeIndex = (saturationModeIndex + 1) % static_cast<int>(kSaturationModes.size());
        float mode = kSaturationModes[static_cast<size_t>(saturationModeIndex)];
        if (mode < 0.0f) {
            enableSaturation = false;
            saturationScale = 1.0f;
        } else {
            enableSaturation = true;
            saturationScale = mode;
        }
        printSettings();
    }
    if (midi.hasSaturationKnobBinding()) {
        float value01 = ofClamp(midi.getSaturationKnobValue01(), 0.0f, 1.0f);
        enableSaturation = true;
        saturationScale = value01;
    }

    updateTrail(ofGetLastFrameTime());
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
        keyShader.setUniform1f("halftoneOn", enableHalftone ? 1.0f : 0.0f);
        keyShader.setUniform1f("halftoneScale", halftoneScale);
        keyShader.setUniform1f("halftoneEdge", halftoneEdge);
        drawTextureCover(grabber.getTexture(), ofGetWidth(), ofGetHeight(), true);
        keyShader.end();
    } else if (compositeReady) {
        drawTextureCover(rgbaTexture, ofGetWidth(), ofGetHeight(), true);
    }

    if (enableTrail) {
        drawTrail();
    }
}

void ofApp::keyPressed(int key) {
    static const std::array<int, 3> kWooferModes = {0, 1, 1};
    if (key == 'K' || (key == 'k' && ofGetKeyPressed(OF_KEY_SHIFT))) {
        midi.beginLearnKaleido();
        return;
    }
    if (key == 'D' || (key == 'd' && ofGetKeyPressed(OF_KEY_SHIFT))) {
        midi.beginLearnHalftone();
        return;
    }
    if (key == 'V' || (key == 'v' && ofGetKeyPressed(OF_KEY_SHIFT))) {
        midi.beginLearnSaturation();
        return;
    }
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
    } else if (key == 'k') {
        cycleKaleidoMode();
        printSettings();
    } else if (key == 'c') {
        enableTrail = !enableTrail;
        hasTrailPos = false;
        if (!enableTrail && trailFbo.isAllocated()) {
            trailFbo.begin();
            ofClear(0, 0, 0, 0);
            trailFbo.end();
        }
        printSettings();
    } else if (key == 'd') {
        halftoneModeIndex = (halftoneModeIndex + 1) % static_cast<int>(kHalftoneModes.size());
        float nextScale = kHalftoneModes[static_cast<size_t>(halftoneModeIndex)];
        enableHalftone = nextScale > 0.5f;
        if (enableHalftone) {
            halftoneScale = nextScale;
        }
        printSettings();
    } else if (key == 'v') {
        saturationModeIndex = (saturationModeIndex + 1) % static_cast<int>(kSaturationModes.size());
        float mode = kSaturationModes[static_cast<size_t>(saturationModeIndex)];
        if (mode < 0.0f) {
            enableSaturation = false;
            saturationScale = 1.0f;
        } else {
            enableSaturation = true;
            saturationScale = mode;
        }
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

void ofApp::cycleKaleidoMode() {
    kaleidoModeIndex = (kaleidoModeIndex + 1) % static_cast<int>(kKaleidoModes.size());
    kaleidoSegments = kKaleidoModes[static_cast<size_t>(kaleidoModeIndex)];
    enableKaleido = kaleidoSegments > 0.5f;
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
        motionCamW = static_cast<float>(gray.cols);
        motionCamH = static_cast<float>(gray.rows);
        prevGray = gray.clone();
        motionLevel = 0.0f;
        motionCenter = {gray.cols * 0.5f, gray.rows * 0.5f};
        int px = ofClamp(static_cast<int>(motionCenter.x), 0, camPixels.getWidth() - 1);
        int py = ofClamp(static_cast<int>(motionCenter.y), 0, camPixels.getHeight() - 1);
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

    cv::Mat thresh;
    cv::threshold(diff, thresh, 25, 255, cv::THRESH_BINARY);
    cv::Moments m = cv::moments(thresh, true);
    if (m.m00 > 0.0) {
        motionCenter = {static_cast<float>(m.m10 / m.m00),
                        static_cast<float>(m.m01 / m.m00)};
    }

    motionCamW = static_cast<float>(gray.cols);
    motionCamH = static_cast<float>(gray.rows);

    int px = ofClamp(static_cast<int>(motionCenter.x), 0, camPixels.getWidth() - 1);
    int py = ofClamp(static_cast<int>(motionCenter.y), 0, camPixels.getHeight() - 1);
    ofColor sample = camPixels.getColor(px, py);
    float hue = sample.getHue() / 255.0f;
    float sat = ofClamp((sample.getSaturation() / 255.0f) * 1.2f, 0.6f, 1.0f);
    float bri = ofClamp((sample.getBrightness() / 255.0f) * 1.2f, 0.6f, 1.0f);
    motionColor = ofFloatColor::fromHsb(hue, sat, bri, 1.0f);

    prevGray = gray.clone();
}

void ofApp::updateTrail(float dt) {
    if (!enableTrail) {
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

    if (motionLevel > motionThreshold && motionCamW > 0.0f && motionCamH > 0.0f) {
        float intensity = ofClamp((motionLevel - motionThreshold) * 12.0f, 0.0f, 1.0f);
        ofVec2f pos = mapCameraToScreen(motionCenter, motionCamW, motionCamH, true);
        ofFloatColor c = motionColor;
        c.a = trailOpacity * intensity;
        ofEnableBlendMode(OF_BLENDMODE_ADD);
        ofSetColor(c);
        float radius = trailSize * (0.7f + 1.4f * intensity);
        if (hasTrailPos) {
            ofSetLineWidth(std::max(1.0f, radius * 0.35f));
            ofDrawLine(lastTrailPos, pos);
        }
        ofDrawCircle(pos, radius);
        lastTrailPos = pos;
        hasTrailPos = true;
    } else {
        hasTrailPos = false;
    }

    ofPopStyle();
    trailFbo.end();
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
                      << " halftone=" << (enableHalftone ? "on" : "off")
                      << " dots=" << halftoneScale;
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
    ofLogNotice() << "Trail: " << (enableTrail ? "on" : "off")
                  << " fade=" << trailFade
                  << " size=" << trailSize
                  << " motion=" << motionLevel;
}
