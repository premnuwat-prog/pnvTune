#include "PluginProcessor.h"
#include <iostream>
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI gui;
    PremTuneProcessor p;p.prepareToPlay(48000,64);
    p.setCurrentProgram(1);
    p.state.getParameter("key")->setValueNotifyingHost(p.state.getParameter("key")->convertTo0to1(7));
    p.state.getParameter("scale")->setValueNotifyingHost(p.state.getParameter("scale")->convertTo0to1(1));
    p.state.getParameter("chordEnabled")->setValueNotifyingHost(1);
    p.state.getParameter("chordRoot")->setValueNotifyingHost(p.state.getParameter("chordRoot")->convertTo0to1(4));
    p.state.getParameter("chord5Enabled")->setValueNotifyingHost(1);
    p.state.getParameter("chord5Root")->setValueNotifyingHost(p.state.getParameter("chord5Root")->convertTo0to1(11));
    p.state.getParameter("vocalGate")->setValueNotifyingHost(p.state.getParameter("vocalGate")->convertTo0to1(-35));
    juce::MemoryBlock saved;p.getStateInformation(saved);
    PremTuneProcessor restored;restored.setStateInformation(saved.getData(),(int)saved.getSize());
    if(restored.state.getRawParameterValue("key")->load()!=7 || restored.state.getRawParameterValue("scale")->load()!=1
       || restored.state.getRawParameterValue("chordEnabled")->load()!=1 || restored.state.getRawParameterValue("chordRoot")->load()!=4
       || restored.state.getRawParameterValue("chord5Enabled")->load()!=1 || restored.state.getRawParameterValue("chord5Root")->load()!=11
       || restored.state.getRawParameterValue("vocalGate")->load()!=-35
       || restored.state.getRawParameterValue("retune")->load()!=0 || restored.getCurrentProgram()!=1)return 1;
    if(p.getLatencySamples()!=576)return 2;
    if(argc>1){
        for(const char* id:{"chordEnabled","chord5Enabled"})p.state.getParameter(id)->setValueNotifyingHost(0);
        p.state.state.setProperty("visibleChords",0,nullptr);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        auto snapshot=editor->createComponentSnapshot(editor->getLocalBounds(),true,2.0f);
        juce::File file=juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
        file.deleteFile();juce::FileOutputStream stream(file);juce::PNGImageFormat png;
        if(!png.writeImageToStream(snapshot,stream))return 3;
    }
    std::cout<<"PASS: parameter state, preset recall, host latency, editor render\n";
}
