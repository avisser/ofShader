#include "VisionHandPoseDetector.h"

#import <Vision/Vision.h>
#import <CoreML/CoreML.h>
#import <CoreVideo/CoreVideo.h>

#include <opencv2/imgproc.hpp>

#include <algorithm>

bool VisionHandPoseDetector::setup(float scale, float minConfidence, int maxHands) {
    setScale(scale);
    setMinConfidence(minConfidence);
    setMaxHands(maxHands);
    return true;
}

void VisionHandPoseDetector::setScale(float scale) {
    this->scale = std::max(0.1f, std::min(scale, 1.0f));
}

void VisionHandPoseDetector::setMinConfidence(float minConfidence) {
    this->minConfidence = std::max(0.0f, std::min(minConfidence, 1.0f));
}

void VisionHandPoseDetector::setMaxHands(int maxHands) {
    this->maxHands = std::max(1, std::min(maxHands, 4));
}

void VisionHandPoseDetector::setFingerEnabled(Finger finger, bool enabled) {
    size_t index = static_cast<size_t>(finger);
    if (index >= fingerEnabled.size()) {
        return;
    }
    fingerEnabled[index] = enabled;
}

void VisionHandPoseDetector::setEnabledFingers(const std::array<bool, 5> &enabled) {
    fingerEnabled = enabled;
}

bool VisionHandPoseDetector::detect(const ofPixels &pixels, std::vector<HandPoint> &outPoints) {
    outPoints.clear();
    lastError.clear();

    if (!pixels.isAllocated()) {
        lastError = "pixels not allocated";
        return false;
    }

    int width = pixels.getWidth();
    int height = pixels.getHeight();
    int channels = pixels.getNumChannels();
    if (width <= 0 || height <= 0) {
        lastError = "invalid pixel size";
        return false;
    }

    if (channels != 3 && channels != 4) {
        lastError = "unsupported pixel format";
        return false;
    }

    int targetW = std::max(64, static_cast<int>(width * scale));
    int targetH = std::max(64, static_cast<int>(height * scale));

    cv::Mat src;
    if (channels == 3) {
        src = cv::Mat(height, width, CV_8UC3,
                      const_cast<unsigned char *>(pixels.getData()),
                      pixels.getBytesStride());
    } else {
        src = cv::Mat(height, width, CV_8UC4,
                      const_cast<unsigned char *>(pixels.getData()),
                      pixels.getBytesStride());
    }

    cv::Mat resized;
    cv::resize(src, resized, cv::Size(targetW, targetH), 0, 0, cv::INTER_LINEAR);

    cv::Mat bgra;
    if (channels == 3) {
        cv::cvtColor(resized, bgra, cv::COLOR_RGB2BGRA);
    } else {
        cv::cvtColor(resized, bgra, cv::COLOR_RGBA2BGRA);
    }

    if (!bgra.isContinuous()) {
        bgra = bgra.clone();
    }

    CVPixelBufferRef buffer = nullptr;
    CVReturn status = CVPixelBufferCreateWithBytes(kCFAllocatorDefault,
                                                  bgra.cols,
                                                  bgra.rows,
                                                  kCVPixelFormatType_32BGRA,
                                                  bgra.data,
                                                  static_cast<size_t>(bgra.step),
                                                  nullptr,
                                                  nullptr,
                                                  nullptr,
                                                  &buffer);
    if (status != kCVReturnSuccess || buffer == nullptr) {
        lastError = "failed to create CVPixelBuffer";
        return false;
    }

    bool success = true;
    @autoreleasepool {
        VNDetectHumanHandPoseRequest *request = [[VNDetectHumanHandPoseRequest alloc] init];
        request.maximumHandCount = maxHands;
        VNImageRequestHandler *handler =
            [[VNImageRequestHandler alloc] initWithCVPixelBuffer:buffer options:@{}];
        NSError *error = nil;
        [handler performRequests:@[request] error:&error];
        if (error) {
            lastError = [[error localizedDescription] UTF8String];
            success = false;
        } else {
            NSArray<VNHumanHandPoseObservation *> *results = request.results;
            float scaleX = static_cast<float>(width) / static_cast<float>(bgra.cols);
            float scaleY = static_cast<float>(height) / static_cast<float>(bgra.rows);
            static NSArray<NSString *> *kTipKeys = nil;
            static NSArray<NSString *> *kBaseKeys = nil;
            static dispatch_once_t onceToken;
            dispatch_once(&onceToken, ^{
                kTipKeys = @[
                    VNHumanHandPoseObservationJointNameThumbTip,
                    VNHumanHandPoseObservationJointNameIndexTip,
                    VNHumanHandPoseObservationJointNameMiddleTip,
                    VNHumanHandPoseObservationJointNameRingTip,
                    VNHumanHandPoseObservationJointNameLittleTip
                ];
                kBaseKeys = @[
                    VNHumanHandPoseObservationJointNameThumbIP,
                    VNHumanHandPoseObservationJointNameIndexDIP,
                    VNHumanHandPoseObservationJointNameMiddleDIP,
                    VNHumanHandPoseObservationJointNameRingDIP,
                    VNHumanHandPoseObservationJointNameLittleDIP
                ];
            });
            for (VNHumanHandPoseObservation *obs in results) {
                NSError *pointsError = nil;
                NSDictionary<VNRecognizedPointKey, VNRecognizedPoint *> *points =
                    [obs recognizedPointsForGroupKey:VNHumanHandPoseObservationJointsGroupNameAll
                                                error:&pointsError];
                if (pointsError) {
                    continue;
                }
                for (NSUInteger i = 0; i < kTipKeys.count; ++i) {
                    if (i < fingerEnabled.size() && !fingerEnabled[i]) {
                        continue;
                    }
                    NSString *key = kTipKeys[i];
                    VNRecognizedPoint *point = points[key];
                    NSString *baseKey = kBaseKeys[i];
                    VNRecognizedPoint *basePoint = points[baseKey];
                    if (!point || point.confidence < minConfidence) {
                        continue;
                    }
                    if (!basePoint || basePoint.confidence < minConfidence * 0.6f) {
                        continue;
                    }
                    float x = point.location.x * bgra.cols;
                    float y = (1.0 - point.location.y) * bgra.rows;
                    float bx = basePoint.location.x * bgra.cols;
                    float by = (1.0 - basePoint.location.y) * bgra.rows;
                    ofVec2f tip(x * scaleX, y * scaleY);
                    ofVec2f base(bx * scaleX, by * scaleY);
                    ofVec2f dir = tip - base;
                    float len = dir.length();
                    if (len < 2.0f) {
                        continue;
                    }
                    float threshold = 0.03f * std::max(width, height);
                    if (len < threshold) {
                        continue;
                    }
                    outPoints.push_back({tip, dir, static_cast<float>(point.confidence)});
                }
            }
        }
    }

    CFRelease(buffer);
    return success;
}
