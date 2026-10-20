#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <stdexcept>

static void check(bool ok,const juce::String& why) {if(!ok) throw std::runtime_error(why.toStdString());}
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
        editor.reset();
        std::cout<<"PASS VST3 scan, effect metadata, instantiate, "<<plugin->getParameters().size()
            <<" parameters, state, 12 sample-rate/block configurations, editor creation; peak="<<peak<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
