#include "PluginProcessor.h"
#include "PluginEditor.h"
juce::AudioProcessorValueTreeState::ParameterLayout PremTuneProcessor::makeParameters() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{"key",1}, "Key", StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, 0));
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{"scale",1}, "Scale", StringArray{"Chromatic","Major","Natural Minor","Harmonic Minor","Major Pentatonic"}, 0));
    for (int chord = 1; chord <= 5; ++chord) {
        const String number(chord);
        const String prefix=chord==1?"chord":"chord"+number;
        p.add(std::make_unique<AudioParameterBool>(ParameterID{prefix+"Enabled",1}, "Chord "+number+" On",false));
        p.add(std::make_unique<AudioParameterChoice>(ParameterID{prefix+"Root",1}, "Chord "+number+" Root", StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}, chord == 1 ? 4 : 0));
        p.add(std::make_unique<AudioParameterChoice>(ParameterID{prefix+"Quality",1}, "Chord "+number+" Type", StringArray{"Major","Minor","7","Maj7","m7","sus2","sus4","dim"}, 0));
    }
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"retune",1}, "Retune", NormalisableRange<float>(0,200,0.1f,0.45f),12, AudioParameterFloatAttributes().withLabel("ms")));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"amount",1}, "Correction", NormalisableRange<float>(0,100,1),100, AudioParameterFloatAttributes().withLabel("%")));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"humanize",1}, "Humanize", NormalisableRange<float>(0,100,1),10, AudioParameterFloatAttributes().withLabel("%")));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"mix",1}, "Mix", NormalisableRange<float>(0,100,1),100, AudioParameterFloatAttributes().withLabel("%")));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"output",1}, "Output", NormalisableRange<float>(-18,12,0.1f),0, AudioParameterFloatAttributes().withLabel("dB")));
    p.add(std::make_unique<AudioParameterFloat>(ParameterID{"vocalGate",1}, "Vocal Gate", NormalisableRange<float>(-60,-18,1),-42, AudioParameterFloatAttributes().withLabel("dB")));
    p.add(std::make_unique<AudioParameterBool>(ParameterID{"bypass",1}, "Bypass",false));
    return p;
}
PremTuneProcessor::PremTuneProcessor()
 : AudioProcessor(BusesProperties().withInput("Vocal",juce::AudioChannelSet::stereo(),true).withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   state(*this,nullptr,"PremTuneState",makeParameters()) {
    values[0]=state.getRawParameterValue("key"); values[1]=state.getRawParameterValue("scale");
    size_t index=2;
    for(int chord=1;chord<=5;++chord) for(const char* suffix:{"Enabled","Root","Quality"})
        values[index++]=state.getRawParameterValue((chord==1?"chord":"chord"+juce::String(chord))+suffix);
    for(const char* id:{"retune","amount","humanize","mix","output","vocalGate","bypass"}) values[index++]=state.getRawParameterValue(id);
}
void PremTuneProcessor::prepareToPlay(double rate,int) { engine.prepare(rate); setLatencySamples(engine.latencySamples()); }
bool PremTuneProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return (l.getMainInputChannelSet()==juce::AudioChannelSet::mono() || l.getMainInputChannelSet()==juce::AudioChannelSet::stereo())
        && l.getMainInputChannelSet()==l.getMainOutputChannelSet();
}
void PremTuneProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&) { process(b,false); }
void PremTuneProcessor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer&) { process(b,true); }
void PremTuneProcessor::process(juce::AudioBuffer<float>& b,bool hostBypass) {
    juce::ScopedNoDenormals noDenormals;
    for (int c=getTotalNumInputChannels();c<b.getNumChannels();++c) b.clear(c,0,b.getNumSamples());
    if (b.getNumChannels()==0) return;
    prem::Settings s;
    s.key=(int)values[0]->load(); s.scale=(int)values[1]->load();
    size_t index=2;
    for(size_t chord=0;chord<5;++chord) { s.chordEnabled[chord]=values[index++]->load()>0.5f; s.chordRoot[chord]=(int)values[index++]->load(); s.chordQuality[chord]=(int)values[index++]->load(); }
    s.retuneMs=values[index++]->load(); s.amount=values[index++]->load(); s.humanize=values[index++]->load(); s.mix=values[index++]->load();
    s.outputDb=hostBypass ? 0 : values[index++]->load(); s.vocalGateDb=values[index++]->load(); s.bypass=hostBypass || values[index]->load()>0.5f;
    if(s.bypass) s.outputDb=0;
    level.store(b.getRMSLevel(0,0,b.getNumSamples()),std::memory_order_relaxed);
    engine.process(b.getArrayOfWritePointers(),std::min(2,b.getNumChannels()),b.getNumSamples(),s);
    detected.store(engine.frequency,std::memory_order_relaxed); target.store(engine.targetMidi,std::memory_order_relaxed);
    cents.store(engine.correctionCents,std::memory_order_relaxed);
    gateOpen.store(engine.vocalGateOpen?1.0f:0.0f,std::memory_order_relaxed);
}
const juce::String PremTuneProcessor::getProgramName(int i) { return juce::StringArray{"Live Vocal","Hard Tune","Natural"}[juce::jlimit(0,2,i)]; }
void PremTuneProcessor::setCurrentProgram(int i) {
    currentProgram.store(juce::jlimit(0,2,i));
    const float settings[3][5]={{12,100,10,100,0},{0,100,0,100,0},{65,85,65,100,0}};
    const char* ids[]={"retune","amount","humanize","mix","output"};
    for(int j=0;j<5;++j) { auto* p=state.getParameter(ids[j]); p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(settings[currentProgram.load()][j])); p->endChangeGesture(); }
}
void PremTuneProcessor::getStateInformation(juce::MemoryBlock& dest) { auto tree=state.copyState(); tree.setProperty("program",currentProgram.load(),nullptr); auto xml=tree.createXml(); copyXmlToBinary(*xml,dest); }
void PremTuneProcessor::setStateInformation(const void* data,int size) { auto xml=getXmlFromBinary(data,size); if(xml && xml->hasTagName(state.state.getType())) { auto tree=juce::ValueTree::fromXml(*xml); currentProgram.store(juce::jlimit(0,2,(int)tree.getProperty("program",0))); state.replaceState(tree); } }
juce::AudioProcessorEditor* PremTuneProcessor::createEditor() { return new PremTuneEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PremTuneProcessor(); }
