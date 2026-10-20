#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI juce;
    try {
        FrostbiteAudioProcessor p;
        const auto defaultValues=frostbite::defaults();
        for(size_t n=0;n<defaultValues.size();++n)
            check(std::abs(p.values()[n]-defaultValues[n])<.002f,"parameter defaults (APVTS range roundtrip)");
        auto* mixParameter=p.state.getParameter("mix");
        check(std::abs(mixParameter->getValueForText("65 %")-.65f)<.001f,"percentage text editing");
        for(int i=0;i<8;++i) {
            p.setCurrentProgram(i);
            auto expected=frostbite::factoryPresets()[size_t(i)].values;
            for(size_t n=0;n<expected.size();++n) check(std::abs(p.values()[n]-expected[n])<.002f,"factory parameter restore");
        }
        p.setCurrentProgram(3);
        juce::MemoryBlock state; p.getStateInformation(state);
        FrostbiteAudioProcessor restored; restored.setStateInformation(state.getData(),int(state.getSize()));
        for(size_t n=0;n<frostbite::Count;++n) check(std::abs(p.values()[n]-restored.values()[n])<.001f,"state roundtrip");
        check(restored.presetName()=="Industrial Fog","preset name roundtrip");
        auto invalid=p.values(); invalid[frostbite::Capture]=1; p.applyValues(invalid); p.getStateInformation(state);
        restored.setStateInformation(state.getData(),int(state.getSize()));
        check(restored.values()[frostbite::Capture]==0,"capture should reopen unarmed");
        const auto before=restored.values();const char bad[]="invalid";restored.setStateInformation(bad,7);
        check(restored.values()==before,"invalid state changed params");
        auto dir=juce::File::getCurrentWorkingDirectory().getChildFile("qa");dir.createDirectory();
        for(int i=0;i<8;++i) {
            p.setCurrentProgram(i); auto f=dir.getChildFile("Factory Presets").getChildFile(p.getProgramName(i)+".frostbite");
            check(p.savePreset(f).wasOk(),"save user preset"); check(restored.loadPreset(f).wasOk(),"load user preset");
            check(std::abs(restored.values()[frostbite::Damage]-p.values()[frostbite::Damage])<.001f,"preset value mismatch");
        }
        for(double sr:{44100.0,48000.0,96000.0}) for(int block:{64,128,512,1024}) {
            p.prepareToPlay(sr,block); juce::AudioBuffer<float> audio(2,block);juce::MidiBuffer midi;
            for(int b=0;b<20;++b) {
                audio.clear();for(int n=0;n<block;++n) audio.setSample(0,n,.1f*std::sin(float(b*block+n)*.1f));
                p.processBlock(audio,midi);
                for(int c=0;c<2;++c) for(int n=0;n<block;++n) check(std::isfinite(audio.getSample(c,n)),"plugin nonfinite output");
            }
            p.releaseResources();
        }
        auto mono=p.getBusesLayout();mono.inputBuses.set(0,juce::AudioChannelSet::mono());
        check(p.setBusesLayout(mono),"mono/stereo layout rejected");
        auto surround=mono;surround.outputBuses.set(0,juce::AudioChannelSet::create5point1());
        check(!p.isBusesLayoutSupported(surround),"unsupported surround accepted");
        p.prepareToPlay(48000,128);p.restoreDefaults();
        auto v=p.values();v[frostbite::Bypass]=1;p.applyValues(v);p.prepareToPlay(48000,128);
        juce::AudioBuffer<float> b(2,128);b.clear();b.setSample(0,16,.25f);juce::MidiBuffer midi;p.processBlockBypassed(b,midi);
        check(b.getSample(0,16)==.25f && b.getSample(1,16)==.25f,"mono bypass mismatch");
        // Render the real JUCE editor, including live parameter attachments, at two scales.
        p.setCurrentProgram(0);
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        check(editor!=nullptr,"editor creation");
        auto snapshot=[&](const juce::String& name) {
            const auto image=editor->createComponentSnapshot(editor->getLocalBounds(),true,1.0f);
            juce::FileOutputStream stream(dir.getChildFile(name));juce::PNGImageFormat png;
            check(stream.setPosition(0) && stream.truncate().wasOk(),"snapshot truncate");
            check(stream.openedOk() && png.writeImageToStream(image,stream),"editor snapshot");
        };
        snapshot("editor-1000.png");editor->setSize(1500,1050);snapshot("editor-1500.png");
        editor->setSize(1000,700);
        std::function<juce::TextButton*(juce::Component&,const juce::String&)> findButton=
            [&](juce::Component& c,const juce::String& name)->juce::TextButton* {
                if(auto* button=dynamic_cast<juce::TextButton*>(&c);button && button->getButtonText()==name) return button;
                for(auto* child:c.getChildren()) if(auto* found=findButton(*child,name)) return found;
                return nullptr;
            };
        auto* details=findButton(*editor,"DETAILS");check(details!=nullptr,"details button");details->onClick();
        for(auto tab:{"FROZEN","ECHO","ABYSS","CHARACTER"}) {
            auto* button=findButton(*editor,tab);check(button!=nullptr,"engine tab");button->onClick();
            snapshot(juce::String("editor-")+tab+".png");
        }
        details->onClick();
        if(argc>1 && juce::String(argv[1])=="--show-editor") {
            editor->addToDesktop(juce::ComponentPeer::windowHasTitleBar|juce::ComponentPeer::windowIsResizable);
            editor->setVisible(true);
            juce::MessageManager::getInstance()->runDispatchLoop();
        }
        std::cout<<"PASS plugin defaults, eight presets, files, state, malformed state, sample rates, buses, bypass, editor snapshots\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
}
