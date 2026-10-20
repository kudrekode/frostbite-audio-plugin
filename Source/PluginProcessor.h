#pragma once
#include "Parameters.h"
#include <atomic>

class FrostbiteAudioProcessor final : public juce::AudioProcessor {
public:
    FrostbiteAudioProcessor();
    ~FrostbiteAudioProcessor() override=default;
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "FROSTBITE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 120; }
    int getNumPrograms() override { return 8; }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    frostbite::Values values() const;
    void applyValues(const frostbite::Values&);
    juce::String presetName() const;
    juce::Result savePreset(const juce::File&);
    juce::Result loadPreset(const juce::File&);
    void restoreDefaults();
    juce::File presetDirectory() const;
    juce::AudioProcessorValueTreeState state;
    std::atomic<float> inputMeter{0},outputMeter{0};
    std::atomic<bool> captured{false};
    std::array<std::atomic<float>,128> scope{};
    std::atomic<unsigned> scopeHead{0};
    std::atomic<unsigned> presetRevision{0};
private:
    frostbite::Engine engine;
    std::array<std::atomic<float>*,frostbite::Count> raw{};
    std::atomic<int> currentProgram{0};
    unsigned scopeCounter=0,scopeIndex=0;
    void process(juce::AudioBuffer<float>&,bool);
    bool restoreTree(juce::ValueTree);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrostbiteAudioProcessor)
};
