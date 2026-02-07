#include "VisionFaceDetector.h"

#import <Vision/Vision.h>
#import <CoreML/CoreML.h>
#import <CoreVideo/CoreVideo.h>

#include <opencv2/imgproc.hpp>

#include <algorithm>

bool VisionFaceDetector::setup(float scale) {
    setScale(scale);
    return true;
}

void VisionFaceDetector::setScale(float scale) {
    this->scale = std::max(0.1f, std::min(scale, 1.0f));
}

bool VisionFaceDetector::detect(const ofPixels &pixels, std::vector<ofRectangle> &outFaces) {
    outFaces.clear();
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
        VNDetectFaceRectanglesRequest *request = [[VNDetectFaceRectanglesRequest alloc] init];
        VNImageRequestHandler *handler =
            [[VNImageRequestHandler alloc] initWithCVPixelBuffer:buffer options:@{}];
        NSError *error = nil;
        [handler performRequests:@[request] error:&error];
        if (error) {
            lastError = [[error localizedDescription] UTF8String];
            success = false;
        } else {
            NSArray<VNFaceObservation *> *results = request.results;
            float scaleX = static_cast<float>(width) / static_cast<float>(bgra.cols);
            float scaleY = static_cast<float>(height) / static_cast<float>(bgra.rows);
            for (VNFaceObservation *obs in results) {
                CGRect box = obs.boundingBox;
                float x = box.origin.x * bgra.cols;
                float y = (1.0 - box.origin.y - box.size.height) * bgra.rows;
                float w = box.size.width * bgra.cols;
                float h = box.size.height * bgra.rows;
                outFaces.emplace_back(x * scaleX, y * scaleY, w * scaleX, h * scaleY);
            }
        }
    }

    CFRelease(buffer);
    return success;
}
