#include "synth/audio/SampleLoader.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

#if defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace synth::audio {

namespace {

std::string displayNameFromPath(const std::filesystem::path& path) {
    const std::string stem = path.stem().string();
    return stem.empty() ? path.filename().string() : stem;
}

#if defined(__APPLE__)

std::string osStatusMessage(OSStatus status) {
    return "Audio file decode failed with OSStatus " + std::to_string(static_cast<long>(status)) + ".";
}

#endif

}  // namespace

SampleLoadResult loadSampleFile(const std::filesystem::path& path) {
    SampleLoadResult result;

    if (path.empty()) {
        return result;
    }

    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError) || existsError) {
        result.errorMessage = "Sample file does not exist.";
        return result;
    }

#if defined(__APPLE__)
    const std::string pathString = path.string();
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(pathString.data()),
        static_cast<CFIndex>(pathString.size()),
        false);
    if (url == nullptr) {
        result.errorMessage = "Could not open sample path.";
        return result;
    }

    ExtAudioFileRef audioFile = nullptr;
    OSStatus status = ExtAudioFileOpenURL(url, &audioFile);
    CFRelease(url);
    if (status != noErr || audioFile == nullptr) {
        result.errorMessage = osStatusMessage(status);
        return result;
    }

    AudioStreamBasicDescription sourceFormat{};
    UInt32 propertySize = sizeof(sourceFormat);
    status = ExtAudioFileGetProperty(
        audioFile,
        kExtAudioFileProperty_FileDataFormat,
        &propertySize,
        &sourceFormat);
    if (status != noErr || sourceFormat.mChannelsPerFrame == 0 || sourceFormat.mSampleRate <= 0.0) {
        ExtAudioFileDispose(audioFile);
        result.errorMessage = status == noErr ? "Sample file has an unsupported format." : osStatusMessage(status);
        return result;
    }

    const UInt32 sourceChannelCount = sourceFormat.mChannelsPerFrame;
    AudioStreamBasicDescription clientFormat{};
    clientFormat.mSampleRate = sourceFormat.mSampleRate;
    clientFormat.mFormatID = kAudioFormatLinearPCM;
    clientFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
    clientFormat.mBytesPerPacket = sizeof(float) * sourceChannelCount;
    clientFormat.mFramesPerPacket = 1;
    clientFormat.mBytesPerFrame = sizeof(float) * sourceChannelCount;
    clientFormat.mChannelsPerFrame = sourceChannelCount;
    clientFormat.mBitsPerChannel = 32;

    status = ExtAudioFileSetProperty(
        audioFile,
        kExtAudioFileProperty_ClientDataFormat,
        sizeof(clientFormat),
        &clientFormat);
    if (status != noErr) {
        ExtAudioFileDispose(audioFile);
        result.errorMessage = osStatusMessage(status);
        return result;
    }

    SInt64 fileFrameCount = 0;
    propertySize = sizeof(fileFrameCount);
    if (ExtAudioFileGetProperty(audioFile, kExtAudioFileProperty_FileLengthFrames, &propertySize, &fileFrameCount)
        != noErr) {
        fileFrameCount = 0;
    }

    std::vector<float> monoSamples;
    if (fileFrameCount > 0) {
        monoSamples.reserve(static_cast<std::size_t>(fileFrameCount));
    }

    constexpr UInt32 kChunkFrames = 4096;
    std::vector<float> interleaved(static_cast<std::size_t>(kChunkFrames) * sourceChannelCount, 0.0f);

    while (true) {
        UInt32 framesToRead = kChunkFrames;
        AudioBufferList bufferList{};
        bufferList.mNumberBuffers = 1;
        bufferList.mBuffers[0].mNumberChannels = sourceChannelCount;
        bufferList.mBuffers[0].mDataByteSize =
            static_cast<UInt32>(interleaved.size() * sizeof(float));
        bufferList.mBuffers[0].mData = interleaved.data();

        status = ExtAudioFileRead(audioFile, &framesToRead, &bufferList);
        if (status != noErr) {
            ExtAudioFileDispose(audioFile);
            result.errorMessage = osStatusMessage(status);
            return result;
        }
        if (framesToRead == 0) {
            break;
        }

        for (UInt32 frame = 0; frame < framesToRead; ++frame) {
            float mixed = 0.0f;
            const std::size_t frameOffset = static_cast<std::size_t>(frame) * sourceChannelCount;
            for (UInt32 channel = 0; channel < sourceChannelCount; ++channel) {
                mixed += interleaved[frameOffset + channel];
            }
            monoSamples.push_back(mixed / static_cast<float>(sourceChannelCount));
        }
    }

    ExtAudioFileDispose(audioFile);

    if (monoSamples.empty()) {
        result.errorMessage = "Sample file contains no audio frames.";
        return result;
    }

    auto buffer = std::make_shared<SampleBuffer>();
    buffer->sampleRate = sourceFormat.mSampleRate;
    buffer->samples = std::move(monoSamples);
    buffer->sourcePath = pathString;
    buffer->displayName = displayNameFromPath(path);
    result.buffer = std::move(buffer);
    return result;
#else
    result.errorMessage = "Sample loading is only implemented on macOS.";
    return result;
#endif
}

}  // namespace synth::audio
