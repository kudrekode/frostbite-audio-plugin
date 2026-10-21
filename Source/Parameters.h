#pragma once
#include "dsp/Engine.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace frostbite {
struct ParameterInfo {
    const char* id; const char* name; const char* unit;
    float min, max, step, skew;
};
extern const std::array<ParameterInfo,Count> parameterInfo;
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
struct FactoryPreset { const char* name; const char* description; Values values; };
const std::array<FactoryPreset,8>& factoryPresets();
}
