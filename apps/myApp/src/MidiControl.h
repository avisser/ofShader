#pragma once

#include "ofMain.h"
#include "ofxMidi.h"

#include <deque>
#include <string>
#include <vector>
#include <iosfwd>
#include <unordered_map>

class MidiControl : public ofxMidiListener {
public:
    void setup();
    void update();
    void close();
    void cyclePort();
    void toggleOutputTest();

    void registerControl(const std::string &id);
    void beginLearn(const std::string &id);
    void beginLearnMute(const std::string &id);
    bool consumePadHit(const std::string &id);
    bool consumeKnobValue(const std::string &id, float &outValue01);
    bool isMuteActive(const std::string &id) const;

    void newMidiMessage(ofxMidiMessage &message) override;

private:
    struct PadBinding {
        int channel = -1;
        int note = -1;
        bool valid() const { return channel >= 0 && note >= 0; }
    };

    struct KnobBinding {
        int channel = -1;
        int control = -1;
        float value01 = 0.0f;
        bool valid() const { return channel >= 0 && control >= 0; }
    };

    struct Binding {
        PadBinding pad;
        PadBinding mutePad;
        KnobBinding knob;
        bool padHit = false;
        bool knobUpdated = false;
        bool muteActive = false;
    };

    struct DeviceSettings {
        std::string name;
        std::unordered_map<std::string, Binding> bindings;
    };

    struct LearnState {
        enum class Mode {
            Auto,
            PadOnlyMute
        };
        bool active = false;
        bool windowStarted = false;
        uint64_t startMs = 0;
        int noteCount = 0;
        int ccCount = 0;
        int lastNote = -1;
        int lastNoteChannel = -1;
        int lastCc = -1;
        int lastCcChannel = -1;
        std::string targetId;
        Mode mode = Mode::Auto;
    };

    void processMessage(const ofxMidiMessage &message);
    void processLearning(const ofxMidiMessage &message);
    void finalizeLearning();
    void openPort(int index);
    void logPorts();
    void sendRandomTestMessage(uint64_t nowMs);
    bool loadSettings();
    void saveSettings();
    bool applySettingsForAvailableDevice();
    int findInPortByName(const std::string &name);
    DeviceSettings buildCurrentDeviceSettings();
    void writeBinding(std::ostream &out,
                      const std::string &target,
                      const PadBinding &pad,
                      const PadBinding &mute,
                      const KnobBinding &knob) const;

    static constexpr uint64_t kLearnWindowMs = 150;

    ofxMidiIn midiIn;
    ofxMidiOut midiOut;
    std::deque<ofxMidiMessage> queue;
    struct PendingNoteOff {
        int channel = 1;
        int note = 0;
        uint64_t dueMs = 0;
    };
    std::vector<PendingNoteOff> pendingNoteOffs;
    int currentPort = 0;
    int currentOutPort = 0;
    bool outputTestActive = false;
    uint64_t lastOutputMs = 0;
    uint64_t outputIntervalMs = 120;
    int outputTestChannel = 3;
    int outputTestControl = 0;
    int outputTestValue = 0;
    int outputTestControlMax = 31;
    std::string settingsPath;
    std::vector<DeviceSettings> savedDevices;

    LearnState learn;
    std::unordered_map<std::string, Binding> bindings;
};
