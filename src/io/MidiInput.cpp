#include "synth/io/MidiInput.hpp"

#include "synth/core/Logger.hpp"

#include <cstring>
#include <utility>

#if defined(SYNTH_PLATFORM_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#endif

namespace synth::io {

#if defined(SYNTH_PLATFORM_MACOS)

namespace {
std::string copyCfString(CFStringRef source) {
    if (source == nullptr) {
        return {};
    }

    const CFIndex length = CFStringGetLength(source);
    const CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    std::string result(static_cast<std::size_t>(maxSize), '\0');
    if (!CFStringGetCString(source, result.data(), maxSize, kCFStringEncodingUTF8)) {
        return {};
    }

    result.resize(std::strlen(result.c_str()));
    return result;
}

std::string querySourceName(MIDIEndpointRef source) {
    if (source == 0) {
        return "Unknown MIDI Source";
    }

    CFStringRef name = nullptr;
    if (MIDIObjectGetStringProperty(source, kMIDIPropertyDisplayName, &name) == noErr && name != nullptr) {
        std::string result = copyCfString(name);
        CFRelease(name);
        if (!result.empty()) {
            return result;
        }
    }

    if (MIDIObjectGetStringProperty(source, kMIDIPropertyName, &name) == noErr && name != nullptr) {
        std::string result = copyCfString(name);
        CFRelease(name);
        if (!result.empty()) {
            return result;
        }
    }

    return "Unknown MIDI Source";
}

void midiReadProc(const MIDIPacketList* packetList, void* readProcRefCon, void* srcConnRefCon) {
    auto* self = static_cast<MidiInput*>(readProcRefCon);
    if (self == nullptr || packetList == nullptr) {
        return;
    }

    const std::uint32_t sourceIndex = srcConnRefCon == nullptr
        ? 0
        : static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(srcConnRefCon) - 1U);

    const MIDIPacket* packet = &packetList->packet[0];
    for (UInt32 packetIndex = 0; packetIndex < packetList->numPackets; ++packetIndex) {
        self->handlePacketData(sourceIndex, packet->data, packet->length);
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

void MidiInput::handlePacketData(std::uint32_t sourceIndex, const unsigned char* data, std::size_t length) {
    if (!callback_ || length < 3 || data == nullptr) {
        return;
    }

    const unsigned char status = static_cast<unsigned char>(data[0] & 0xF0);
    const int noteNumber = static_cast<int>(data[1]);
    const float velocity = static_cast<float>(data[2]) / 127.0f;

    if (status == 0x90) {
        callback_(sourceIndex, noteNumber, data[2] == 0 ? 0.0f : velocity);
        return;
    }

    if (status == 0x80) {
        callback_(sourceIndex, noteNumber, 0.0f);
    }
}

bool MidiInput::start(MidiNoteCallback callback, bool connectAllSources) {
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

    if (!populateSources(connectAllSources)) {
        MIDIPortDispose(inputPort_);
        inputPort_ = 0;
        MIDIClientDispose(client_);
        client_ = 0;
        return false;
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
        for (const auto& source : sources_) {
            if (source.info.connected) {
                MIDIPortDisconnectSource(inputPort_, static_cast<MIDIEndpointRef>(source.endpoint));
            }
        }
        MIDIPortDispose(inputPort_);
        inputPort_ = 0;
    }
    if (client_ != 0) {
        MIDIClientDispose(client_);
        client_ = 0;
    }
#endif

    sources_.clear();
    callback_ = {};
    running_ = false;
}

bool MidiInput::isRunning() const {
    return running_;
}

std::size_t MidiInput::sourceCount() const {
    return sources_.size();
}

std::size_t MidiInput::connectedSourceCount() const {
    std::size_t count = 0;
    for (const auto& source : sources_) {
        if (source.info.connected) {
            ++count;
        }
    }
    return count;
}

std::vector<MidiSourceInfo> MidiInput::sources() const {
    std::vector<MidiSourceInfo> result;
    result.reserve(sources_.size());
    for (const auto& source : sources_) {
        result.push_back(source.info);
    }
    return result;
}

bool MidiInput::setSourceConnected(std::uint32_t sourceIndex, bool connected) {
    SourceState* source = findSourceState(sourceIndex);
    if (source == nullptr) {
        return false;
    }

#if defined(SYNTH_PLATFORM_MACOS)
    if (source->info.connected == connected) {
        return true;
    }

    const MIDIEndpointRef endpoint = static_cast<MIDIEndpointRef>(source->endpoint);
    void* sourceRefCon = reinterpret_cast<void*>(static_cast<std::uintptr_t>(source->info.index) + 1U);
    const OSStatus status = connected
        ? MIDIPortConnectSource(inputPort_, endpoint, sourceRefCon)
        : MIDIPortDisconnectSource(inputPort_, endpoint);

    if (status != noErr) {
        logger_.error("CoreMIDI: failed to update MIDI source connection.");
        return false;
    }

    source->info.connected = connected;
    return true;
#else
    (void)connected;
    return false;
#endif
}

#if defined(SYNTH_PLATFORM_MACOS)
bool MidiInput::populateSources(bool connectAllSources) {
    sources_.clear();
    sources_.reserve(static_cast<std::size_t>(MIDIGetNumberOfSources()));

    for (ItemCount index = 0; index < MIDIGetNumberOfSources(); ++index) {
        const MIDIEndpointRef source = MIDIGetSource(index);
        if (source == 0) {
            continue;
        }

        SourceState state;
        state.info.index = static_cast<std::uint32_t>(index);
        state.info.name = querySourceName(source);
        state.endpoint = static_cast<std::uint32_t>(source);
        if (connectAllSources) {
            void* sourceRefCon = reinterpret_cast<void*>(static_cast<std::uintptr_t>(state.info.index) + 1U);
            if (MIDIPortConnectSource(inputPort_, source, sourceRefCon) != noErr) {
                logger_.error("CoreMIDI: failed to connect MIDI source.");
                return false;
            }
            state.info.connected = true;
        }

        sources_.push_back(std::move(state));
    }

    return true;
}
#endif

MidiInput::SourceState* MidiInput::findSourceState(std::uint32_t sourceIndex) {
    for (auto& source : sources_) {
        if (source.info.index == sourceIndex) {
            return &source;
        }
    }

    return nullptr;
}

const MidiInput::SourceState* MidiInput::findSourceState(std::uint32_t sourceIndex) const {
    for (const auto& source : sources_) {
        if (source.info.index == sourceIndex) {
            return &source;
        }
    }

    return nullptr;
}

}  // namespace synth::io
