#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>
#include <thread>

static void check(bool ok,const juce::String& why) {if(!ok) throw std::runtime_error(why.toStdString());}

// A VST3 editor is a native child window, not a traversable JUCE control tree.
// Check the exported automation boundary here; UiTests.cpp checks real control pixels.
class HostAutomationCheck final : private juce::Timer {
public:
    explicit HostAutomationCheck(juce::AudioPluginInstance& instance):plugin(instance) {
        for(auto name:{"Freeze","Damage","Distance","Echo","Abyss","Motion","Mix","Output",
                       "Grain size","Echo time","Feedback","Decay","Echo tone","Motion rate","Extra drive"}) {
            juce::AudioProcessorParameter* found=nullptr;
            for(auto* p:plugin.getParameters()) if(p->getName(128)==name) found=p;
            check(found!=nullptr,"Missing exported VST3 parameter: "+juce::String(name));
            parameters.push_back(found);
        }
        plugin.setRateAndBufferSizeDetails(48000,128);plugin.prepareToPlay(48000,128);
    }
    void run() {
        setFromHost(); startTimer(40);
        juce::MessageManager::getInstance()->runDispatchLoop();
        if(failure) std::rethrow_exception(failure);
        check(control==parameters.size(),"VST3 automation message loop ended early");
        plugin.releaseResources();
    }
private:
    juce::AudioPluginInstance& plugin;
    std::vector<juce::AudioProcessorParameter*> parameters;
    size_t control=0;
    int state=0;
    juce::AudioBuffer<float> audio{2,128};
    juce::MidiBuffer midi;
    std::exception_ptr failure;
    void setFromHost() {
        std::thread worker([&] { parameters[control]->setValueNotifyingHost(float(state)*.5f); });worker.join();
    }
    void timerCallback() override {
        try {
            audio.clear();plugin.processBlock(audio,midi);
            auto* p=parameters[control];
            check(std::abs(p->getValue()-float(state)*.5f)<.006f,"VST3 automation value mismatch: "+p->getName(128));
            for(int c=0;c<2;++c) for(int n=0;n<audio.getNumSamples();++n)
                check(std::isfinite(audio.getSample(c,n)),"VST3 automated output nonfinite");
            if(++state==3) { state=0;++control; }
            if(control<parameters.size()) { setFromHost(); return; }
        } catch(...) { failure=std::current_exception(); }
        stopTimer();juce::MessageManager::getInstance()->stopDispatchLoop();
    }
};
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI juce;
    try {
        check(argc>1,"Usage: frostbite_host_check <path-to-FROSTBITE.vst3>");
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> types;
        format.findAllTypesForFile(types,juce::File(argv[1]).getFullPathName());
        check(types.size()==1,"VST3 scan did not find exactly one effect");
        // JUCE 9 scans moduleinfo.json without instantiating: channel counts are
        // intentionally zero there, and must be queried from the live instance.
        check(!types[0]->isInstrument && types[0]->category.contains("Fx"),"VST3 effect metadata incorrect");
        juce::String error;
        auto plugin=format.createInstanceFromDescription(*types[0],48000,512,error);
        check(plugin!=nullptr,"VST3 instantiate: "+error);
        check(plugin->getTotalNumOutputChannels()==2 && plugin->getTotalNumInputChannels()==2,"VST3 stereo bus layout incorrect");
        check(plugin->getParameters().size()>=34,"Missing exported parameters");
        juce::MemoryBlock saved;plugin->getStateInformation(saved);check(saved.getSize()>0,"Empty VST3 state");
        std::vector<float> initial;for(auto* parameter:plugin->getParameters()) initial.push_back(parameter->getValue());
        double energy=0;float peak=0;
        for(double sr:{44100.0,48000.0,96000.0}) for(int size:{64,128,512,1024}) {
            plugin->setRateAndBufferSizeDetails(sr,size);plugin->prepareToPlay(sr,size);
            juce::AudioBuffer<float> audio(2,size);juce::MidiBuffer midi;
            for(int block=0;block<150;++block) {
                audio.clear();
                for(int n=0;n<size;++n) {float s=.2f*std::sin(float(block*size+n)*float(2*juce::MathConstants<double>::pi*220/sr));
                    audio.setSample(0,n,s);audio.setSample(1,n,s*.6f);}
                float* channels[]{audio.getWritePointer(0),audio.getWritePointer(1)};
                const int count=block%3==0?std::max(1,size-17):size;
                juce::AudioBuffer<float> view(channels,2,count);
                plugin->processBlock(view,midi);
                for(int c=0;c<2;++c) for(int n=0;n<count;++n) {
                    const float x=audio.getSample(c,n);check(std::isfinite(x) && std::abs(x)<=1.01f,"Invalid VST3 output");
                    energy+=x*x;peak=std::max(peak,std::abs(x));
                }
            }
            plugin->releaseResources();
        }
        check(energy>1,"VST3 did not process audio");
        plugin->setStateInformation(saved.getData(),int(saved.getSize()));
        std::unique_ptr<juce::AudioProcessorEditor> editor(plugin->createEditorIfNeeded());check(editor!=nullptr,"VST3 editor could not open");
        HostAutomationCheck(*plugin).run();
        editor.reset();
        plugin->setStateInformation(saved.getData(),int(saved.getSize()));
        for(int i=0;i<plugin->getParameters().size();++i)
            check(std::abs(plugin->getParameters()[i]->getValue()-initial[size_t(i)])<.002f,"VST3 state restore after automation");
        editor.reset(plugin->createEditorIfNeeded());check(editor!=nullptr,"VST3 editor reopen after automation/state restore");editor.reset();
        std::cout<<"PASS VST3 scan, effect metadata, instantiate, "<<plugin->getParameters().size()
            <<" parameters, state, 12 sample-rate/block configurations; 15 parameters automated off-thread at min/mid/max, state restore and editor reopen (native editor pixels require host acceptance); peak="<<peak<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
