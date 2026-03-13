#include "synth/io/MidiInput.hpp"

#include "synth/core/Logger.hpp"

#if defined(SYNTH_PLATFORM_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#endif

namespace synth::io {

#if defined(SYNTH_PLATFORM_MACOS)

namespace {
void midiReadProc(const MIDIPacketList* packetList, void* readProcRefCon, void* /*srcConnRefCon*/) {
    auto* self = static_cast<MidiInput*>(readProcRefCon);
    if (self == nullptr || packetList == nullptr) {
        return;
    }

    const MIDIPacket* packet = &packetList->packet[0];
    for (UInt32 packetIndex = 0; packetIndex < packetList->numPackets; ++packetIndex) {
        self->handlePacketData(packet->data, packet->length);
        packet = MIDIPacketNext(packet);
    }
}

}  // namespace

#endif

MidiInput::MidiInput(core::Logger& logger)
    : logger_(logger) {}

MidiInput::~MidiInput() {
    stop();
}

void MidiInput::handlePacketData(const unsigned char* data, std::size_t length) {
    if (!callback_ || length < 3 || data == nullptr) {
        return;
    }

    const unsigned char status = static_cast<unsigned char>(data[0] & 0xF0);
    const int noteNumber = static_cast<int>(data[1]);
    const float velocity = static_cast<float>(data[2]) / 127.0f;

    if (status == 0x90) {
        callback_(noteNumber, data[2] == 0 ? 0.0f : velocity);
        return;
    }

    if (status == 0x80) {
        callback_(noteNumber, 0.0f);
    }
}

bool MidiInput::start(MidiNoteCallback callback) {
    if (running_) {
        return true;
    }

    callback_ = std::move(callback);

#if defined(SYNTH_PLATFORM_MACOS)
    if (MIDIClientCreate(CFSTR("Synthesizer MIDI Client"), nullptr, nullptr, &client_) != noErr) {
        logger_.error("CoreMIDI: failed to create MIDI client.");
        return false;
    }

    if (MIDIInputPortCreate(client_, CFSTR("Synthesizer MIDI Input"), &midiReadProc, this, &inputPort_) != noErr) {
        logger_.error("CoreMIDI: failed to create MIDI input port.");
        MIDIClientDispose(client_);
        client_ = 0;
        return false;
    }

    sourceCount_ = static_cast<std::size_t>(MIDIGetNumberOfSources());
    for (ItemCount index = 0; index < MIDIGetNumberOfSources(); ++index) {
        MIDIEndpointRef source = MIDIGetSource(index);
        if (source == 0) {
            continue;
        }
        MIDIPortConnectSource(inputPort_, source, nullptr);
    }

    logger_.info("CoreMIDI: MIDI input started.");
    running_ = true;
    return true;
#else
    logger_.error("MIDI input is only implemented for macOS in this build.");
    return false;
#endif
}

void MidiInput::stop() {
#if defined(SYNTH_PLATFORM_MACOS)
    if (inputPort_ != 0) {
        MIDIPortDispose(inputPort_);
        inputPort_ = 0;
    }
    if (client_ != 0) {
        MIDIClientDispose(client_);
        client_ = 0;
    }
#endif

    sourceCount_ = 0;
    callback_ = {};
    running_ = false;
}

bool MidiInput::isRunning() const {
    return running_;
}

std::size_t MidiInput::sourceCount() const {
    return sourceCount_;
}

}  // namespace synth::io
