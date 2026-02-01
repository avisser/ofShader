#include "ofApp.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>

ofApp::ofApp(const AppConfig &config)
: config(config) {}

void ofApp::setup() {
    ofSetVerticalSync(true);
    ofSetFrameRate(config.camFps);
    ofSetFullscreen(true);
    setupKeyShader();

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

float luma(vec3 c) {
    return dot(c, vec3(0.299, 0.587, 0.114));
}

void main() {
    vec3 rgb = texture(tex0, vTexCoord).rgb;
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
    float lumR = luma(texture(tex0, vTexCoord + vec2(1.0, 0.0)).rgb);
    float lumU = luma(texture(tex0, vTexCoord + vec2(0.0, 1.0)).rgb);
    float edge = abs(lumC - lumR) + abs(lumC - lumU);

    vec3 color = mix(rgb, poster, 0.85);
    color += edgeStrength * edge;
    color = clamp(color, 0.0, 1.0);

    outputColor = vec4(color * alpha, alpha);
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
        if (!useShaderKey) {
            updateComposite();
        }
    }
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
        keyShader.setUniform1f("keyHue", keyHueDeg / 360.0f);
        keyShader.setUniform1f("keyHueRange", keyHueRangeDeg / 360.0f);
        keyShader.setUniform1f("keyMinSat", keyMinSat);
        keyShader.setUniform1f("keyMinVal", keyMinVal);
        keyShader.setUniform1f("levels", posterizeLevels);
        keyShader.setUniform1f("edgeStrength", edgeStrength);
        drawTextureCover(grabber.getTexture(), ofGetWidth(), ofGetHeight(), true);
        keyShader.end();
    } else if (compositeReady) {
        drawTextureCover(rgbaTexture, ofGetWidth(), ofGetHeight(), true);
    }
}

void ofApp::keyPressed(int key) {
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
        enableBlur = !enableBlur;
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

void ofApp::printSettings() {
    ofLogNotice() << "Settings: mode=" << (useShaderKey ? "shader-key" : "bg-sub");
    if (useShaderKey) {
        ofLogNotice() << "Key: hue=" << keyHueDeg
                      << " range=" << keyHueRangeDeg
                      << " minSat=" << keyMinSat
                      << " minVal=" << keyMinVal
                      << " posterize=" << posterizeLevels
                      << " edge=" << edgeStrength;
    } else {
        ofLogNotice() << "BG: threshold=" << maskThreshold
                      << " morph=" << (enableMorph ? "on" : "off")
                      << " blur=" << (enableBlur ? "on" : "off")
                      << " shadows=" << (detectShadows ? "on" : "off");
    }
}
