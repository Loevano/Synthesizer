#include "synth/midi/MidiInput.hpp"

#include "synth/core/Logger.hpp"

#include <memory>

#if defined(SYNTH_PLATFORM_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#endif

namespace synth::midi {

#if defined(SYNTH_PLATFORM_MACOS)

namespace {

std::string cfStringToStd(CFStringRef value) {
    if (value == nullptr) {
        return "unknown";
    }

    char buffer[256] = {0};
    if (CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
        return std::string(buffer);
    }
    return "unknown";
}

}  // namespace

struct MidiInput::Impl {
    explicit Impl(core::Logger& loggerRef)
        : logger(loggerRef) {}

    static void midiReadProc(const MIDIPacketList* packetList, void* readProcRefCon, void* /*srcConnRefCon*/) {
        auto* self = static_cast<Impl*>(readProcRefCon);
        if (self == nullptr || packetList == nullptr) {
            return;
        }

        const MIDIPacket* packet = &packetList->packet[0];
        for (UInt32 i = 0; i < packetList->numPackets; ++i) {
            std::size_t index = 0;
            while (index < packet->length) {
                const std::uint8_t status = packet->data[index];
                if ((status & 0x80) == 0) {
                    ++index;
                    continue;
                }

                const std::uint8_t type = status & 0xF0;
                const std::size_t messageSize = (type == 0xC0 || type == 0xD0) ? 2 : 3;
                if (index + messageSize > packet->length) {
                    break;
                }

                MidiMessage message{};
                message.size = messageSize;
                for (std::size_t b = 0; b < messageSize; ++b) {
                    message.bytes[b] = packet->data[index + b];
                }

                if (self->handler) {
                    self->handler(message);
                }

                index += messageSize;
            }

            packet = MIDIPacketNext(packet);
        }
    }

    bool start(MessageHandler messageHandler) {
        handler = std::move(messageHandler);

        if (MIDIClientCreate(CFSTR("SynthesizerClient"), nullptr, nullptr, &client) != noErr) {
            logger.error("MIDI: failed to create MIDI client.");
            return false;
        }

        if (MIDIInputPortCreate(client, CFSTR("SynthesizerInput"), midiReadProc, this, &inputPort) != noErr) {
            logger.error("MIDI: failed to create input port.");
            MIDIClientDispose(client);
            client = 0;
            return false;
        }

        const ItemCount sourceCount = MIDIGetNumberOfSources();
        if (sourceCount == 0) {
            logger.warn("MIDI: no MIDI input sources found.");
            return true;
        }

        for (ItemCount i = 0; i < sourceCount; ++i) {
            MIDIEndpointRef source = MIDIGetSource(i);
            if (source == 0) {
                continue;
            }

            if (MIDIPortConnectSource(inputPort, source, nullptr) == noErr) {
                CFStringRef name = nullptr;
                MIDIObjectGetStringProperty(source, kMIDIPropertyName, &name);
                logger.info("MIDI: connected source " + cfStringToStd(name));
                if (name != nullptr) {
                    CFRelease(name);
                }
            }
        }

        return true;
    }

    void stop() {
        if (inputPort != 0) {
            MIDIPortDispose(inputPort);
            inputPort = 0;
        }
        if (client != 0) {
            MIDIClientDispose(client);
            client = 0;
        }
        handler = nullptr;
    }

    core::Logger& logger;
    MIDIClientRef client = 0;
    MIDIPortRef inputPort = 0;
    MessageHandler handler;
};

#else

struct MidiInput::Impl {
    explicit Impl(core::Logger& loggerRef)
        : logger(loggerRef) {}

    bool start(MessageHandler /*messageHandler*/) {
        logger.warn("MIDI input scaffold is only implemented for macOS.");
        return true;
    }

    void stop() {}

    core::Logger& logger;
};

#endif

MidiInput::MidiInput(core::Logger& logger)
    : impl_(new Impl(logger)) {}

MidiInput::~MidiInput() {
    stop();
    delete impl_;
}

bool MidiInput::start(MessageHandler handler) {
    return impl_->start(std::move(handler));
}

void MidiInput::stop() {
    impl_->stop();
}

}  // namespace synth::midi
