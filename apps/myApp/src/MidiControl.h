#pragma once

#include "ofMain.h"
#include "ofxMidi.h"

#include <deque>
#include <vector>

class MidiControl : public ofxMidiListener {
public:
    void setup();
    void update();
    void close();
    void cyclePort();
    void toggleOutputTest();

    void beginLearnKaleido();
    void beginLearnHalftone();
    bool isLearningKaleido() const;
    bool isLearningHalftone() const;

    bool consumeKaleidoPadHit();
    bool consumeHalftonePadHit();
    bool hasKaleidoKnobBinding() const;
    bool hasHalftoneKnobBinding() const;
    float getKaleidoKnobValue01() const;
    float getHalftoneKnobValue01() const;

    void newMidiMessage(ofxMidiMessage &message) override;

private:
    enum class LearnTarget {
        Kaleido,
        Halftone
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

    LearnState learn;
    Binding kaleidoBinding;
    Binding halftoneBinding;
};
