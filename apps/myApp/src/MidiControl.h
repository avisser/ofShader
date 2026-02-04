#pragma once

#include "ofMain.h"
#include "ofxMidi.h"

#include <deque>
#include <string>
#include <vector>
#include <iosfwd>

class MidiControl : public ofxMidiListener {
public:
    void setup();
    void update();
    void close();
    void cyclePort();
    void toggleOutputTest();

    void beginLearnKaleido();
    void beginLearnKaleidoZoom();
    void beginLearnHalftone();
    void beginLearnSaturation();
    bool isLearningKaleido() const;
    bool isLearningKaleidoZoom() const;
    bool isLearningHalftone() const;
    bool isLearningSaturation() const;

    bool consumeKaleidoPadHit();
    bool consumeKaleidoZoomPadHit();
    bool consumeHalftonePadHit();
    bool consumeSaturationPadHit();
    bool hasKaleidoKnobBinding() const;
    bool hasKaleidoZoomKnobBinding() const;
    bool hasHalftoneKnobBinding() const;
    bool hasSaturationKnobBinding() const;
    bool consumeKaleidoKnobValue(float &outValue01);
    bool consumeKaleidoZoomKnobValue(float &outValue01);
    bool consumeHalftoneKnobValue(float &outValue01);
    bool consumeSaturationKnobValue(float &outValue01);

    void newMidiMessage(ofxMidiMessage &message) override;

private:
    enum class LearnTarget {
        Kaleido,
        KaleidoZoom,
        Halftone,
        Saturation
    };

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
        KnobBinding knob;
        bool padHit = false;
        bool knobUpdated = false;
    };

    struct DeviceSettings {
        std::string name;
        Binding kaleido;
        Binding kaleidoZoom;
        Binding halftone;
        Binding saturation;
    };

    struct LearnState {
        bool active = false;
        bool windowStarted = false;
        uint64_t startMs = 0;
        int noteCount = 0;
        int ccCount = 0;
        int lastNote = -1;
        int lastNoteChannel = -1;
        int lastCc = -1;
        int lastCcChannel = -1;
        LearnTarget target = LearnTarget::Kaleido;
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
    Binding *bindingForTarget(DeviceSettings &device, const std::string &target);
    DeviceSettings buildCurrentDeviceSettings();
    void writeBinding(std::ostream &out,
                      const std::string &target,
                      const PadBinding &pad,
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
    Binding kaleidoBinding;
    Binding kaleidoZoomBinding;
    Binding halftoneBinding;
    Binding saturationBinding;
};
