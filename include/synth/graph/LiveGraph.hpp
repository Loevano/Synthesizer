#pragma once

#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

namespace synth::graph {

class LiveGraph {
public:
    struct SourceNode {
        std::string_view name;
        std::function<void(double, std::uint32_t)> prepare;
        std::function<bool()> isImplemented;
        std::function<bool()> isEnabled;
        std::function<void(float*, std::uint32_t, std::uint32_t)> renderAdd;
        std::function<void(int, float)> noteOn;
        std::function<void(int)> noteOff;
    };

    struct OutputProcessorNode {
        std::string_view name;
        std::function<void(double, std::uint32_t)> prepare;
        std::function<void(float*, std::uint32_t, std::uint32_t)> process;
    };

    void clear();
    void addSourceNode(SourceNode node);
    void addOutputProcessorNode(OutputProcessorNode node);
    void prepare(double sampleRate, std::uint32_t outputChannels);
    void render(float* output, std::uint32_t frames, std::uint32_t channels);
    void noteOn(int noteNumber, float velocity);
    void noteOff(int noteNumber);

    std::vector<std::string_view> sourceOrder() const;
    std::vector<std::string_view> activeSourceNames() const;
    std::vector<std::string_view> outputProcessorOrder() const;

private:
    std::vector<SourceNode> sourceNodes_;
    std::vector<OutputProcessorNode> outputProcessorNodes_;
};

}  // namespace synth::graph
