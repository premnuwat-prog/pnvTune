#pragma once
#include "PluginProcessor.h"
class PremLook final : public juce::LookAndFeel_V4 {
public:
    PremLook();
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
};
class PremTuneEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit PremTuneEditor(PremTuneProcessor&);
    ~PremTuneEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void updateChordVisibility();
    PremTuneProcessor& processor;
    PremLook look;
    juce::ComboBox key,scale,preset;
    std::array<juce::ComboBox,5> chordRoot,chordQuality;
    std::array<juce::TextButton,5> chordToggle;
    juce::TextButton addChord{"+  ADD CHORD"},removeChord{"X"},bypass{"BYPASS"};
    juce::Slider vocalGate;
    std::array<juce::Slider,5> knobs;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,5> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttachment,scaleAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>,5> chordRootAttachment,chordQualityAttachment;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>,5> chordToggleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vocalGateAttachment;
    std::array<float,150> pitchHistory{},targetHistory{};
    int historyWrite=0;
    int visibleChords=0;
    float hz=0,note=-1,correction=0,meter=0;
    bool voiceOpen=false;
};
