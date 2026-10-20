#include "Parameters.h"

namespace frostbite {
const std::array<ParameterInfo,Count> parameterInfo {{
    {"freeze","Freeze","%",0,1,.001f,1},{"damage","Damage","%",0,1,.001f,1},
    {"distance","Distance","%",0,1,.001f,1},{"echo","Echo","%",0,1,.001f,1},
    {"abyss","Abyss","%",0,1,.001f,1},{"motion","Motion","%",0,1,.001f,1},
    {"mix","Mix","%",0,1,.001f,1},{"output","Output","dB",-24,6,.1f,1},
    {"capture","Capture","",0,1,1,1},{"bypass","Bypass","",0,1,1,1},
    {"texture","Grain size","ms",40,400,1,.7f},{"drift","Grain drift","%",0,1,.001f,1},
    {"freeze_level","Frozen level","dB",-24,6,.1f,1},
    {"echo_ms","Echo time","ms",20,2000,1,.5f},{"echo_sync","Tempo sync","",0,1,1,1},
    {"echo_division","Division","",0,8,1,1},{"feedback","Feedback","%",0,.94f,.001f,1},
    {"echo_tone","Echo tone","Hz",600,12000,1,.45f},{"width","Echo width","%",0,1.5f,.001f,1},
    {"echo_level","Echo level","%",0,1,.001f,1},{"size","Room size","%",0,1,.001f,1},
    {"decay","Decay","s",.5f,35,.1f,.6f},{"damping","Damping","%",0,1,.001f,1},
    {"diffusion","Diffusion","%",0,1,.001f,1},{"reverb_mod","Tail motion","%",0,1,.001f,1},
    {"predelay","Pre-delay","ms",0,150,1,1},{"reverb_level","Reverb level","%",0,1,.001f,1},
    {"drive","Extra drive","dB",0,30,.1f,1},{"character","Character","",0,1,1,1},
    {"damage_tone","Damage tone","Hz",600,16000,1,.45f},{"damage_blend","Damage blend","%",0,1,.001f,1},
    {"motion_rate","Motion rate","Hz",.03f,2,.01f,.5f},{"motion_depth","Motion depth","%",0,1,.001f,1},
    {"input","Input","dB",-24,12,.1f,1}
}};
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto d=defaults();
    for(size_t i=0;i<Count;++i) {
        const auto& p=parameterInfo[i]; const juce::ParameterID id(p.id,1);
        if(i==Capture || i==Bypass || i==EchoSync)
            layout.add(std::make_unique<juce::AudioParameterBool>(id,p.name,d[i]>.5f));
        else if(i==EchoDivision)
            layout.add(std::make_unique<juce::AudioParameterChoice>(id,p.name,juce::StringArray{"1/16","1/8","1/8 T","1/4","1/8 D","1/4 T","1/2","1/4 D","1/1"},int(d[i])));
        else if(i==Character)
            layout.add(std::make_unique<juce::AudioParameterChoice>(id,p.name,juce::StringArray{"Warm","Broken"},0));
        else {
            auto attributes=juce::AudioParameterFloatAttributes().withLabel(p.unit)
                .withStringFromValueFunction([i](float v,int) {
                    if(juce::String(parameterInfo[i].unit)=="%") return juce::String(v*100,1)+" %";
                    // Preserve the parameter's precision (Motion Rate has a 0.01 Hz step).
                    const auto step=parameterInfo[i].step;
                    const int decimals=step<.1f?2:step<1?1:0;
                    return juce::String(v,decimals)+" "+parameterInfo[i].unit;
                })
                .withValueFromStringFunction([i](const juce::String& text) {
                    const float value=text.getFloatValue();
                    return juce::String(parameterInfo[i].unit)=="%"?value*.01f:value;
                });
            layout.add(std::make_unique<juce::AudioParameterFloat>(id,p.name,
                juce::NormalisableRange<float>(p.min,p.max,p.step,p.skew),d[i],attributes));
        }
    }
    return layout;
}
const std::array<FactoryPreset,8>& factoryPresets() {
    static const auto presets=[] {
        std::array<FactoryPreset,8> p{};
        auto put=[&](int n,const char* name,const char* desc,std::initializer_list<std::pair<Param,double>> values) {
            auto v=defaults(); for(auto [key,value]:values) v[key]=float(value);
            p[size_t(n)]={name,desc,v};
        };
        put(0,"Frozen Cello","A suspended harmonic cloud. Play a note, then capture.",
            {{Freeze,.82f},{Damage,.14f},{Distance,.38f},{Echo,.22f},{Abyss,.66f},{Motion,.26f},{Texture,260},{Drift,.15f},{Decay,12},{Mix,.65f}});
        put(1,"Glacial Echo","Clean first reflections. Dark, alternating returns.",
            {{Freeze,.35f},{Damage,.08f},{Distance,.16f},{Echo,.82f},{Abyss,.28f},{Motion,.12f},{EchoDivision,7},{Feedback,.78f},{EchoTone,7000},{Mix,.48f}});
        put(2,"Distant Cathedral","Soft edges and a vast, slowly falling stereo tail.",
            {{Freeze,.45f},{Damage,.04f},{Distance,.68f},{Echo,.18f},{Abyss,.92f},{Motion,.18f},{Size,.94f},{Decay,19},{Predelay,64},{Damping,.48f},{Mix,.72f}});
        put(3,"Industrial Fog","Asymmetric grit buried in dense, dark reflections.",
            {{Freeze,.58f},{Damage,.86f},{Distance,.57f},{Echo,.48f},{Abyss,.76f},{Motion,.42f},{Character,1},{Drive,14},{DamageTone,5100},{Decay,14},{Mix,.77f},{Output,-5}});
        put(4,"Broken Transmission","Narrow, unstable repeats with a ragged silhouette.",
            {{Freeze,.38f},{Damage,.70f},{Distance,.74f},{Echo,.78f},{Abyss,.32f},{Motion,.72f},{Character,1},{EchoDivision,2},{Feedback,.85f},{EchoTone,2600},{MotionRate,.67f},{MotionDepth,.64f},{Mix,.64f}});
        put(5,"Black Ice","Short grains, cold reflections and restrained motion.",
            {{Freeze,.74f},{Damage,.31f},{Distance,.28f},{Echo,.42f},{Abyss,.63f},{Motion,.52f},{Texture,72},{Drift,.66f},{Size,.25f},{Damping,.21f},{Decay,9},{Mix,.67f}});
        put(6,"Endless Winter","Long, quiet horizons. Capture audio to hold the moment.",
            {{Freeze,.96f},{Damage,.06f},{Distance,.45f},{Echo,.33f},{Abyss,.97f},{Motion,.36f},{Texture,350},{Drift,.26f},{Decay,32},{Damping,.66f},{Size,1},{Mix,.85f},{Output,-5}});
        put(7,"Ruined Piano","Rounded attacks, cracked harmonics and trailing dust.",
            {{Freeze,.48f},{Damage,.58f},{Distance,.45f},{Echo,.56f},{Abyss,.67f},{Motion,.27f},{Drive,10},{DamageTone,4900},{EchoDivision,4},{Feedback,.63f},{Decay,11},{Predelay,38},{Mix,.58f}});
        return p;
    }();
    return presets;
}
}
