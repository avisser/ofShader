#include "MidiControl.h"

void MidiControl::setup() {
    midiIn.addListener(this);
    midiIn.ignoreTypes(false, false, false);
    midiIn.setVerbose(false);
    midiIn.listInPorts();
    midiOut.listOutPorts();
    openPort(0);
}

void MidiControl::close() {
    midiIn.closePort();
    midiIn.removeListener(this);
    midiOut.closePort();
}

void MidiControl::newMidiMessage(ofxMidiMessage &message) {
    queue.push_back(message);
    if (queue.size() > 256) {
        queue.pop_front();
    }
}

void MidiControl::update() {
    uint64_t now = ofGetElapsedTimeMillis();
    if (learn.active && learn.windowStarted) {
        if ((now - learn.startMs) >= kLearnWindowMs) {
            finalizeLearning();
        }
    }

    if (outputTestActive) {
        sendRandomTestMessage(now);
        for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end();) {
            if (now >= it->dueMs) {
                midiOut.sendNoteOff(it->channel, it->note, 0);
                it = pendingNoteOffs.erase(it);
            } else {
                ++it;
            }
        }
    }

    while (!queue.empty()) {
        ofxMidiMessage msg = queue.front();
        queue.pop_front();
        processMessage(msg);
    }
}

void MidiControl::beginLearnKaleido() {
    learn = LearnState{};
    learn.active = true;
    learn.target = LearnTarget::Kaleido;
    ofLogNotice() << "MIDI learn (kaleido): waiting for pad or knob input.";
}

void MidiControl::beginLearnHalftone() {
    learn = LearnState{};
    learn.active = true;
    learn.target = LearnTarget::Halftone;
    ofLogNotice() << "MIDI learn (halftone): waiting for pad or knob input.";
}

bool MidiControl::isLearningKaleido() const {
    return learn.active && learn.target == LearnTarget::Kaleido;
}

bool MidiControl::isLearningHalftone() const {
    return learn.active && learn.target == LearnTarget::Halftone;
}

bool MidiControl::consumeKaleidoPadHit() {
    if (!kaleidoBinding.padHit) {
        return false;
    }
    kaleidoBinding.padHit = false;
    return true;
}

bool MidiControl::consumeHalftonePadHit() {
    if (!halftoneBinding.padHit) {
        return false;
    }
    halftoneBinding.padHit = false;
    return true;
}

bool MidiControl::hasKaleidoKnobBinding() const {
    return kaleidoBinding.knob.valid();
}

bool MidiControl::hasHalftoneKnobBinding() const {
    return halftoneBinding.knob.valid();
}

float MidiControl::getKaleidoKnobValue01() const {
    return kaleidoBinding.knob.value01;
}

float MidiControl::getHalftoneKnobValue01() const {
    return halftoneBinding.knob.value01;
}

void MidiControl::cyclePort() {
    int numPorts = midiIn.getNumInPorts();
    if (numPorts <= 0) {
        ofLogWarning() << "MIDI: no input ports available.";
        return;
    }
    int nextPort = (currentPort + 1) % numPorts;
    openPort(nextPort);
    logPorts();
}

void MidiControl::toggleOutputTest() {
    outputTestActive = !outputTestActive;
    lastOutputMs = 0;
    pendingNoteOffs.clear();
    ofLogNotice() << "MIDI test output: " << (outputTestActive ? "on" : "off");
}

void MidiControl::processMessage(const ofxMidiMessage &message) {
    if (learn.active) {
        processLearning(message);
        return;
    }

    if (kaleidoBinding.pad.valid() && message.status == MIDI_NOTE_ON && message.velocity > 0) {
        if (message.channel == kaleidoBinding.pad.channel && message.pitch == kaleidoBinding.pad.note) {
            kaleidoBinding.padHit = true;
        }
    }

    if (halftoneBinding.pad.valid() && message.status == MIDI_NOTE_ON && message.velocity > 0) {
        if (message.channel == halftoneBinding.pad.channel && message.pitch == halftoneBinding.pad.note) {
            halftoneBinding.padHit = true;
        }
    }

    if (kaleidoBinding.knob.valid() && message.status == MIDI_CONTROL_CHANGE) {
        if (message.channel == kaleidoBinding.knob.channel && message.control == kaleidoBinding.knob.control) {
            kaleidoBinding.knob.value01 = ofClamp(message.value / 127.0f, 0.0f, 1.0f);
        }
    }

    if (halftoneBinding.knob.valid() && message.status == MIDI_CONTROL_CHANGE) {
        if (message.channel == halftoneBinding.knob.channel && message.control == halftoneBinding.knob.control) {
            halftoneBinding.knob.value01 = ofClamp(message.value / 127.0f, 0.0f, 1.0f);
        }
    }
}

void MidiControl::processLearning(const ofxMidiMessage &message) {
    if (!learn.windowStarted) {
        learn.windowStarted = true;
        learn.startMs = ofGetElapsedTimeMillis();
    }

    if (message.status == MIDI_NOTE_ON && message.velocity > 0) {
        learn.noteCount += 1;
        learn.lastNote = message.pitch;
        learn.lastNoteChannel = message.channel;
    } else if (message.status == MIDI_CONTROL_CHANGE) {
        learn.ccCount += 1;
        learn.lastCc = message.control;
        learn.lastCcChannel = message.channel;
    }
}

void MidiControl::finalizeLearning() {
    Binding *binding = nullptr;
    const char *targetName = "unknown";
    if (learn.target == LearnTarget::Kaleido) {
        binding = &kaleidoBinding;
        targetName = "kaleido";
    } else if (learn.target == LearnTarget::Halftone) {
        binding = &halftoneBinding;
        targetName = "halftone";
    }

    if (!binding) {
        ofLogWarning() << "MIDI learn: invalid target.";
        learn.active = false;
        learn.windowStarted = false;
        return;
    }

    if (learn.ccCount >= 5 && learn.lastCc >= 0) {
        binding->knob.channel = learn.lastCcChannel;
        binding->knob.control = learn.lastCc;
        binding->knob.value01 = 0.0f;
        ofLogNotice() << "MIDI learn (" << targetName << "): bound knob CC "
                      << binding->knob.control << " on channel " << binding->knob.channel;
    } else if (learn.noteCount >= 1 && learn.lastNote >= 0) {
        binding->pad.channel = learn.lastNoteChannel;
        binding->pad.note = learn.lastNote;
        ofLogNotice() << "MIDI learn (" << targetName << "): bound pad note "
                      << binding->pad.note << " on channel " << binding->pad.channel;
    } else {
        ofLogWarning() << "MIDI learn (" << targetName << "): no valid input detected.";
    }

    learn.active = false;
    learn.windowStarted = false;
}

void MidiControl::openPort(int index) {
    int numPorts = midiIn.getNumInPorts();
    if (numPorts <= 0) {
        ofLogWarning() << "MIDI: no input ports available.";
        return;
    }

    int clamped = index % numPorts;
    if (clamped < 0) {
        clamped += numPorts;
    }

    if (midiIn.isOpen()) {
        midiIn.closePort();
    }

    midiIn.openPort(clamped);
    currentPort = clamped;
    ofLogNotice() << "MIDI: listening on port " << currentPort
                  << " (" << midiIn.getInPortName(currentPort) << ")";

    int numOutPorts = midiOut.getNumOutPorts();
    if (numOutPorts > 0) {
        int outIndex = currentPort % numOutPorts;
        if (midiOut.isOpen()) {
            midiOut.closePort();
        }
        midiOut.openPort(outIndex);
        currentOutPort = outIndex;
        ofLogNotice() << "MIDI: sending on port " << currentOutPort
                      << " (" << midiOut.getOutPortName(currentOutPort) << ")";
    } else {
        ofLogWarning() << "MIDI: no output ports available.";
    }
}

void MidiControl::logPorts() {
    int numPorts = midiIn.getNumInPorts();
    ofLogNotice() << "MIDI ports: " << numPorts;
    for (int i = 0; i < numPorts; ++i) {
        ofLogNotice() << "  [" << i << "] " << midiIn.getInPortName(i);
    }
    int numOutPorts = midiOut.getNumOutPorts();
    ofLogNotice() << "MIDI out ports: " << numOutPorts;
    for (int i = 0; i < numOutPorts; ++i) {
        ofLogNotice() << "  [" << i << "] " << midiOut.getOutPortName(i);
    }
}

void MidiControl::sendRandomTestMessage(uint64_t nowMs) {
    if (!midiOut.isOpen()) {
        return;
    }

    if (lastOutputMs != 0 && (nowMs - lastOutputMs) < outputIntervalMs) {
        return;
    }
    lastOutputMs = nowMs;

    int channel = 1;
    if (kaleidoBinding.pad.valid()) {
        channel = kaleidoBinding.pad.channel;
    } else if (kaleidoBinding.knob.valid()) {
        channel = kaleidoBinding.knob.channel;
    }

    int note = 36 + static_cast<int>(ofRandom(0, 16));
    int velocity = 30 + static_cast<int>(ofRandom(0, 97));
    midiOut.sendNoteOn(channel, note, velocity);
    pendingNoteOffs.push_back({channel, note, nowMs + 120});

    int control = 1 + static_cast<int>(ofRandom(0, 32));
    int value = static_cast<int>(ofRandom(0, 128));
    midiOut.sendControlChange(channel, control, value);
}
