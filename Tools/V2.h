#pragma once
#include <algorithm>
namespace v2 {
using namespace frostbite;
inline int execute(const juce::File& dir) {
    if(dir.createDirectory().failed())return 1;
    std::ofstream metrics(dir.getChildFile("matrix.csv").getFullPathName().toStdString());
    std::ofstream stages(dir.getChildFile("stages.csv").getFullPathName().toStdString());
    const std::string columns=",peak,peak_dbfs,rms,rms_dbfs,gain_db,dc,clips,nonfinite,stereo_correlation,dc_left,dc_right";
    metrics<<"case,signal,input_peak_dbfs,sample_rate,block"<<columns<<",input_rms,tail_rms,dry_wet_cosine,max_dry_error\n";
    stages<<"case,signal,input_peak_dbfs,sample_rate,block,stage"<<columns<<'\n';
    for(const std::string kind:{"piano","cello","guitar","synth","transient"}) {
        const auto input=audit::signal(kind,48000,4,-18);
        if(!audit::wav(dir,"input_"+kind,input,48000))return 2;
        for(size_t index=8;index<factoryPresets().size();++index) {
            const auto& preset=factoryPresets()[index]; const auto name=audit::slug(preset.name);
            std::cout<<"V2 render "<<name<<' '<<kind<<std::endl;
            auto out=audit::run({name,preset.values},input,48000,256,6,metrics,stages,kind,-18);
            if(!audit::wav(dir,name+"_"+kind,out,48000)||!audit::matched(dir,name+"_"+kind,out,48000,input.getNumSamples(),kind=="transient"?-30.0f:-24.0f))return 2;
            const auto hot=audit::signal(kind,48000,4,-6);
            audit::run({name,preset.values},hot,48000,256,2,metrics,stages,kind,-6);
        }
    }
    // These files make the requested capture -> stop -> destroy -> echo -> huge-space sequence explicit.
    std::ofstream sustain(dir.getChildFile("hold_stages.csv").getFullPathName().toStdString());
    sustain<<"case,second,frozen_rms,damage_rms,echo_rms,reverb_rms,output_rms\n";
    for(int mode=0;mode<3;++mode) {
        auto p=mode==0?legacyDefaults():defaults();
        p[Freeze]=1;p[FreezeMode]=0;p[FreezeRouting]=mode==2?1.0f:0.0f;
        p[Damage]=p[Echo]=p[Abyss]=p[Distance]=p[Motion]=0;p[Mix]=1;p[Output]=0;
        Engine engine;engine.setParameters(p);engine.prepare(48000,256);
        juce::AudioBuffer<float> out(2,48000*10);out.clear();const auto in=audit::signal("cello",48000,2,-18);
        for(int c=0;c<2;++c)out.copyFrom(c,0,in,c,0,in.getNumSamples());
        std::array<TraceFrame,256> trace{};std::array<std::array<audit::Stats,5>,10> stats{};
        for(int pos=0;pos<out.getNumSamples();pos+=256) {
            p[Capture]=pos>=72000?1.0f:0.0f;
            p[Damage]=pos>=144000?1.0f:0.0f;p[Echo]=pos>=192000?.9f:0.0f;p[Abyss]=pos>=240000?1.0f:0.0f;
            engine.setParameters(p);const int n=std::min(256,out.getNumSamples()-pos);
            engine.processOffline(out.getWritePointer(0,pos),out.getWritePointer(1,pos),n,trace.data());
            for(int i=0;i<n;++i){const int sec=(pos+i)/48000;int j=0;for(auto stage:{Frozen,DamageOut,EchoReturn,ReverbReturn,Final})stats[size_t(sec)][size_t(j++)].add(trace[size_t(i)].stages[stage]);}
        }
        const std::string name=mode==0?"hold_legacy":mode==1?"hold_clean_V2":"hold_destroyed_V2";
        for(size_t sec=0;sec<10;++sec){sustain<<name<<','<<sec;for(auto& st:stats[sec])sustain<<','<<st.rms();sustain<<'\n';}
        if(!audit::wav(dir,name,out,48000)||!audit::matched(dir,name,out,48000,48000*10))return 2;
    }
    // Save both character choices and intermodulation probes, independent of whole-chain ambience.
    for(int voice=0;voice<2;++voice)for(int character=0;character<2;++character)
    for(float amount:{0.0f,.25f,.5f,.75f,1.0f})for(float level:{-18.0f,-6.0f}) {
        auto p=voice?defaults():legacyDefaults();p[Damage]=amount;p[Character]=float(character);
        auto b=audit::signal("multitone",48000,2,level);DamageEngine damage;damage.prepare(48000);damage.configure(mapParameters(p,120));
        for(int n=0;n<b.getNumSamples();++n){const auto y=damage.process({b.getSample(0,n),b.getSample(1,n)});b.setSample(0,n,y.l);b.setSample(1,n,y.r);}
        const std::string name=std::string("imd_")+(voice?"V2":"Legacy")+(character?"_Broken_":"_Warm_")+std::to_string(int(amount*100))+"_"+std::to_string(int(level));
        if(!audit::wav(dir,name,b,48000))return 2;
    }
    return metrics && stages && sustain?0:3;
}
inline int benchmark(const juce::File& dir) {
    if(dir.createDirectory().failed())return 1;
    std::ofstream file(dir.getChildFile("cpu.csv").getFullPathName().toStdString());
    file<<"voicing,sample_rate,block,scenario,realtime_percent,median_block_us,p99_block_us,max_block_us\n";
    for(int voice=0;voice<2;++voice)for(double sr:{44100.0,48000.0,96000.0})for(int block:{1,64,128,256,512,1024})for(int scenario=0;scenario<3;++scenario) {
        auto p=voice?defaults():legacyDefaults();
        if(scenario==2){for(size_t k=Freeze;k<=Mix;++k)p[k]=1;p[Drive]=30;p[Decay]=35;p[Feedback]=.94f;}
        Engine engine;engine.setParameters(p);engine.prepare(sr,block);
        std::vector<float> l(size_t(block),0),r(size_t(block),0);std::vector<double> times;
        const int rounds=std::max(64,int(sr*.5)/block);times.reserve(size_t(rounds));
        for(int b=-32;b<rounds;++b) {
            for(int i=0;i<block;++i)l[size_t(i)]=r[size_t(i)]=scenario?.2f*std::sin(float(i+b*block)*.071f):0;
            if(scenario==2){p[Capture]=float((b/8)%2==0);p[FreezeRouting]=float((b/16)%2==0);engine.setParameters(p);}
            const auto start=std::chrono::steady_clock::now();engine.setParameters(p);engine.process(l.data(),r.data(),block);
            const auto us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
            if(b>=0)times.push_back(us);
        }
        double total=0;for(auto t:times)total+=t;std::sort(times.begin(),times.end());
        file<<(voice?"V2":"Legacy")<<','<<sr<<','<<block<<','<<(scenario==0?"silence":scenario==1?"default":"extreme_automation")<<','
            <<100*total*1e-6/(double(rounds*block)/sr)<<','<<times[times.size()/2]<<','<<times[size_t(.99*(times.size()-1))]<<','<<times.back()<<'\n';
    }
    return file?0:1;
}
}
