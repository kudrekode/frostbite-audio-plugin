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
    {"input","Input","dB",-24,12,.1f,1},
    {"voicing","Voicing","",0,1,1,1},{"freeze_mode","Freeze mode","",0,1,1,1},
    {"freeze_routing","Frozen routing","",0,1,1,1}
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
        else if(i==Voicing || i==FreezeMode || i==FreezeRouting)
            layout.add(std::make_unique<juce::AudioParameterChoice>(id,p.name,
                i==Voicing?juce::StringArray{"Legacy","V2"}:i==FreezeMode?juce::StringArray{"Hold","Live Texture"}:juce::StringArray{"Clean","Damage + Echo"},int(d[i])));
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
const std::array<FactoryPreset,18>& factoryPresets() {
    static const auto presets=[] {
        std::array<FactoryPreset,18> p{};
        auto put=[&](int n,const char* name,const char* desc,std::initializer_list<std::pair<Param,double>> values) {
            auto v=legacyDefaults(); for(auto [key,value]:values) v[key]=float(value);
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
        auto modern=[&](int n,const char* name,const char* desc,std::initializer_list<std::pair<Param,double>> values) {
            auto v=defaults(); for(auto [key,value]:values) v[key]=float(value); p[size_t(n)]={name,desc,v};
        };
        modern(8,"Frozen Cello II","A living harmonic cloud. Capture to suspend a moment.", {{Freeze,0.72},{Damage,0.18},{Distance,0.27},{Echo,0.26},{Abyss,0.65},{Motion,0.3},{Texture,280},{Drift,0.18},{Decay,15},{Mix,0.66},{FreezeRouting,0},{Output,-0.5}});
        modern(9,"Glacial Echo II","Wide crystalline repeats dissolving into a bright hall.", {{Freeze,0.22},{Damage,0.08},{Distance,0.1},{Echo,0.86},{Abyss,0.48},{Motion,0.24},{EchoDivision,7},{Feedback,0.79},{EchoTone,8500},{Damping,0.24},{Width,1.3},{Mix,0.52},{Output,-0.5}});
        modern(10,"Distant Cathedral II","An immense diffuse hall, with a clear first arrival.", {{Freeze,0.18},{Damage,0.04},{Distance,0.5},{Echo,0.14},{Abyss,0.96},{Motion,0.21},{Size,0.96},{Decay,24},{Predelay,65},{Damping,0.35},{Mix,0.72}});
        modern(11,"Industrial Fog II","Ragged asymmetric harmonics inside a moving storm.", {{Freeze,0.38},{Damage,0.86},{Distance,0.22},{Echo,0.42},{Abyss,0.62},{Motion,0.4},{Character,1},{Drive,7},{DamageTone,10000},{Decay,14},{Mix,0.76},{Output,-1.5}});
        modern(12,"Broken Transmission II","Fractured short grains and narrow unstable triplets.", {{Freeze,0.45},{Damage,0.72},{Distance,0.6},{Echo,0.8},{Abyss,0.22},{Motion,0.8},{Character,1},{Texture,48},{Drift,0.9},{EchoDivision,2},{Feedback,0.83},{EchoTone,3800},{Width,0.4},{MotionRate,0.71},{MotionDepth,0.7},{Mix,0.68}});
        modern(13,"Black Ice II","Cold short grains and bright metallic depth.", {{Freeze,0.65},{Damage,0.37},{Distance,0.12},{Echo,0.36},{Abyss,0.65},{Motion,0.55},{Texture,64},{Drift,0.72},{Size,0.32},{Damping,0.12},{Decay,12},{DamageTone,13000},{Mix,0.68},{FreezeRouting,0},{Output,-1}});
        modern(14,"Endless Winter II","Continuously refreshed long grains in an enormous horizon.", {{Freeze,0.9},{Damage,0.1},{Distance,0.3},{Echo,0.3},{Abyss,0.96},{Motion,0.45},{Texture,380},{Drift,0.34},{Decay,32},{Damping,0.48},{Size,1},{Mix,0.83},{FreezeRouting,0},{Output,-1.5}});
        modern(15,"Ruined Piano II","Jagged attacks feeding dark, rhythmically spaced trails.", {{Freeze,0.28},{Damage,0.78},{Distance,0.3},{Echo,0.62},{Abyss,0.6},{Motion,0.28},{Drive,6},{DamageTone,8800},{EchoDivision,4},{Feedback,0.65},{Decay,13},{Predelay,40},{Mix,0.64},{Output,-1}});
        modern(16,"Whiteout","A luminous live cloud. Capture for an enduring frozen environment.", {{Freeze,1},{Damage,0.16},{Distance,0.12},{Echo,0.48},{Abyss,1},{Motion,0.5},{Texture,400},{Drift,0.22},{Decay,35},{Size,1},{Damping,0.18},{Feedback,0.8},{Mix,0.9},{FreezeRouting,0},{Output,-3}});
        modern(17,"Cataclysm","Controlled industrial destruction with a vast unstable wake.", {{Freeze,0.52},{Damage,1},{Distance,0.15},{Echo,0.68},{Abyss,0.78},{Motion,0.8},{Character,1},{Drive,16},{DamageTone,13000},{Texture,90},{Drift,0.8},{Feedback,0.87},{Decay,20},{MotionDepth,0.65},{Mix,0.9},{Output,-2}});
        return p;
    }();
    return presets;
}
}
