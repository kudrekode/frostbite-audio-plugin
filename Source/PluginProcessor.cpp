#include "PluginProcessor.h"
#include "PluginEditor.h"

FrostbiteAudioProcessor::FrostbiteAudioProcessor()
    :AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
        .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
    state(*this,nullptr,"FrostbiteState",frostbite::createParameterLayout()) {
    static_assert(std::atomic<float>::is_always_lock_free);
    for(size_t i=0;i<frostbite::Count;++i) raw[i]=state.getRawParameterValue(frostbite::parameterInfo[i].id);
    state.state.setProperty("presetName","Init",nullptr);
    for(auto& v:scope) v.store(0);
}
bool FrostbiteAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    return l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo()
        && (l.getMainInputChannelSet()==juce::AudioChannelSet::mono() || l.getMainInputChannelSet()==juce::AudioChannelSet::stereo());
}
void FrostbiteAudioProcessor::prepareToPlay(double sr,int block) {
    engine.setParameters(values()); engine.prepare(sr,block); setLatencySamples(0);
    scopeCounter=scopeIndex=0;
}
void FrostbiteAudioProcessor::reset() { engine.reset(); }
frostbite::Values FrostbiteAudioProcessor::values() const {
    frostbite::Values v{}; for(size_t i=0;i<v.size();++i) v[i]=raw[i]->load(std::memory_order_relaxed); return v;
}
void FrostbiteAudioProcessor::process(juce::AudioBuffer<float>& buffer,bool bypass) {
    juce::ScopedNoDenormals guard;
    if(buffer.getNumChannels()<2) return;
    if(getTotalNumInputChannels()==1) buffer.copyFrom(1,0,buffer,0,0,buffer.getNumSamples());
    for(int c=2;c<buffer.getNumChannels();++c) buffer.clear(c,0,buffer.getNumSamples());
    auto v=values(); if(bypass) v[frostbite::Bypass]=1;
    double tempo=120;
    if(auto* playhead=getPlayHead())
        if(auto pos=playhead->getPosition())
            if(auto bpm=pos->getBpm()) tempo=*bpm;
    engine.setParameters(v,tempo);
    engine.process(buffer.getWritePointer(0),buffer.getWritePointer(1),buffer.getNumSamples());
    inputMeter.store(engine.inputPeak(),std::memory_order_relaxed);
    outputMeter.store(engine.outputPeak(),std::memory_order_relaxed);
    captured.store(engine.hasCapture(),std::memory_order_relaxed);
    for(int n=0;n<buffer.getNumSamples();++n) if((scopeCounter++&127u)==0) {
        scope[scopeIndex].store((buffer.getSample(0,n)+buffer.getSample(1,n))*.5f,std::memory_order_relaxed);
        scopeIndex=(scopeIndex+1)%128;
    }
    scopeHead.store(scopeIndex,std::memory_order_release);
}
void FrostbiteAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& m) { m.clear(); process(b,false); }
void FrostbiteAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer& m) { m.clear(); process(b,true); }
juce::AudioProcessorParameter* FrostbiteAudioProcessor::getBypassParameter() const { return state.getParameter("bypass"); }
juce::AudioProcessorEditor* FrostbiteAudioProcessor::createEditor() { return new FrostbiteAudioProcessorEditor(*this); }
void FrostbiteAudioProcessor::applyValues(const frostbite::Values& valuesToApply) {
    const auto v=frostbite::sanitise(valuesToApply);
    for(size_t i=0;i<v.size();++i) {
        auto* p=state.getParameter(frostbite::parameterInfo[i].id);
        p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v[i])); p->endChangeGesture();
    }
}
void FrostbiteAudioProcessor::setCurrentProgram(int index) {
    if(index<0 || index>=getNumPrograms()) return;
    const auto& p=frostbite::factoryPresets()[size_t(index)];
    applyValues(p.values); currentProgram.store(index);
    state.state.setProperty("presetName",p.name,nullptr);
    presetRevision.fetch_add(1);
}
const juce::String FrostbiteAudioProcessor::getProgramName(int i) {
    return i>=0 && i<getNumPrograms()?frostbite::factoryPresets()[size_t(i)].name:juce::String{};
}
juce::String FrostbiteAudioProcessor::presetName() const { return state.state.getProperty("presetName","Init").toString(); }
void FrostbiteAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto tree=state.copyState();
    tree.setProperty("schema",2,nullptr); tree.setProperty("program",currentProgram.load(),nullptr);
    // State/presets contain settings, never recorded audio. Reopen unarmed.
    for(auto child:tree) if(child.getProperty("id").toString()=="capture") child.setProperty("value",0,nullptr);
    if(auto xml=tree.createXml()) copyXmlToBinary(*xml,dest);
}
bool FrostbiteAudioProcessor::restoreTree(juce::ValueTree tree) {
    if(!tree.isValid() || !tree.hasType("FrostbiteState")) return false;
    const int schema=int(tree.getProperty("schema",1));
    if(schema<1 || schema>2) return false;
    // Absence of explicit voicing ALWAYS means historical defaults, even in schema 2.
    auto v=frostbite::legacyDefaults();
    for(auto child:tree) {
        const auto id=child.getProperty("id").toString();
        for(size_t i=0;i<frostbite::Count;++i) if(id==frostbite::parameterInfo[i].id) v[i]=float(child.getProperty("value",v[i]));
    }
    v[frostbite::Capture]=0;
    applyValues(v);
    state.state.setProperty("presetName",tree.getProperty("presetName","User"),nullptr);
    currentProgram.store(juce::jlimit(0,getNumPrograms()-1,int(tree.getProperty("program",0))));
    presetRevision.fetch_add(1);
    return true;
}
void FrostbiteAudioProcessor::setStateInformation(const void* data,int size) {
    if(size<=0 || size>1024*1024) return;
    if(auto xml=getXmlFromBinary(data,size)) restoreTree(juce::ValueTree::fromXml(*xml));
}
juce::File FrostbiteAudioProcessor::presetDirectory() const {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Frostbite/Presets");
}
juce::Result FrostbiteAudioProcessor::savePreset(const juce::File& file) {
    auto tree=state.copyState(); tree.setProperty("schema",2,nullptr);
    tree.setProperty("program",currentProgram.load(),nullptr);
    tree.setProperty("presetName",file.getFileNameWithoutExtension(),nullptr);
    for(auto child:tree) if(child.getProperty("id").toString()=="capture") child.setProperty("value",0,nullptr);
    auto result=file.getParentDirectory().createDirectory(); if(result.failed()) return result;
    auto xml=tree.createXml();
    if(!xml || !file.replaceWithText(xml->toString())) return juce::Result::fail("Could not write preset.");
    state.state.setProperty("presetName",file.getFileNameWithoutExtension(),nullptr); presetRevision.fetch_add(1);
    return juce::Result::ok();
}
juce::Result FrostbiteAudioProcessor::loadPreset(const juce::File& file) {
    if(file.getSize()>1024*1024) return juce::Result::fail("Preset is too large.");
    if(auto xml=juce::XmlDocument::parse(file))
        if(restoreTree(juce::ValueTree::fromXml(*xml))) return juce::Result::ok();
    return juce::Result::fail("This is not a supported FROSTBITE preset.");
}
void FrostbiteAudioProcessor::restoreDefaults() {
    applyValues(frostbite::defaults()); state.state.setProperty("presetName","Init",nullptr); presetRevision.fetch_add(1);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FrostbiteAudioProcessor(); }
