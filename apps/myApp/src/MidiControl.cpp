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

void MidiControl::registerControl(const std::string &id) {
    if (id.empty()) {
        return;
    }
    bindings[id];
}

void MidiControl::beginLearn(const std::string &id) {
    if (id.empty()) {
        return;
    }
    registerControl(id);
    learn = LearnState{};
    learn.active = true;
    learn.mode = LearnState::Mode::Auto;
    learn.targetId = id;
    ofLogNotice() << "MIDI learn (" << id << "): waiting for pad or knob input.";
}

void MidiControl::beginLearnMute(const std::string &id) {
    if (id.empty()) {
        return;
    }
    registerControl(id);
    learn = LearnState{};
    learn.active = true;
    learn.mode = LearnState::Mode::PadOnlyMute;
    learn.targetId = id;
    ofLogNotice() << "MIDI learn (" << id << "): waiting for mute pad input.";
}

bool MidiControl::consumePadHit(const std::string &id) {
    auto it = bindings.find(id);
    if (it == bindings.end()) {
        return false;
    }
    if (!it->second.padHit) {
        return false;
    }
    it->second.padHit = false;
    return true;
}

bool MidiControl::consumeKnobValue(const std::string &id, float &outValue01) {
    auto it = bindings.find(id);
    if (it == bindings.end()) {
        return false;
    }
    if (!it->second.knobUpdated) {
        return false;
    }
    it->second.knobUpdated = false;
    outValue01 = it->second.knob.value01;
    return true;
}

bool MidiControl::isMuteActive(const std::string &id) const {
    auto it = bindings.find(id);
    if (it == bindings.end()) {
        return false;
    }
    return it->second.muteActive;
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

    bool isNoteOn = (message.status == MIDI_NOTE_ON && message.velocity > 0);
    bool isNoteOff = (message.status == MIDI_NOTE_OFF) ||
                     (message.status == MIDI_NOTE_ON && message.velocity == 0);

    if (isNoteOn) {
        for (auto &entry : bindings) {
            auto &binding = entry.second;
            if (binding.pad.valid() &&
                message.channel == binding.pad.channel &&
                message.pitch == binding.pad.note) {
                binding.padHit = true;
            }
            if (binding.mutePad.valid() &&
                message.channel == binding.mutePad.channel &&
                message.pitch == binding.mutePad.note) {
                binding.muteActive = true;
            }
        }
    }

    if (isNoteOff) {
        for (auto &entry : bindings) {
            auto &binding = entry.second;
            if (binding.mutePad.valid() &&
                message.channel == binding.mutePad.channel &&
                message.pitch == binding.mutePad.note) {
                binding.muteActive = false;
            }
        }
    }

    if (message.status == MIDI_CONTROL_CHANGE) {
        for (auto &entry : bindings) {
            auto &binding = entry.second;
            if (binding.knob.valid() &&
                message.channel == binding.knob.channel &&
                message.control == binding.knob.control) {
                float value01 = ofClamp(message.value / 127.0f, 0.0f, 1.0f);
                if (std::abs(value01 - binding.knob.value01) > 0.0005f) {
                    binding.knob.value01 = value01;
                    binding.knobUpdated = true;
                }
            }
        }
    }
}

void MidiControl::processLearning(const ofxMidiMessage &message) {
    if (!learn.windowStarted) {
        learn.windowStarted = true;
        learn.startMs = ofGetElapsedTimeMillis();
    }

    if (learn.mode == LearnState::Mode::PadOnlyMute) {
        if (message.status == MIDI_NOTE_ON && message.velocity > 0) {
            learn.noteCount += 1;
            learn.lastNote = message.pitch;
            learn.lastNoteChannel = message.channel;
        }
        return;
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
    if (learn.targetId.empty()) {
        ofLogWarning() << "MIDI learn: invalid target.";
        learn.active = false;
        learn.windowStarted = false;
        return;
    }

    Binding &binding = bindings[learn.targetId];

    if (learn.mode == LearnState::Mode::PadOnlyMute) {
        if (learn.noteCount >= 1 && learn.lastNote >= 0) {
            binding.mutePad.channel = learn.lastNoteChannel;
            binding.mutePad.note = learn.lastNote;
            ofLogNotice() << "MIDI learn (" << learn.targetId << "): bound mute pad note "
                          << binding.mutePad.note << " on channel " << binding.mutePad.channel;
        } else {
            ofLogWarning() << "MIDI learn (" << learn.targetId << "): no valid mute pad input detected.";
        }
        learn.active = false;
        learn.windowStarted = false;
        saveSettings();
        return;
    }

    if (learn.ccCount >= 5 && learn.lastCc >= 0) {
        binding.knob.channel = learn.lastCcChannel;
        binding.knob.control = learn.lastCc;
        binding.knob.value01 = 0.0f;
        ofLogNotice() << "MIDI learn (" << learn.targetId << "): bound knob CC "
                      << binding.knob.control << " on channel " << binding.knob.channel;
    } else if (learn.noteCount >= 1 && learn.lastNote >= 0) {
        binding.pad.channel = learn.lastNoteChannel;
        binding.pad.note = learn.lastNote;
        ofLogNotice() << "MIDI learn (" << learn.targetId << "): bound pad note "
                      << binding.pad.note << " on channel " << binding.pad.channel;
    } else {
        ofLogWarning() << "MIDI learn (" << learn.targetId << "): no valid input detected.";
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
    std::string currentControl;
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
            currentControl.clear();
            currentType.clear();
            continue;
        }

        if (!currentDevice) {
            continue;
        }

        if (trimmed.rfind("- target:", 0) == 0 || trimmed.rfind("- control:", 0) == 0) {
            currentControl = parseValue(trimmed);
            currentType.clear();
            continue;
        }

        if (trimmed.rfind("type:", 0) == 0) {
            currentType = parseValue(trimmed);
            continue;
        }

        if (currentControl.empty()) {
            continue;
        }
        Binding &binding = currentDevice->bindings[currentControl];

        if (trimmed.rfind("channel:", 0) == 0) {
            int channel = -1;
            try {
                channel = std::stoi(parseValue(trimmed));
            } catch (...) {
                continue;
            }
            if (currentType == "pad") {
                binding.pad.channel = channel;
            } else if (currentType == "mute") {
                binding.mutePad.channel = channel;
            } else if (currentType == "knob") {
                binding.knob.channel = channel;
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
                binding.pad.note = note;
            } else if (currentType == "mute") {
                binding.mutePad.note = note;
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
                binding.knob.control = control;
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
        std::vector<std::string> keys;
        keys.reserve(device.bindings.size());
        for (const auto &entry : device.bindings) {
            keys.push_back(entry.first);
        }
        std::sort(keys.begin(), keys.end());
        for (const auto &key : keys) {
            auto it = device.bindings.find(key);
            if (it == device.bindings.end()) {
                continue;
            }
            const auto &binding = it->second;
            writeBinding(out, key, binding.pad, binding.mutePad, binding.knob);
        }
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
            bindings = device.bindings;
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

MidiControl::DeviceSettings MidiControl::buildCurrentDeviceSettings() {
    DeviceSettings device;
    if (midiIn.getNumInPorts() <= 0) {
        return device;
    }
    if (currentPort >= 0 && currentPort < midiIn.getNumInPorts()) {
        device.name = midiIn.getInPortName(currentPort);
    }
    for (const auto &entry : bindings) {
        const auto &id = entry.first;
        const auto &binding = entry.second;
        Binding clean = binding;
        clean.padHit = false;
        clean.knobUpdated = false;
        clean.muteActive = false;
        clean.knob.value01 = 0.0f;
        if (clean.pad.valid() || clean.mutePad.valid() || clean.knob.valid()) {
            device.bindings[id] = clean;
        }
    }
    return device;
}

void MidiControl::writeBinding(std::ostream &out,
                               const std::string &target,
                               const PadBinding &pad,
                               const PadBinding &mute,
                               const KnobBinding &knob) const {
    if (pad.valid()) {
        out << "      - control: " << target << "\n";
        out << "        type: pad\n";
        out << "        channel: " << pad.channel << "\n";
        out << "        note: " << pad.note << "\n";
    }
    if (mute.valid()) {
        out << "      - control: " << target << "\n";
        out << "        type: mute\n";
        out << "        channel: " << mute.channel << "\n";
        out << "        note: " << mute.note << "\n";
    }
    if (knob.valid()) {
        out << "      - control: " << target << "\n";
        out << "        type: knob\n";
        out << "        channel: " << knob.channel << "\n";
        out << "        control: " << knob.control << "\n";
    }
}
