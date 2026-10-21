#pragma once
// Deterministic offline diagnostics. No metrics or file I/O run in the plugin callback.
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>

namespace audit {
using namespace frostbite;
inline bool legacyAudit=false;
inline Values auditDefaults() { return legacyAudit?legacyDefaults():defaults(); }
constexpr double tau=6.2831853071795864769;
struct Stats {
    double sum=0, squares=0, l2=0, r2=0, lr=0, sumL=0, sumR=0, peak=0;
    uint64_t frames=0, clips=0, invalid=0;
    void add(Stereo x) {
        ++frames;
        if(!std::isfinite(x.l)||!std::isfinite(x.r)) {++invalid;return;}
        sum+=x.l+x.r;sumL+=x.l;sumR+=x.r;
        l2+=double(x.l)*x.l;r2+=double(x.r)*x.r;lr+=double(x.l)*x.r;
        squares+=double(x.l)*x.l+double(x.r)*x.r;
        peak=std::max({peak,double(std::abs(x.l)),double(std::abs(x.r))});
        clips+=uint64_t(std::abs(x.l)>=1)+uint64_t(std::abs(x.r)>=1);
    }
    double rms() const {return frames?std::sqrt(squares/(2*frames)):0;}
    double corr() const {
        if(!frames) return 0;
        const double a=l2-sumL*sumL/frames,b=r2-sumR*sumR/frames;
        return a*b>1e-30?(lr-sumL*sumR/frames)/std::sqrt(a*b):0;
    }
};
inline double db(double x) {return 20*std::log10(std::max(1e-15,x));}
inline std::string slug(const std::string& s) {std::string o;for(char c:s)o+=c==' '?'_':c;return o;}
inline void row(std::ostream& f,const Stats& s,double reference) {
    f<<','<<s.peak<<','<<db(s.peak)<<','<<s.rms()<<','<<db(s.rms())<<','<<(reference>1e-15?db(s.rms()/reference):std::numeric_limits<double>::quiet_NaN())
     <<','<<(s.frames?s.sum/(2*s.frames):0)<<','<<s.clips<<','<<s.invalid<<','<<s.corr()
     <<','<<(s.frames?s.sumL/s.frames:0)<<','<<(s.frames?s.sumR/s.frames:0);
}
inline juce::AudioBuffer<float> signal(const std::string& kind,double sr,double seconds,float level) {
    juce::AudioBuffer<float> b(2,int(sr*seconds));b.clear();uint32_t random=0x5481ab2u;
    double pinkRows[2][16]{},pinkSum[2]{};
    for(int n=0;n<b.getNumSamples();++n) {
        const double t=n/sr;
        for(int c=0;c<2;++c) {
            double x=0;
            if(kind=="impulse") x=n==0?1:0;
            else if(kind=="sine100"||kind=="sine1000"||kind=="sine5000"||kind=="sine7301") {
                const double hz=kind=="sine100"?100:kind=="sine1000"?1000:kind=="sine5000"?5000:7301;
                x=std::sin(tau*hz*t);
            } else if(kind=="multitone") {
                for(double hz:{83.0,251.0,997.0,3001.0,7001.0}) x+=std::sin(tau*hz*t+c*.31)/5;
            } else if(kind=="white"||kind=="pink") {
                random=random*1664525u+1013904223u; x=double(random>>8)/8388608.0-1;
                // Voss-McCartney octave sample/hold rows plus white term: approximate pink noise.
                if(kind=="pink") {
                    unsigned v=unsigned(n)+1;int row=0;while((v&1u)==0&&row<15){++row;v>>=1;}
                    pinkSum[c]-=pinkRows[c][row];pinkRows[c][row]=x;pinkSum[c]+=x;
                    random=random*1664525u+1013904223u;
                    x=pinkSum[c]+double(random>>8)/8388608.0-1;
                }
            } else if(kind=="piano") {
                const double age=std::fmod(t,.65); constexpr double notes[]{261.626,311.127,391.995,466.164};
                const double hz=notes[int(t/.65)%4]*(c?1.001:1);
                for(int h=1;h<=9;++h) x+=std::sin(tau*hz*h*age)*std::exp(-age*(2.5+h*.4))*(1-std::exp(-age*400))/h;
            } else if(kind=="guitar") {
                const double age=std::fmod(t,.8);
                for(double hz:{82.407,123.471,164.814})for(int h=1;h<=12;++h)
                    x+=std::sin(tau*hz*h*age+c*.02*h)*std::exp(-age*(2+h*.15))*(1-std::exp(-age*700))/h;
            } else if(kind=="synth") {
                for(int h=1;h<=15;++h)x+=std::sin(tau*(110+(c?.17:0))*h*t)/h;
                x*=.65+.35*std::sin(tau*.33*t);
            } else if(kind=="transient") {
                const double age=std::fmod(t,.75);
                random=random*1664525u+1013904223u;
                x=(double(random>>8)/8388608.0-1)*std::exp(-age*95)+.5*std::sin(tau*75*age)*std::exp(-age*22);
            } else if(kind=="cello") {
                for(int h=1;h<=9;++h) x+=std::sin(tau*110*h*t+c*.08*h)/h;
                x*=std::min(1.0,t*5)*std::clamp((seconds-t)*5,0.0,1.0);
            }
            b.setSample(c,n,float(x));
        }
    }
    const float peak=b.getMagnitude(0,b.getNumSamples());
    if(peak>0)b.applyGain(dbGain(level)/peak); // levels are sample-peak dBFS, not RMS
    return b;
}
inline bool wav(const juce::File& dir,const std::string& name,const juce::AudioBuffer<float>& b,double sr) {
    return writeWav(dir.getChildFile(juce::String(name)+".wav"),b,sr);
}
inline bool matched(const juce::File& dir,const std::string& name,juce::AudioBuffer<float> b,double sr,int active,float targetDb=-24) {
    Stats s;for(int n=0;n<active;++n)s.add({b.getSample(0,n),b.getSample(1,n)});
    const double gain=s.rms()>1e-12?dbGain(targetDb)/s.rms():1;
    b.applyGain(float(gain));
    // Normalise only to common RMS; refuse hidden clipping rather than peak-normalising each example.
    if(b.getMagnitude(0,b.getNumSamples())>=1) return false;
    return wav(dir,name+"_rms_minus"+std::to_string(int(-targetDb)),b,sr);
}
struct Case {std::string name;Values p;bool capture=false;};
inline juce::AudioBuffer<float> run(const Case& c,const juce::AudioBuffer<float>& input,double sr,int block,double tail,
                                  std::ostream& metrics,std::ostream& stages,const std::string& kind,float level) {
    Engine e;e.setParameters(c.p);e.prepare(sr,block);
    const int active=input.getNumSamples(),total=active+int(sr*tail);
    juce::AudioBuffer<float> out(2,total);out.clear();
    std::array<Stats,TraceCount> sums{};Stats in,output,tailStats;double cross=0, dry2=0,wet2=0,maxError=0;
    std::vector<TraceFrame> trace(static_cast<size_t>(block));
    for(int pos=0;pos<total;pos+=block) {
        const int n=std::min(block,total-pos);
        if(pos<active)for(int ch=0;ch<2;++ch)out.copyFrom(ch,pos,input,ch,pos,std::min(n,active-pos));
        if(c.capture) {auto p=c.p;p[Capture]=pos>=int(sr*1.5)&&pos<int(sr*5)?1.0f:0.0f;e.setParameters(p);}
        e.processOffline(out.getWritePointer(0,pos),out.getWritePointer(1,pos),n,trace.data());
        for(int i=0;i<n;++i) {
            const auto& f=trace[size_t(i)].stages;
            if(pos+i<active) {
                in.add(f[Dry]);output.add(f[Final]);
                for(size_t s=0;s<TraceCount;++s)sums[s].add(f[s]);
                cross+=double(f[Dry].l)*f[WetAfterDC].l+double(f[Dry].r)*f[WetAfterDC].r;
                dry2+=double(f[Dry].l)*f[Dry].l+double(f[Dry].r)*f[Dry].r;
                wet2+=double(f[WetAfterDC].l)*f[WetAfterDC].l+double(f[WetAfterDC].r)*f[WetAfterDC].r;
                maxError=std::max({maxError,double(std::abs(f[Final].l-f[Dry].l)),double(std::abs(f[Final].r-f[Dry].r))});
            } else tailStats.add(f[Final]);
        }
    }
    const std::string prefix=c.name+","+kind+","+std::to_string(level)+","+std::to_string(int(sr))+","+std::to_string(block);
    metrics<<prefix;row(metrics,output,in.rms());
    metrics<<','<<in.rms()<<','<<tailStats.rms()<<','<<(dry2*wet2>1e-30?cross/std::sqrt(dry2*wet2):0)<<','<<maxError<<'\n';
    const char* names[]{"dry","input_gain","frozen","damage","echo_return","layer_pre_motion","layer_post_motion","reverb_return","wet_pre_distance","wet_post_distance","wet_post_dc","mixed","protected","final"};
    for(size_t s=0;s<TraceCount;++s) {stages<<prefix<<','<<names[s];row(stages,sums[s],in.rms());stages<<'\n';}
    return out;
}
inline int execute(const juce::File& dir,bool legacy=false) {
    legacyAudit=legacy;
    if(dir.createDirectory().failed())return 1;
    auto path=[&](const char* n){return dir.getChildFile(n).getFullPathName().toStdString();};
    std::ofstream metrics(path("matrix.csv")), stages(path("stages.csv")),maps(path("macro_map.csv")),presets(path("presets.csv"));
    const std::string columns=",peak,peak_dbfs,rms,rms_dbfs,gain_db,dc,clips,nonfinite,stereo_correlation,dc_left,dc_right";
    metrics<<"case,signal,input_peak_dbfs,sample_rate,block"<<columns<<",input_rms,tail_rms,dry_wet_cosine,max_dry_error\n";
    stages<<"case,signal,input_peak_dbfs,sample_rate,block,stage"<<columns<<'\n';
    for(auto* f:{&metrics,&stages,&maps,&presets}) *f<<std::setprecision(10);
    Values flat=auditDefaults();for(int i=Freeze;i<=Motion;++i)flat[size_t(i)]=0;flat[Output]=0;
    std::vector<Case> cases;
    auto d=auditDefaults(); auto p=d;p[Bypass]=1;cases.push_back({"bypass",p});
    p=d;p[Mix]=0;cases.push_back({"dry_default_output",p});p[Output]=0;cases.push_back({"dry_unity",p});
    for(float mix:{0.0f,.5f,1.0f}) {p=flat;p[Mix]=mix;cases.push_back({"minimum_mix"+std::to_string(int(mix*100)),p});}
    cases.push_back({"init",d});for(float mix:{.5f,1.0f}) {p=d;p[Mix]=mix;cases.push_back({"init_mix"+std::to_string(int(mix*100)),p});}
    for(auto [param,name]:std::initializer_list<std::pair<Param,const char*>>{{Damage,"damage_max"},{Abyss,"abyss_max"},{Echo,"echo_max"}}) {p=d;p[param]=1;cases.push_back({name,p});}
    for(const auto& f:factoryPresets())if(!legacy || f.values[Voicing]==0)cases.push_back({slug(f.name),f.values});
    cases.push_back({"Frozen_Cello_capture",factoryPresets()[0].values,true});
    // Coverage matrix includes realistic levels and multiple spectra. Long IRs are measured separately.
    for(const auto& c:cases) {
        std::cout<<"audit "<<c.name<<std::endl;
        for(float level:{-18.0f,-6.0f})for(const std::string kind:{"silence","impulse","sine100","sine1000","sine5000","multitone","white","pink","piano","cello"}) {
            const auto input=signal(kind,48000,4,level);
            auto out=run(c,input,48000,256,2,metrics,stages,kind,level);
            if(level==-18&&(kind=="piano"||kind=="cello")) {
                if(!wav(dir,c.name+"_"+kind,out,48000)||!matched(dir,c.name+"_"+kind,out,48000,input.getNumSamples()))return 2;
            }
        }
    }
    for(const std::string kind:{"piano","cello"})if(!wav(dir,"input_"+kind,signal(kind,48000,4,-18),48000))return 2;
    // Sample-rate / segmentation matrix (full regression tests perform additional automation checks).
    for(double sr:{44100.0,48000.0,96000.0})for(int block:{1,64,257,1024})for(const auto& c:std::vector<Case>{{"rate_init",d},{"rate_dry",cases[2].p},{"rate_max",[]{auto v=auditDefaults();for(int k=0;k<=Mix;++k)v[size_t(k)]=1;v[Drive]=30;v[Feedback]=.94f;v[Decay]=35;return v;}()}}) {
        auto input=signal("multitone",sr,.5,-6);run(c,input,sr,block,.1,metrics,stages,"multitone",-6);
    }
    maps<<"macro,position,freeze_level,grain_ms,drift,drive_db,damage_blend,broken,damage_tone,echo_ms,feedback,echo_tone,width,echo_level,size,decay,damping,diffusion,reverb_mod,predelay,reverb_level,distance,motion_depth,motion_rate,mix,output_db,input_db\n";
    for(int k=Freeze;k<=Output;++k)for(float x:{0.0f,.25f,.5f,.75f,1.0f}) {
        p=d;p[size_t(k)]=k==Output?-24+30*x:x;const auto m=mapParameters(p,120);
        maps<<parameterInfo[size_t(k)].id<<','<<x<<','<<m.freezeLevel<<','<<m.grainMs<<','<<m.drift<<','<<db(m.damageDrive)<<','<<m.damageBlend<<','<<m.broken<<','<<m.damageTone<<','<<m.echoMs<<','<<m.feedback<<','<<m.echoTone<<','<<m.width<<','<<m.echoLevel<<','<<m.size<<','<<m.decay<<','<<m.damping<<','<<m.diffusion<<','<<m.reverbMod<<','<<m.predelay<<','<<m.reverbLevel<<','<<m.distance<<','<<m.motionDepth<<','<<m.motionRate<<','<<m.mix<<','<<db(m.output)<<','<<db(m.input)<<'\n';
    }
    // Audio macro sweep, isolating each effect (Abyss/Distance also have their documented coupled effects).
    for(int k=Freeze;k<=Motion;++k)for(float x:{0.0f,.25f,.5f,.75f,1.0f}) {
        p=flat;p[size_t(k)]=x;p[Mix]=1;Case c{"macro_"+std::string(parameterInfo[size_t(k)].id)+"_"+std::to_string(int(x*100)),p,k==Freeze};
        auto input=signal("cello",48000,4,-18);run(c,input,48000,256,3,metrics,stages,"cello",-18);
    }
    presets<<"name";for(const auto& info:parameterInfo)presets<<','<<info.id;presets<<'\n';
    for(const auto& f:factoryPresets()) if(!legacy || f.values[Voicing]==0) {presets<<f.name;for(float v:f.values)presets<<','<<v;presets<<'\n';}
    // Isolated distortion: unnormalised and common-RMS pairs, high-frequency alias probes.
    for(double sr:{44100.0,48000.0,96000.0})for(float level:{-18.0f,-6.0f})for(float x:{0.0f,.25f,.5f,.75f,1.0f})for(const std::string kind:{"sine1000","sine7301"}) {
        p=d;p[Damage]=x;DamageEngine damage;damage.prepare(sr);damage.configure(mapParameters(p,120));
        auto b=signal(kind,sr,2,level);
        for(int n=0;n<b.getNumSamples();++n) {auto v=damage.process({b.getSample(0,n),b.getSample(1,n)});b.setSample(0,n,v.l);b.setSample(1,n,v.r);}
        const auto name="damage_"+std::to_string(int(x*100))+"_"+kind+"_"+std::to_string(int(level))+"_"+std::to_string(int(sr));
        if(!wav(dir,name,b,sr)||!matched(dir,name,b,sr,b.getNumSamples()))return 2;
    }
    // Forty-second IRs give an adequate measured dynamic range; fitted T20 is not nominal RT60.
    for(double sr:{44100.0,48000.0,96000.0})for(float x:{0.0f,.25f,.5f,.75f,1.0f}) {
        p=d;p[Abyss]=x;ReverbEngine reverb;reverb.prepare(sr);reverb.configure(mapParameters(p,120));
        juce::AudioBuffer<float> b(2,int(sr*40));b.clear();
        for(int n=0;n<b.getNumSamples();++n){auto v=reverb.process(n==0?Stereo{dbGain(-6),dbGain(-6)}:Stereo{});b.setSample(0,n,v.l);b.setSample(1,n,v.r);}
        if(!wav(dir,"reverb_"+std::to_string(int(x*100))+"_"+std::to_string(int(sr)),b,sr))return 2;
    }
    // Representative, full-chain impulses distinguish algorithm decay from return masking.
    for(const auto& c:cases) {
        auto input=signal("impulse",48000,.01,-6);
        auto b=run(c,input,48000,256,30,metrics,stages,"long_impulse",-6);
        if(!wav(dir,"chain_ir_"+c.name,b,48000))return 2;
    }
    // Objective defect probes, identical before and after.
    p=d;p[Mix]=1;p[Damage]=p[Echo]=p[Abyss]=p[Motion]=p[Freeze]=0;p[Distance]=1;p[ReverbLevel]=0;p[Output]=0;
    {auto input=signal("impulse",48000,.01,-6);auto b=run({"muted_reverb_distance",p},input,48000,128,5,metrics,stages,"impulse",-6);if(!wav(dir,"muted_reverb_distance",b,48000))return 2;}
    p=d;p[Mix]=0;p[Output]=-12;Engine engine;engine.setParameters(p);engine.prepare(48000,1);
    juce::AudioBuffer<float> b=signal("sine100",48000,1,-6);
    for(int n=0;n<b.getNumSamples();++n){if(n==24000){p[Output]=0;engine.setParameters(p);}engine.process(b.getWritePointer(0,n),b.getWritePointer(1,n),1);}
    if(!wav(dir,"output_automation",b,48000))return 2;
    for(auto* f:{&metrics,&stages,&maps,&presets})if(!*f)return 3;
    std::cout<<"Audit complete: "<<dir.getFullPathName()<<std::endl;return 0;
}
}
