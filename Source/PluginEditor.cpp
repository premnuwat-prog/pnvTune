#include "PluginEditor.h"
namespace {
const juce::Colour bg{0xff101313},panel{0xff191e1d},line{0xff303735},ink{0xffeff5ef},muted{0xff8b9891},lime{0xffc2f970};
void text(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,float size,juce::Colour c,juce::Justification j=juce::Justification::centredLeft) {
    g.setColour(c); g.setFont(juce::Font(juce::FontOptions(size))); g.drawText(s,r,j);
}
juce::String noteName(float midi) {
    if(midi<0) return "--";
    int n=(int)std::round(midi);
    return juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}[(n%12+12)%12]+juce::String(n/12-1);
}
juce::String chordId(int index,const char* suffix) { return (index==0?"chord":"chord"+juce::String(index+1))+suffix; }
}
PremLook::PremLook() {
    setColour(juce::ComboBox::backgroundColourId,panel); setColour(juce::ComboBox::outlineColourId,line);
    setColour(juce::ComboBox::textColourId,ink); setColour(juce::ComboBox::arrowColourId,lime);
    setColour(juce::PopupMenu::backgroundColourId,panel); setColour(juce::PopupMenu::textColourId,ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,lime); setColour(juce::PopupMenu::highlightedTextColourId,bg);
    setColour(juce::Slider::textBoxTextColourId,ink); setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
    setColour(juce::Slider::trackColourId,line); setColour(juce::Slider::thumbColourId,lime);
    setColour(juce::TextButton::buttonColourId,panel); setColour(juce::TextButton::buttonOnColourId,lime);
    setColour(juce::TextButton::textColourOffId,muted); setColour(juce::TextButton::textColourOnId,bg);
}
void PremLook::drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float value,float start,float end,juce::Slider&) {
    auto r=juce::Rectangle<float>((float)x,(float)y,(float)w,(float)h).reduced(10);
    const float radius=std::min(r.getWidth(),r.getHeight())*0.5f;
    const auto centre=r.getCentre(); const float angle=start+value*(end-start);
    juce::Path track,fill;
    track.addCentredArc(centre.x,centre.y,radius,radius,0,start,end,true);
    fill.addCentredArc(centre.x,centre.y,radius,radius,0,start,angle,true);
    g.setColour(line); g.strokePath(track,juce::PathStrokeType(4,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(lime); g.strokePath(fill,juce::PathStrokeType(4,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xff242b28)); g.fillEllipse(centre.x-radius+9,centre.y-radius+9,2*(radius-9),2*(radius-9));
    const float tip=radius-15;
    g.setColour(ink); g.drawLine(centre.x+std::sin(angle)*tip*0.6f,centre.y-std::cos(angle)*tip*0.6f,centre.x+std::sin(angle)*tip,centre.y-std::cos(angle)*tip,2.5f);
}
PremTuneEditor::PremTuneEditor(PremTuneProcessor& p):AudioProcessorEditor(p),processor(p) {
    setLookAndFeel(&look); setSize(840,620);
    key.addItemList({"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},1);
    scale.addItemList({"Chromatic","Major","Natural Minor","Harmonic Minor","Major Pentatonic"},1);
    preset.addItemList({"Live Vocal","Hard Tune","Natural"},1); preset.setSelectedId(p.getCurrentProgram()+1,juce::dontSendNotification);
    preset.onChange=[this]{processor.setCurrentProgram(preset.getSelectedId()-1);};
    for(auto* c:{&key,&scale,&preset}) addAndMakeVisible(c);
    keyAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"key",key);
    scaleAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,"scale",scale);
    visibleChords=(int)p.state.state.getProperty("visibleChords",-1);
    if(visibleChords<0) {
        visibleChords=0;
        for(int i=0;i<5;++i) if(p.state.getRawParameterValue(chordId(i,"Enabled"))->load()>0.5f) visibleChords=i+1;
    }
    visibleChords=juce::jlimit(0,5,visibleChords);
    for(size_t i=0;i<5;++i) {
        chordRoot[i].addItemList({"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},1);
        chordQuality[i].addItemList({"Major","Minor","7","Maj7","m7","sus2","sus4","dim"},1);
        chordToggle[i].setButtonText(juce::String((int)i+1)); chordToggle[i].setClickingTogglesState(true);
        chordToggle[i].setTooltip("Add this chord's tones to the scale. Automate each numbered switch for its song section.");
        addAndMakeVisible(chordRoot[i]); addAndMakeVisible(chordQuality[i]); addAndMakeVisible(chordToggle[i]);
        chordRootAttachment[i]=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,chordId((int)i,"Root"),chordRoot[i]);
        chordQualityAttachment[i]=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.state,chordId((int)i,"Quality"),chordQuality[i]);
        chordToggleAttachment[i]=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,chordId((int)i,"Enabled"),chordToggle[i]);
    }
    bypass.setClickingTogglesState(true); addAndMakeVisible(bypass);
    bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state,"bypass",bypass);
    vocalGate.setSliderStyle(juce::Slider::LinearHorizontal);vocalGate.setTextBoxStyle(juce::Slider::TextBoxRight,false,62,22);
    vocalGate.setTextValueSuffix(" dB");vocalGate.setTooltip("Raise this until guitar bleed no longer triggers tuning, then lower it slightly so quiet vocal words still open the gate.");
    addAndMakeVisible(vocalGate);vocalGateAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,"vocalGate",vocalGate);
    addChord.setTooltip("Reveal and enable one borrowed chord slot, up to five."); addAndMakeVisible(addChord);
    removeChord.setTooltip("Remove the last borrowed chord slot."); addAndMakeVisible(removeChord);
    addChord.onClick=[this]{
        if(visibleChords>=5)return;
        auto* parameter=processor.state.getParameter(chordId(visibleChords,"Enabled"));
        parameter->beginChangeGesture();parameter->setValueNotifyingHost(1);parameter->endChangeGesture();
        ++visibleChords;processor.state.state.setProperty("visibleChords",visibleChords,nullptr);updateChordVisibility();
    };
    removeChord.onClick=[this]{
        if(visibleChords<=0)return;
        --visibleChords;
        auto* parameter=processor.state.getParameter(chordId(visibleChords,"Enabled"));
        parameter->beginChangeGesture();parameter->setValueNotifyingHost(0);parameter->endChangeGesture();
        processor.state.state.setProperty("visibleChords",visibleChords,nullptr);updateChordVisibility();
    };
    const char* ids[]={"retune","amount","humanize","mix","output"};
    const char* suffixes[]={" ms"," %"," %"," %"," dB"};
    const char* tips[]={"How quickly the vocal moves to the target note. 0 ms gives the strongest hard-tune effect.","Strength of pitch correction.","Preserve up to 20 cents of natural pitch movement near the target note.","Blend corrected and latency-aligned original vocal.","Output gain. Reduce this if your track clips."};
    for(size_t i=0;i<knobs.size();++i) {
        auto& k=knobs[i]; k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,112,24); k.setTextValueSuffix(suffixes[i]);
        k.setTooltip(tips[i]); k.setName(ids[i]); addAndMakeVisible(k);
        sliderAttachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state,ids[i],k);
    }
    pitchHistory.fill(-1); targetHistory.fill(-1); updateChordVisibility(); startTimerHz(24);
}
PremTuneEditor::~PremTuneEditor(){stopTimer();setLookAndFeel(nullptr);}
void PremTuneEditor::updateChordVisibility(){
    for(int i=0;i<5;++i){const bool shown=i<visibleChords;chordRoot[(size_t)i].setVisible(shown);chordQuality[(size_t)i].setVisible(shown);chordToggle[(size_t)i].setVisible(shown);}
    addChord.setEnabled(visibleChords<5);removeChord.setVisible(visibleChords>0);resized();repaint();
}
void PremTuneEditor::resized(){
    preset.setBounds(544,24,154,32); bypass.setBounds(712,24,100,32);
    key.setBounds(42,112,84,34); scale.setBounds(138,112,220,34);
    addChord.setBounds(42,162,130,30); removeChord.setBounds(180,162,34,30);
    for(int i=0;i<5;++i) {
        const int y=201+i*35;
        chordToggle[(size_t)i].setBounds(42,y,38,29);
        chordRoot[(size_t)i].setBounds(88,y,72,29);
        chordQuality[(size_t)i].setBounds(168,y,98,29);
    }
    for(int i=0;i<5;++i) knobs[(size_t)i].setBounds(24+i*158,424,140,128);
    vocalGate.setBounds(420,582,145,24);
}
void PremTuneEditor::timerCallback(){
    hz=processor.detected.load(); note=processor.target.load(); correction=processor.cents.load();
    voiceOpen=processor.gateOpen.load()>0.5f;
    meter=std::max(processor.level.load(),meter*0.84f);
    pitchHistory[(size_t)historyWrite]=hz>0?(float)(69+12*std::log2(hz/440.0)):-1;
    targetHistory[(size_t)historyWrite]=note; historyWrite=(historyWrite+1)%(int)pitchHistory.size();
    preset.setSelectedId(processor.getCurrentProgram()+1,juce::dontSendNotification);
    repaint();
}
void PremTuneEditor::paint(juce::Graphics& g){
    g.fillAll(bg);
    text(g,"pnv",{28,14,72,48},31,ink); text(g,"Tune",{91,14,94,48},31,lime);
    text(g,"LIVE PITCH",{200,23,130,30},10,muted);
    g.setColour(line);g.drawHorizontalLine(76,28,812);
    g.setColour(panel);g.fillRoundedRectangle(28,92,350,(float)(105+visibleChords*35),10);
    text(g,"KEY",{42,92,84,20},9,muted);text(g,"SCALE",{138,92,180,20},9,muted);
    text(g,"AUTOMATABLE",{226,166,120,22},8,muted);
    const int keyValue=(int)processor.state.getRawParameterValue("key")->load();
    const int scaleValue=(int)processor.state.getRawParameterValue("scale")->load();
    for(int chord=0;chord<5;++chord) {
        if(processor.state.getRawParameterValue(chordId(chord,"Enabled"))->load()<0.5f) continue;
        const int root=(int)processor.state.getRawParameterValue(chordId(chord,"Root"))->load();
        const int quality=(int)processor.state.getRawParameterValue(chordId(chord,"Quality"))->load();
        const int added=prem::chordMask(root,quality)&~prem::scaleMask(keyValue,scaleValue);
        juce::String names;
        for(int n=0;n<12;++n) if(added&(1<<n)) names+=(names.isEmpty()?"":" ")+juce::StringArray{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}[n];
        text(g,names.isEmpty()?"-":"+ "+names,{278,206+chord*35,82,20},9,names.isEmpty()?muted:lime);
    }
    text(g,"IN",{438,100,80,18},9,muted);text(g,"TO",{566,100,80,18},9,muted);text(g,"SHIFT",{694,100,100,18},9,muted);
    text(g,hz>0?noteName((float)(69+12*std::log2(hz/440.0))):"--",{436,119,110,44},31,ink);
    text(g,noteName(note),{564,119,110,44},31,lime);
    text(g,(correction>0?"+":"")+juce::String((int)std::round(correction))+" ct",{692,123,116,38},21,ink);
    const juce::Rectangle<float> graph(420,178,392,184);
    g.setColour(panel);g.fillRoundedRectangle(graph,12);
    text(g,"PITCH",{436,185,70,20},9,muted);
    text(g,"IN",{714,185,30,20},9,ink);text(g,"TO",{766,185,30,20},9,lime);
    float centre=note>=0?note:60;
    for(int j=-2;j<=2;++j){g.setColour(line.withAlpha(0.55f));const float y=278-j*27.0f;g.drawHorizontalLine((int)y,436,796);}
    for(int series=0;series<2;++series){
        juce::Path path;bool pen=false;
        for(size_t j=0;j<pitchHistory.size();++j){
            size_t index=((size_t)historyWrite+j)%pitchHistory.size();
            float v=series==0?pitchHistory[index]:targetHistory[index];
            if(v<0){pen=false;continue;}
            float x=436+(float)j/(pitchHistory.size()-1)*360;
            float y=juce::jlimit(215.0f,344.0f,278-(v-centre)*27);
            if(!pen)path.startNewSubPath(x,y);else path.lineTo(x,y);pen=true;
        }
        g.setColour(series==0?ink.withAlpha(0.65f):lime);g.strokePath(path,juce::PathStrokeType(series==0?1.5f:2.0f));
    }
    if(hz<=0)text(g,"SING",{556,258,100,36},13,muted,juce::Justification::centred);
    const char* names[]={"RETUNE","AMOUNT","HUMANIZE","MIX","OUTPUT"};
    for(int i=0;i<5;++i)text(g,names[i],{24+i*158,402,140,22},10,muted,juce::Justification::centred);
    g.setColour(line);g.drawHorizontalLine(574,28,812);
    g.setColour(processor.state.getRawParameterValue("bypass")->load()>0.5f?muted:lime);g.fillEllipse(29,594,6,6);
    text(g,"12 ms",{44,585,70,24},10,muted);
    text(g,"90-1000 Hz",{132,585,100,24},10,muted);
    g.setColour(voiceOpen?lime:muted);g.fillEllipse(326,592,7,7);
    text(g,"VOCAL GATE",{339,585,78,24},9,muted);
    text(g,"IN",{600,585,24,24},9,muted);
    g.setColour(line);g.fillRoundedRectangle(630,593,102,7,3);
    g.setColour(meter>0.8f?juce::Colour(0xffff9f7d):lime);
    const float db=juce::Decibels::gainToDecibels(meter,-60.0f);
    g.fillRoundedRectangle(630,593,102*juce::jlimit(0.0f,1.0f,(db+60)/60),7,3);
    text(g,"v0.4.1",{757,585,55,24},9,muted,juce::Justification::centredRight);
}
