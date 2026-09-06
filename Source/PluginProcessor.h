#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PitchEngine.h"
class PremTuneProcessor final : public juce::AudioProcessor {
public:
    PremTuneProcessor();
    const juce::String getName() const override { return "pnvTune"; }
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    double getTailLengthSeconds() const override { return 0.04; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override { return 3; }
    int getCurrentProgram() override { return currentProgram.load(); }
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState state;
    std::atomic<float> detected{0}, target{-1}, cents{0}, level{0}, gateOpen{0};
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    void process(juce::AudioBuffer<float>&, bool);
    prem::PitchEngine engine;
    std::array<std::atomic<float>*, 24> values;
    std::atomic<int> currentProgram{0};
};
