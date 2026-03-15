#include "synth/graph/LiveGraph.hpp"

#include <algorithm>

namespace synth::graph {

void LiveGraph::clear() {
    sourceNodes_.clear();
    outputProcessorNodes_.clear();
}

void LiveGraph::addSourceNode(SourceNode node) {
    sourceNodes_.push_back(std::move(node));
}

void LiveGraph::addOutputProcessorNode(OutputProcessorNode node) {
    outputProcessorNodes_.push_back(std::move(node));
}

void LiveGraph::prepare(double sampleRate, std::uint32_t outputChannels) {
    for (auto& node : sourceNodes_) {
        if (node.prepare) {
            node.prepare(sampleRate, outputChannels);
        }
    }

    for (auto& node : outputProcessorNodes_) {
        if (node.prepare) {
            node.prepare(sampleRate, outputChannels);
        }
    }
}

void LiveGraph::render(float* output, std::uint32_t frames, std::uint32_t channels) {
    if (output == nullptr || channels == 0) {
        return;
    }

    std::fill(output, output + (static_cast<std::size_t>(frames) * channels), 0.0f);

    for (auto& node : sourceNodes_) {
        if (node.isImplemented && !node.isImplemented()) {
            continue;
        }
        if (node.isEnabled && !node.isEnabled()) {
            continue;
        }
        if (node.renderAdd) {
            node.renderAdd(output, frames, channels);
        }
    }

    for (auto& node : outputProcessorNodes_) {
        if (node.process) {
            node.process(output, frames, channels);
        }
    }
}

void LiveGraph::noteOn(int noteNumber, float velocity) {
    for (auto& node : sourceNodes_) {
        if (node.isImplemented && !node.isImplemented()) {
            continue;
        }
        if (node.isEnabled && !node.isEnabled()) {
            continue;
        }
        if (node.noteOn) {
            node.noteOn(noteNumber, velocity);
        }
    }
}

void LiveGraph::noteOff(int noteNumber) {
    for (auto& node : sourceNodes_) {
        if (node.isImplemented && !node.isImplemented()) {
            continue;
        }
        if (node.isEnabled && !node.isEnabled()) {
            continue;
        }
        if (node.noteOff) {
            node.noteOff(noteNumber);
        }
    }
}

std::vector<std::string_view> LiveGraph::sourceOrder() const {
    std::vector<std::string_view> names;
    names.reserve(sourceNodes_.size());
    for (const auto& node : sourceNodes_) {
        names.push_back(node.name);
    }
    return names;
}

std::vector<std::string_view> LiveGraph::activeSourceNames() const {
    std::vector<std::string_view> names;
    names.reserve(sourceNodes_.size());
    for (const auto& node : sourceNodes_) {
        if (node.isImplemented && !node.isImplemented()) {
            continue;
        }
        if (node.isEnabled && !node.isEnabled()) {
            continue;
        }
        names.push_back(node.name);
    }
    return names;
}

std::vector<std::string_view> LiveGraph::outputProcessorOrder() const {
    std::vector<std::string_view> names;
    names.reserve(outputProcessorNodes_.size());
    for (const auto& node : outputProcessorNodes_) {
        names.push_back(node.name);
    }
    return names;
}

}  // namespace synth::graph
