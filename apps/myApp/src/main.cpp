#include "ofMain.h"
#include "ofApp.h"

#include <cstdlib>
#include <cerrno>
#include <string>

namespace {
bool parseInt(const char *value, int &out) {
    if (!value) {
        return false;
    }
    char *end = nullptr;
    errno = 0;
    long parsed = std::strtol(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0') {
        return false;
    }
    out = static_cast<int>(parsed);
    return true;
}

AppConfig parseArgs(int argc, char **argv) {
    AppConfig config;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--bg" && i + 1 < argc) {
            config.bgPath = argv[++i];
        } else if (arg == "--cam" && i + 1 < argc) {
            int value = 0;
            if (parseInt(argv[++i], value)) {
                config.camIndex = value;
            }
        } else if (arg == "--width" && i + 1 < argc) {
            int value = 0;
            if (parseInt(argv[++i], value) && value > 0) {
                config.camWidth = value;
            }
        } else if (arg == "--height" && i + 1 < argc) {
            int value = 0;
            if (parseInt(argv[++i], value) && value > 0) {
                config.camHeight = value;
            }
        } else if (arg == "--fps" && i + 1 < argc) {
            int value = 0;
            if (parseInt(argv[++i], value) && value > 0) {
                config.camFps = value;
            }
        }
    }
    return config;
}
} // namespace

int main(int argc, char **argv) {
    AppConfig config = parseArgs(argc, argv);

    ofGLWindowSettings settings;
    settings.setSize(config.camWidth, config.camHeight);
    settings.setGLVersion(3, 2);
    settings.windowMode = OF_FULLSCREEN;

    auto window = ofCreateWindow(settings);
    ofRunApp(window, std::make_shared<ofApp>(config));
    ofRunMainLoop();
}
