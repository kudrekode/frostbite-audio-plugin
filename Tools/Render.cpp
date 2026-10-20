#include "PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include "Audition.h"
#include <chrono>
#include <iostream>

static bool writeWav(const juce::File& path,const juce::AudioBuffer<float>& audio,double rate) {
    path.getParentDirectory().createDirectory();
    auto fileStream=path.createOutputStream();
    if(!fileStream || !fileStream->setPosition(0) || fileStream->truncate().failed()) return false;
    std::unique_ptr<juce::OutputStream> stream=std::move(fileStream);
    juce::WavAudioFormat wav;
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}
        .withSampleRate(rate).withNumChannels(audio.getNumChannels()).withBitsPerSample(24));
    if(!writer) return false;
    return writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples());
}
static juce::AudioBuffer<float> render(FrostbiteAudioProcessor& p,const juce::AudioBuffer<float>& dry,double sr,bool capture,int tail) {
    const int total=dry.getNumSamples()+int(sr)*tail;
    juce::AudioBuffer<float> output(2,total),block(2,512);output.clear();
    p.prepareToPlay(sr,512);juce::MidiBuffer midi;
    const auto start=std::chrono::steady_clock::now();
    for(int pos=0;pos<total;pos+=512) {
        const int n=std::min(512,total-pos);block.setSize(2,n,false,false,true);block.clear();
        for(int c=0;c<2;++c) if(pos<dry.getNumSamples()) block.copyFrom(c,0,dry,std::min(c,dry.getNumChannels()-1),pos,std::min(n,dry.getNumSamples()-pos));
        if(capture && pos>=int(sr*1.5) && pos<int(sr*1.5)+512) {
            auto* param=p.state.getParameter("capture");param->setValueNotifyingHost(1);
        }
        if(capture && pos>=int(sr*9) && pos<int(sr*9)+512) p.state.getParameter("capture")->setValueNotifyingHost(0);
        p.processBlock(block,midi);
        for(int c=0;c<2;++c) output.copyFrom(c,pos,block,c,0,n);
    }
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    double sum=0,dc=0;float peak=0;int last=0;
    for(int c=0;c<2;++c) for(int n=0;n<total;++n) {const float x=output.getSample(c,n);sum+=x*x;dc+=x;peak=std::max(peak,std::abs(x));if(std::abs(x)>.001f)last=std::max(last,n);}
    std::cout<<p.presetName()<<",peak="<<peak<<",rms="<<std::sqrt(sum/(2*total))<<",dc="<<dc/(2*total)
        <<",last_above_-60dB_s="<<double(last)/sr<<",render_seconds="<<seconds<<",cpu_realtime_percent="<<100*seconds/(total/sr)<<"\n";
    return output;
}
int main(int argc,char** argv) {
    juce::ScopedJuceInitialiser_GUI juce;
    if(argc>=2 && juce::String(argv[1])=="--audition") {
        AuditionWindow window;
        if(argc>=3) static_cast<AuditionPanel*>(window.getContentComponent())->loadFile(juce::File(argv[2]));
        juce::MessageManager::getInstance()->runDispatchLoop();
        return 0;
    }
    if(argc>=2 && juce::String(argv[1])=="--diagnostics") {
        auto folder=argc>=3?juce::File(argv[2]):juce::File::getCurrentWorkingDirectory().getChildFile("renders");
        folder.createDirectory();
        constexpr double sr=48000;
        const char* names[]{"01-frozen-cello","02-ruined-piano","03-glacial-echo","04-industrial-fog","05-distant-cathedral"};
        const int programs[]{0,7,1,3,2};
        for(int kind=0;kind<5;++kind) {
            juce::AudioBuffer<float> dry(2,int(sr*4));dry.clear();
            for(int n=0;n<dry.getNumSamples();++n) {
                const double t=n/sr;float s=0;
                if(kind==2) s=n==0?.5f:0;
                else if(kind==1) {
                    const int note=int(t/.65);const double age=std::fmod(t,.65);
                    constexpr double notes[]{261.626,311.127,391.995,466.164,391.995,311.127,261.626};
                    const double f=notes[note%7];
                    for(int h=1;h<=7;++h) s+=float(.18/h*std::sin(2*juce::MathConstants<double>::pi*f*h*age)*std::exp(-age*(2.5+h*.4))*(1-std::exp(-age*400)));
                } else {
                    const double env=std::min(1.0,t*5)*std::clamp((3.5-t)*3,0.0,1.0);
                    for(int h=1;h<=6;++h) s+=float(.16/h*std::sin(2*juce::MathConstants<double>::pi*110*h*t)*env);
                }
                dry.setSample(0,n,s);
                dry.setSample(1,n,kind==4?float(.13*std::sin(2*juce::MathConstants<double>::pi*164.814*t)*std::min(1.0,t*5)*std::clamp((3.5-t)*3,0.0,1.0)):s*.86f);
            }
            FrostbiteAudioProcessor p;p.setCurrentProgram(programs[kind]);
            auto wet=render(p,dry,sr,kind==0,18);
            if(!writeWav(folder.getChildFile(juce::String(names[kind])+"-dry.wav"),dry,sr) ||
               !writeWav(folder.getChildFile(juce::String(names[kind])+"-wet.wav"),wet,sr)) return 1;
        }
        return 0;
    }
    if(argc<3) {std::cout<<"Usage: frostbite_render input.wav output.wav [preset 0..7] [tail seconds]\n       frostbite_render --diagnostics [output directory]\n";return 0;}
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    auto reader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(juce::File(argv[1])));
    if(!reader || reader->lengthInSamples>reader->sampleRate*600) {std::cerr<<"Cannot read input (maximum 10 minutes).\n";return 1;}
    juce::AudioBuffer<float> dry(2,int(reader->lengthInSamples));reader->read(&dry,0,dry.getNumSamples(),0,true,true);
    if(reader->numChannels==1) dry.copyFrom(1,0,dry,0,0,dry.getNumSamples());
    FrostbiteAudioProcessor p;p.setCurrentProgram(argc>=4?juce::jlimit(0,7,juce::String(argv[3]).getIntValue()):0);
    auto wet=render(p,dry,reader->sampleRate,false,argc>=5?juce::jlimit(0,120,juce::String(argv[4]).getIntValue()):15);
    return writeWav(juce::File(argv[2]),wet,reader->sampleRate)?0:1;
}
