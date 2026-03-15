#include "synth/interfaces/IAudioDriver.hpp"

#include "synth/core/Logger.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#if defined(SYNTH_PLATFORM_MACOS)
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace synth::interfaces {

#if defined(SYNTH_PLATFORM_MACOS)

namespace {

struct OutputDeviceDescriptor {
    AudioDeviceID deviceId = kAudioObjectUnknown;
    OutputDeviceInfo info;
};

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

AudioDeviceID queryDefaultOutputDeviceId() {
    AudioDeviceID deviceId = kAudioObjectUnknown;
    UInt32 size = sizeof(deviceId);
    AudioObjectPropertyAddress address{
        kAudioHardwarePropertyDefaultOutputDevice,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    if (AudioObjectGetPropertyData(
            kAudioObjectSystemObject,
            &address,
            0,
            nullptr,
            &size,
            &deviceId) != noErr) {
        return kAudioObjectUnknown;
    }

    return deviceId;
}

std::string queryDeviceName(AudioDeviceID deviceId) {
    CFStringRef value = nullptr;
    UInt32 size = sizeof(value);
    AudioObjectPropertyAddress address{
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size, &value) != noErr || value == nullptr) {
        return {};
    }

    const std::string result = copyCfString(value);
    CFRelease(value);
    return result;
}

std::string queryDeviceUid(AudioDeviceID deviceId) {
    CFStringRef value = nullptr;
    UInt32 size = sizeof(value);
    AudioObjectPropertyAddress address{
        kAudioDevicePropertyDeviceUID,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size, &value) != noErr || value == nullptr) {
        return {};
    }

    const std::string result = copyCfString(value);
    CFRelease(value);
    return result;
}

std::uint32_t queryOutputChannelCount(AudioDeviceID deviceId) {
    AudioObjectPropertyAddress address{
        kAudioDevicePropertyStreamConfiguration,
        kAudioDevicePropertyScopeOutput,
        kAudioObjectPropertyElementMain
    };

    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(deviceId, &address, 0, nullptr, &size) != noErr || size == 0) {
        return 0;
    }

    std::vector<std::byte> storage(size);
    auto* bufferList = reinterpret_cast<AudioBufferList*>(storage.data());
    if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size, bufferList) != noErr) {
        return 0;
    }

    std::uint32_t channelCount = 0;
    for (UInt32 bufferIndex = 0; bufferIndex < bufferList->mNumberBuffers; ++bufferIndex) {
        channelCount += bufferList->mBuffers[bufferIndex].mNumberChannels;
    }

    return channelCount;
}

std::vector<OutputDeviceDescriptor> queryOutputDeviceDescriptors() {
    AudioObjectPropertyAddress address{
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain
    };

    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, nullptr, &size) != noErr || size == 0) {
        return {};
    }

    std::vector<AudioDeviceID> deviceIds(size / sizeof(AudioDeviceID), kAudioObjectUnknown);
    if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, nullptr, &size, deviceIds.data()) != noErr) {
        return {};
    }

    const AudioDeviceID defaultOutputDeviceId = queryDefaultOutputDeviceId();
    std::vector<OutputDeviceDescriptor> devices;
    devices.reserve(deviceIds.size());

    for (const AudioDeviceID deviceId : deviceIds) {
        const std::uint32_t outputChannels = queryOutputChannelCount(deviceId);
        if (outputChannels == 0) {
            continue;
        }

        OutputDeviceDescriptor descriptor;
        descriptor.deviceId = deviceId;
        descriptor.info.id = queryDeviceUid(deviceId);
        if (descriptor.info.id.empty()) {
            descriptor.info.id = std::to_string(static_cast<unsigned int>(deviceId));
        }
        descriptor.info.name = queryDeviceName(deviceId);
        if (descriptor.info.name.empty()) {
            descriptor.info.name = "Output Device " + std::to_string(static_cast<unsigned int>(deviceId));
        }
        descriptor.info.outputChannels = outputChannels;
        descriptor.info.isDefault = deviceId == defaultOutputDeviceId;
        devices.push_back(std::move(descriptor));
    }

    std::sort(devices.begin(), devices.end(), [](const OutputDeviceDescriptor& left, const OutputDeviceDescriptor& right) {
        if (left.info.isDefault != right.info.isDefault) {
            return left.info.isDefault && !right.info.isDefault;
        }
        return left.info.name < right.info.name;
    });

    return devices;
}

const OutputDeviceDescriptor* findOutputDeviceDescriptor(
    const std::vector<OutputDeviceDescriptor>& devices,
    std::string_view outputDeviceId) {
    if (!outputDeviceId.empty()) {
        for (const auto& device : devices) {
            if (device.info.id == outputDeviceId) {
                return &device;
            }
        }
    }

    for (const auto& device : devices) {
        if (device.info.isDefault) {
            return &device;
        }
    }

    return devices.empty() ? nullptr : &devices.front();
}

}  // namespace

class CoreAudioDriver final : public IAudioDriver {
public:
    explicit CoreAudioDriver(core::Logger& logger)
        : logger_(logger) {}

    ~CoreAudioDriver() override {
        stop();
    }

    bool start(const AudioConfig& config, AudioCallback callback) override {
        if (running_) {
            return true;
        }

        // Store the synth callback and config so CoreAudio can trigger them later.
        callback_ = std::move(callback);
        config_ = config;

        const auto outputDevices = queryOutputDeviceDescriptors();
        const auto* selectedDevice = findOutputDeviceDescriptor(outputDevices, config.outputDeviceId);
        if (selectedDevice == nullptr) {
            logger_.error("CoreAudio: no output device available.");
            return false;
        }

        if (selectedDevice->info.outputChannels < config.channels) {
            logger_.error("CoreAudio: selected output device does not support the requested channel count.");
            return false;
        }

        AudioComponentDescription desc{};
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_HALOutput;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;

        AudioComponent component = AudioComponentFindNext(nullptr, &desc);
        if (component == nullptr) {
            logger_.error("CoreAudio: output component not found.");
            return false;
        }

        if (AudioComponentInstanceNew(component, &audioUnit_) != noErr) {
            logger_.error("CoreAudio: failed to create output instance.");
            return false;
        }

        UInt32 enableOutput = 1;
        if (AudioUnitSetProperty(
                audioUnit_,
                kAudioOutputUnitProperty_EnableIO,
                kAudioUnitScope_Output,
                0,
                &enableOutput,
                sizeof(enableOutput)) != noErr) {
            logger_.error("CoreAudio: failed to enable HAL output.");
            AudioComponentInstanceDispose(audioUnit_);
            audioUnit_ = nullptr;
            return false;
        }

        UInt32 disableInput = 0;
        (void)AudioUnitSetProperty(
            audioUnit_,
            kAudioOutputUnitProperty_EnableIO,
            kAudioUnitScope_Input,
            1,
            &disableInput,
            sizeof(disableInput));

        AudioDeviceID deviceId = selectedDevice->deviceId;
        if (AudioUnitSetProperty(
                audioUnit_,
                kAudioOutputUnitProperty_CurrentDevice,
                kAudioUnitScope_Global,
                0,
                &deviceId,
                sizeof(deviceId)) != noErr) {
            logger_.error("CoreAudio: failed to bind selected output device.");
            AudioComponentInstanceDispose(audioUnit_);
            audioUnit_ = nullptr;
            return false;
        }

        AudioStreamBasicDescription format{};
        format.mSampleRate = config_.sampleRate;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
        format.mFramesPerPacket = 1;
        format.mChannelsPerFrame = config_.channels;
        format.mBitsPerChannel = 32;
        format.mBytesPerFrame = sizeof(float) * format.mChannelsPerFrame;
        format.mBytesPerPacket = format.mBytesPerFrame;

        const OSStatus formatStatus = AudioUnitSetProperty(
            audioUnit_,
            kAudioUnitProperty_StreamFormat,
            kAudioUnitScope_Input,
            0,
            &format,
            sizeof(format));
        if (formatStatus != noErr) {
            logger_.error("CoreAudio: failed to set stream format.");
            AudioComponentInstanceDispose(audioUnit_);
            audioUnit_ = nullptr;
            return false;
        }

        // Register the static bridge function CoreAudio will call on every audio block.
        AURenderCallbackStruct callbackStruct{};
        callbackStruct.inputProc = &CoreAudioDriver::renderCallback;
        callbackStruct.inputProcRefCon = this;

        const OSStatus callbackStatus = AudioUnitSetProperty(
            audioUnit_,
            kAudioUnitProperty_SetRenderCallback,
            kAudioUnitScope_Input,
            0,
            &callbackStruct,
            sizeof(callbackStruct));
        if (callbackStatus != noErr) {
            logger_.error("CoreAudio: failed to set render callback.");
            AudioComponentInstanceDispose(audioUnit_);
            audioUnit_ = nullptr;
            return false;
        }

        if (AudioUnitInitialize(audioUnit_) != noErr) {
            logger_.error("CoreAudio: failed to initialize audio unit.");
            AudioComponentInstanceDispose(audioUnit_);
            audioUnit_ = nullptr;
            return false;
        }

        scratchBuffer_.assign(8192 * config_.channels, 0.0f);

        if (AudioOutputUnitStart(audioUnit_) != noErr) {
            logger_.error("CoreAudio: failed to start output unit.");
            AudioUnitUninitialize(audioUnit_);
            AudioComponentInstanceDispose(audioUnit_);
            audioUnit_ = nullptr;
            return false;
        }

        running_ = true;
        return true;
    }

    void stop() override {
        if (!running_) {
            return;
        }

        AudioOutputUnitStop(audioUnit_);
        AudioUnitUninitialize(audioUnit_);
        AudioComponentInstanceDispose(audioUnit_);

        audioUnit_ = nullptr;
        running_ = false;
        callback_ = nullptr;
    }

    bool isRunning() const override {
        return running_;
    }

    std::vector<OutputDeviceInfo> availableOutputDevices() const override {
        const auto descriptors = queryOutputDeviceDescriptors();
        std::vector<OutputDeviceInfo> devices;
        devices.reserve(descriptors.size());
        for (const auto& descriptor : descriptors) {
            devices.push_back(descriptor.info);
        }
        return devices;
    }

private:
    static OSStatus renderCallback(
        void* inRefCon,
        AudioUnitRenderActionFlags* /*ioActionFlags*/,
        const AudioTimeStamp* /*inTimeStamp*/,
        UInt32 /*inBusNumber*/,
        UInt32 inNumberFrames,
        AudioBufferList* ioData) {
        // CoreAudio gives us a generic pointer back; cast it to our driver instance.
        auto* self = static_cast<CoreAudioDriver*>(inRefCon);
        if (self == nullptr || ioData == nullptr) {
            return noErr;
        }

        // Forward the request into the instance method that adapts CoreAudio buffers.
        self->render(ioData, inNumberFrames);
        return noErr;
    }

    void render(AudioBufferList* ioData, std::uint32_t frames) {
        if (!callback_) {
            for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
                std::memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
            }
            return;
        }

        if (ioData->mNumberBuffers == 1) {
            // Fast path: CoreAudio already gave us one interleaved float buffer.
            auto* output = static_cast<float*>(ioData->mBuffers[0].mData);
            callback_(output, frames, config_.channels);
            return;
        }

        const std::size_t sampleCount = static_cast<std::size_t>(frames) * config_.channels;
        if (sampleCount > scratchBuffer_.size()) {
            for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
                std::memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
            }
            return;
        }

        // Slow path: render into a temporary interleaved buffer first...
        callback_(scratchBuffer_.data(), frames, config_.channels);

        // ...then copy each channel into CoreAudio's separate output buffers.
        const UInt32 outputChannels = std::min(ioData->mNumberBuffers, config_.channels);
        for (UInt32 channel = 0; channel < outputChannels; ++channel) {
            auto* channelBuffer = static_cast<float*>(ioData->mBuffers[channel].mData);
            for (UInt32 frame = 0; frame < frames; ++frame) {
                channelBuffer[frame] = scratchBuffer_[(frame * config_.channels) + channel];
            }
        }

        for (UInt32 channel = outputChannels; channel < ioData->mNumberBuffers; ++channel) {
            std::memset(ioData->mBuffers[channel].mData, 0, ioData->mBuffers[channel].mDataByteSize);
        }
    }

    core::Logger& logger_;
    AudioUnit audioUnit_ = nullptr;
    AudioCallback callback_;
    AudioConfig config_;
    bool running_ = false;
    std::vector<float> scratchBuffer_;
};

#else

class CoreAudioDriver final : public IAudioDriver {
public:
    explicit CoreAudioDriver(core::Logger& logger)
        : logger_(logger) {}

    bool start(const AudioConfig& /*config*/, AudioCallback /*callback*/) override {
        logger_.error("Audio driver is only implemented for macOS in this scaffold.");
        return false;
    }

    void stop() override {}

    bool isRunning() const override {
        return false;
    }

    std::vector<OutputDeviceInfo> availableOutputDevices() const override {
        return {};
    }

private:
    core::Logger& logger_;
};

#endif

std::unique_ptr<IAudioDriver> createAudioDriver(core::Logger& logger) {
    return std::make_unique<CoreAudioDriver>(logger);
}

}  // namespace synth::interfaces
