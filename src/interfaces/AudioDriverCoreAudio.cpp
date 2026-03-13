#include "synth/interfaces/IAudioDriver.hpp"

#include "synth/core/Logger.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#if defined(SYNTH_PLATFORM_MACOS)
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#endif

namespace synth::interfaces {

#if defined(SYNTH_PLATFORM_MACOS)

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

        AudioComponentDescription desc{};
        desc.componentType = kAudioUnitType_Output;
        desc.componentSubType = kAudioUnitSubType_DefaultOutput;
        desc.componentManufacturer = kAudioUnitManufacturer_Apple;

        AudioComponent component = AudioComponentFindNext(nullptr, &desc);
        if (component == nullptr) {
            desc.componentSubType = kAudioUnitSubType_HALOutput;
            component = AudioComponentFindNext(nullptr, &desc);
        }
        if (component == nullptr) {
            logger_.error("CoreAudio: output component not found.");
            return false;
        }

        if (AudioComponentInstanceNew(component, &audioUnit_) != noErr) {
            logger_.error("CoreAudio: failed to create output instance.");
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

private:
    core::Logger& logger_;
};

#endif

std::unique_ptr<IAudioDriver> createAudioDriver(core::Logger& logger) {
    return std::make_unique<CoreAudioDriver>(logger);
}

}  // namespace synth::interfaces
