#include "MidiControl.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

void MidiControl::setup() {
    midiIn.addListener(this);
    midiIn.ignoreTypes(false, false, false);
    midiIn.setVerbose(false);
    midiIn.listInPorts();
    midiOut.listOutPorts();
    settingsPath = ofToDataPath("settings.yaml", true);
    loadSettings();
    if (!applySettingsForAvailableDevice()) {
        openPort(0);
    }
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

void MidiControl::beginLearnSaturation() {
    learn = LearnState{};
    learn.active = true;
    learn.target = LearnTarget::Saturation;
    ofLogNotice() << "MIDI learn (saturation): waiting for pad or knob input.";
}

bool MidiControl::isLearningKaleido() const {
    return learn.active && learn.target == LearnTarget::Kaleido;
}

bool MidiControl::isLearningHalftone() const {
    return learn.active && learn.target == LearnTarget::Halftone;
}

bool MidiControl::isLearningSaturation() const {
    return learn.active && learn.target == LearnTarget::Saturation;
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

bool MidiControl::consumeSaturationPadHit() {
    if (!saturationBinding.padHit) {
        return false;
    }
    saturationBinding.padHit = false;
    return true;
}

bool MidiControl::hasKaleidoKnobBinding() const {
    return kaleidoBinding.knob.valid();
}

bool MidiControl::hasHalftoneKnobBinding() const {
    return halftoneBinding.knob.valid();
}

bool MidiControl::hasSaturationKnobBinding() const {
    return saturationBinding.knob.valid();
}

bool MidiControl::consumeKaleidoKnobValue(float &outValue01) {
    if (!kaleidoBinding.knobUpdated) {
        return false;
    }
    kaleidoBinding.knobUpdated = false;
    outValue01 = kaleidoBinding.knob.value01;
    return true;
}

bool MidiControl::consumeHalftoneKnobValue(float &outValue01) {
    if (!halftoneBinding.knobUpdated) {
        return false;
    }
    halftoneBinding.knobUpdated = false;
    outValue01 = halftoneBinding.knob.value01;
    return true;
}

bool MidiControl::consumeSaturationKnobValue(float &outValue01) {
    if (!saturationBinding.knobUpdated) {
        return false;
    }
    saturationBinding.knobUpdated = false;
    outValue01 = saturationBinding.knob.value01;
    return true;
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
    ofLogNotice() << "MIDI test output: " << (outputTestActive ? "on" : "off")
                  << " (channel " << outputTestChannel << ")";
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

    if (saturationBinding.pad.valid() && message.status == MIDI_NOTE_ON && message.velocity > 0) {
        if (message.channel == saturationBinding.pad.channel && message.pitch == saturationBinding.pad.note) {
            saturationBinding.padHit = true;
        }
    }

    if (kaleidoBinding.knob.valid() && message.status == MIDI_CONTROL_CHANGE) {
        if (message.channel == kaleidoBinding.knob.channel && message.control == kaleidoBinding.knob.control) {
            float value01 = ofClamp(message.value / 127.0f, 0.0f, 1.0f);
            if (std::abs(value01 - kaleidoBinding.knob.value01) > 0.0005f) {
                kaleidoBinding.knob.value01 = value01;
                kaleidoBinding.knobUpdated = true;
            }
        }
    }

    if (halftoneBinding.knob.valid() && message.status == MIDI_CONTROL_CHANGE) {
        if (message.channel == halftoneBinding.knob.channel && message.control == halftoneBinding.knob.control) {
            float value01 = ofClamp(message.value / 127.0f, 0.0f, 1.0f);
            if (std::abs(value01 - halftoneBinding.knob.value01) > 0.0005f) {
                halftoneBinding.knob.value01 = value01;
                halftoneBinding.knobUpdated = true;
            }
        }
    }

    if (saturationBinding.knob.valid() && message.status == MIDI_CONTROL_CHANGE) {
        if (message.channel == saturationBinding.knob.channel && message.control == saturationBinding.knob.control) {
            float value01 = ofClamp(message.value / 127.0f, 0.0f, 1.0f);
            if (std::abs(value01 - saturationBinding.knob.value01) > 0.0005f) {
                saturationBinding.knob.value01 = value01;
                saturationBinding.knobUpdated = true;
            }
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
    } else if (learn.target == LearnTarget::Saturation) {
        binding = &saturationBinding;
        targetName = "saturation";
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
    saveSettings();
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
        int outIndex = -1;
        std::string inName = midiIn.getInPortName(currentPort);
        for (int i = 0; i < numOutPorts; ++i) {
            if (midiOut.getOutPortName(i) == inName) {
                outIndex = i;
                break;
            }
        }
        if (outIndex < 0) {
            outIndex = currentPort % numOutPorts;
        }
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

    int channel = outputTestChannel;
    int control = outputTestControl;
    int value = outputTestValue;
    midiOut.sendControlChange(channel, control, value);

    outputTestControl += 1;
    if (outputTestControl > outputTestControlMax) {
        outputTestControl = 0;
        outputTestValue += 16;
        if (outputTestValue > 127) {
            outputTestValue = 0;
        }
    }
}

bool MidiControl::loadSettings() {
    savedDevices.clear();
    ofFile file(settingsPath);
    if (!file.exists()) {
        return false;
    }

    ofBuffer buffer = file.readToBuffer();
    DeviceSettings *currentDevice = nullptr;
    std::string currentTarget;
    std::string currentType;

    auto trim = [](const std::string &s) {
        size_t start = 0;
        while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
            ++start;
        }
        size_t end = s.size();
        while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
            --end;
        }
        return s.substr(start, end - start);
    };

    auto parseValue = [&](const std::string &line) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) {
            return std::string();
        }
        std::string value = trim(line.substr(colon + 1));
        if (value.size() >= 2) {
            char quote = value.front();
            if ((quote == '"' || quote == '\'') && value.back() == quote) {
                value = value.substr(1, value.size() - 2);
            }
        }
        return value;
    };

    for (const auto &rawLine : buffer.getLines()) {
        std::string line = rawLine;
        size_t hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        std::string trimmed = trim(line);
        if (trimmed.empty()) {
            continue;
        }

        if (trimmed == "devices:" || trimmed == "bindings:") {
            continue;
        }

        if (trimmed.rfind("- name:", 0) == 0) {
            std::string name = parseValue(trimmed);
            savedDevices.push_back(DeviceSettings{});
            savedDevices.back().name = name;
            currentDevice = &savedDevices.back();
            currentTarget.clear();
            currentType.clear();
            continue;
        }

        if (!currentDevice) {
            continue;
        }

        if (trimmed.rfind("- target:", 0) == 0) {
            currentTarget = parseValue(trimmed);
            currentType.clear();
            continue;
        }

        if (trimmed.rfind("type:", 0) == 0) {
            currentType = parseValue(trimmed);
            continue;
        }

        Binding *binding = bindingForTarget(*currentDevice, currentTarget);
        if (!binding) {
            continue;
        }

        if (trimmed.rfind("channel:", 0) == 0) {
            int channel = -1;
            try {
                channel = std::stoi(parseValue(trimmed));
            } catch (...) {
                continue;
            }
            if (currentType == "pad") {
                binding->pad.channel = channel;
            } else if (currentType == "knob") {
                binding->knob.channel = channel;
            }
            continue;
        }

        if (trimmed.rfind("note:", 0) == 0) {
            int note = -1;
            try {
                note = std::stoi(parseValue(trimmed));
            } catch (...) {
                continue;
            }
            if (currentType == "pad") {
                binding->pad.note = note;
            }
            continue;
        }

        if (trimmed.rfind("control:", 0) == 0) {
            int control = -1;
            try {
                control = std::stoi(parseValue(trimmed));
            } catch (...) {
                continue;
            }
            if (currentType == "knob") {
                binding->knob.control = control;
            }
            continue;
        }
    }

    return !savedDevices.empty();
}

void MidiControl::saveSettings() {
    DeviceSettings current = buildCurrentDeviceSettings();
    if (current.name.empty()) {
        return;
    }

    auto it = std::find_if(savedDevices.begin(),
                           savedDevices.end(),
                           [&](const DeviceSettings &device) { return device.name == current.name; });
    if (it != savedDevices.end()) {
        *it = current;
    } else {
        savedDevices.push_back(current);
    }

    std::ostringstream out;
    out << "devices:\n";
    for (const auto &device : savedDevices) {
        if (device.name.empty()) {
            continue;
        }
        out << "  - name: \"" << device.name << "\"\n";
        out << "    bindings:\n";
        writeBinding(out, "kaleido", device.kaleido.pad, device.kaleido.knob);
        writeBinding(out, "halftone", device.halftone.pad, device.halftone.knob);
        writeBinding(out, "saturation", device.saturation.pad, device.saturation.knob);
    }

    ofBuffer buffer(out.str().c_str(), out.str().size());
    ofFile file(settingsPath, ofFile::WriteOnly, true);
    file.writeFromBuffer(buffer);
}

bool MidiControl::applySettingsForAvailableDevice() {
    if (savedDevices.empty()) {
        return false;
    }
    for (const auto &device : savedDevices) {
        if (device.name.empty()) {
            continue;
        }
        int portIndex = findInPortByName(device.name);
        if (portIndex >= 0) {
            openPort(portIndex);
            kaleidoBinding = device.kaleido;
            halftoneBinding = device.halftone;
            saturationBinding = device.saturation;
            ofLogNotice() << "MIDI settings: loaded bindings for device \""
                          << device.name << "\".";
            return true;
        }
    }

    ofLogWarning() << "MIDI settings: no matching device found for saved names.";
    logPorts();
    return false;
}

int MidiControl::findInPortByName(const std::string &name) {
    auto lower = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    };

    std::string target = lower(name);
    int numPorts = midiIn.getNumInPorts();
    int fallback = -1;
    for (int i = 0; i < numPorts; ++i) {
        std::string portName = midiIn.getInPortName(i);
        std::string portLower = lower(portName);
        if (portLower == target) {
            return i;
        }
        if (fallback < 0 && portLower.find(target) != std::string::npos) {
            fallback = i;
        }
    }
    return fallback;
}

MidiControl::Binding *MidiControl::bindingForTarget(DeviceSettings &device, const std::string &target) {
    if (target == "kaleido") {
        return &device.kaleido;
    }
    if (target == "halftone") {
        return &device.halftone;
    }
    if (target == "saturation") {
        return &device.saturation;
    }
    return nullptr;
}

MidiControl::DeviceSettings MidiControl::buildCurrentDeviceSettings() {
    DeviceSettings device;
    if (midiIn.getNumInPorts() <= 0) {
        return device;
    }
    if (currentPort >= 0 && currentPort < midiIn.getNumInPorts()) {
        device.name = midiIn.getInPortName(currentPort);
    }
    device.kaleido = kaleidoBinding;
    device.halftone = halftoneBinding;
    device.saturation = saturationBinding;
    device.kaleido.padHit = false;
    device.halftone.padHit = false;
    device.saturation.padHit = false;
    device.kaleido.knobUpdated = false;
    device.halftone.knobUpdated = false;
    device.saturation.knobUpdated = false;
    device.kaleido.knob.value01 = 0.0f;
    device.halftone.knob.value01 = 0.0f;
    device.saturation.knob.value01 = 0.0f;
    return device;
}

void MidiControl::writeBinding(std::ostream &out,
                               const std::string &target,
                               const PadBinding &pad,
                               const KnobBinding &knob) const {
    if (pad.valid()) {
        out << "      - target: " << target << "\n";
        out << "        type: pad\n";
        out << "        channel: " << pad.channel << "\n";
        out << "        note: " << pad.note << "\n";
    }
    if (knob.valid()) {
        out << "      - target: " << target << "\n";
        out << "        type: knob\n";
        out << "        channel: " << knob.channel << "\n";
        out << "        control: " << knob.control << "\n";
    }
}
