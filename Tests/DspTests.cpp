#include "dsp/Engine.h"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <limits>
#include <stdexcept>
#include <string>

static std::atomic<bool> watchAllocations{false};
static std::atomic<int> allocations{0};
void* operator new(std::size_t n) {
    if(watchAllocations.load(std::memory_order_relaxed)) ++allocations;
    if(void* p=std::malloc(n?n:1)) return p; throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete[](void* p,std::size_t) noexcept { ::operator delete(p); }
using namespace frostbite;
void require(bool ok,const std::string& message) { if(!ok) throw std::runtime_error(message); }
double energy(Stereo s) { return double(s.l)*s.l+double(s.r)*s.r; }
bool good(Stereo s) { return std::isfinite(s.l) && std::isfinite(s.r) && std::abs(s.l)<32 && std::abs(s.r)<32; }
int main() {
    int passed=0,failed=0;
    auto test=[&](const char* name,auto run) {
        try {run(); ++passed; std::cout<<"PASS "<<name<<"\n";}
        catch(const std::exception& e) {++failed; std::cout<<"FAIL "<<name<<": "<<e.what()<<"\n";}
    };
    test("Freeze captures stereo audio, sustains, releases and stays bounded",[] {
        FreezeEngine f; f.prepare(48000); f.configure(180,.3f);
        const auto capacity=f.capacity();
        for(int i=0;i<72000;++i) f.process({.3f*std::sin(2*pi*220*i/48000),.19f*std::sin(2*pi*330*i/48000)},false);
        double held=0,difference=0; float maxStep=0; Stereo previous{};
        for(int i=0;i<96000;++i) {
            auto s=f.process({},true); require(good(s),"invalid held output");
            if(i>24000) { held+=energy(s); difference+=std::abs(s.l-s.r); }
            maxStep=std::max(maxStep,std::abs(s.l-previous.l)); previous=s;
        }
        require(f.hasCapture(),"capture missing"); require(held/72000>0.0005,"sustain inaudible");
        require(difference/72000>.01,"stereo collapsed"); require(maxStep<.12f,"capture/grain discontinuity");
        double tail=0;
        for(int i=0;i<24000;++i) { auto s=f.process({},false); if(i>12000) tail+=energy(s); }
        require(tail<1e-10,"release did not finish"); require(f.capacity()==capacity,"buffer capacity changed");
    });
    test("Freeze is edge-triggered and supports short/empty captures",[] {
        FreezeEngine f; f.prepare(44100); f.configure(40,1);
        for(int n=0;n<2000;++n) require(good(f.process({},true)),"empty capture invalid");
        f.process({},false);
        for(int n=0;n<256;++n) f.process({.2f,-.15f},false);
        for(int n=0;n<12000;++n) require(good(f.process({},true)),"short capture invalid");
        for(int n=0;n<20000;++n) require(good(f.process({},(n/130)%2==0)),"rapid capture invalid");
    });
    test("Echo exact timing, stereo alternation, decaying repeats",[] {
        EchoEngine e; e.prepare(48000); e.configure(100,.6f,12000,1);
        double before=0,firstL=0,firstR=0,secondL=0,secondR=0,third=0;
        for(int n=0;n<20000;++n) {
            auto s=e.process(n==0?Stereo{.5f,.5f}:Stereo{});
            if(n<4800) before+=energy(s);
            if(n>=4800 && n<5000) {firstL+=s.l*s.l;firstR+=s.r*s.r;}
            if(n>=9600 && n<9900) {secondL+=s.l*s.l;secondR+=s.r*s.r;}
            if(n>=14400 && n<14900) third+=energy(s);
        }
        require(before==0,"echo arrived early"); require(firstL>.2 && firstR<1e-10,"first repeat not left");
        require(secondR>.015 && secondL<1e-9,"second repeat not right");
        require(secondR<firstL && third<secondR,"feedback not decaying");
    });
    test("Echo BPM, dotted/triplet divisions and fallback",[] {
        require(std::abs(delayMilliseconds(10,true,3,120)-500)<.01,"quarter timing");
        require(std::abs(delayMilliseconds(10,true,7,120)-750)<.01,"dotted timing");
        require(std::abs(delayMilliseconds(10,true,2,120)-166.6667f)<.01,"triplet timing");
        require(delayMilliseconds(10,true,3,0)==500,"fallback tempo");
        require(delayMilliseconds(10,true,8,20)==12000,"slow whole note capacity");
        require(delayMilliseconds(317,false,0,90)==317,"free time");
    });
    test("Echo maximum feedback and abrupt time changes remain finite",[] {
        EchoEngine e; e.prepare(96000); e.configure(20,.94f,1000,1.5f);
        float peak=0;
        for(int n=0;n<96000*4;++n) {
            if(n%12000==0) e.configure(n%24000?20.0f:1700.0f,.94f,600,1.5f);
            auto s=e.process({.3f*std::sin(n*.1f),.3f*std::sin(n*.13f)});
            require(good(s),"unstable feedback"); peak=std::max({peak,std::abs(s.l),std::abs(s.r)});
        }
        require(peak<2,"excessive echo amplitude");
    });
    test("Reverb diffuse stereo tail, decay control and DC",[] {
        auto render=[](float decay) {
            ReverbEngine r; r.prepare(48000); Mapped m; m.decay=decay; m.size=.55f; m.reverbMod=.3f; m.diffusion=.8f; r.configure(m);
            double late=0,early=0,dc=0,stereo=0;
            for(int n=0;n<48000*6;++n) {
                auto s=r.process(n==0?Stereo{.7f,.1f}:Stereo{}); require(good(s),"invalid reverb");
                if(n>4800 && n<48000) early+=energy(s);
                if(n>96000) {late+=energy(s);dc+=s.l;stereo+=std::abs(s.l-s.r);}
            }
            require(early>1e-4,"no impulse tail"); require(std::abs(dc/(48000*4))<1e-4,"DC in tail");
            require(stereo>1e-6,"mono tail"); return late;
        };
        const double shortTail=render(1),longTail=render(18);
        require(longTail>shortTail*10,"decay parameter ineffective");
    });
    test("Damage changes harmonics, modes differ, blend zero is exact",[] {
        auto render=[](float mode,float amount) {
            DamageEngine d; d.prepare(48000); Mapped m; m.damageDrive=20; m.damageBlend=amount; m.broken=mode; m.damageTone=12000; d.configure(m);
            double fundamental=0,third=0;
            for(int n=0;n<48000;++n) {
                float x=.2f*std::sin(2*pi*250*n/48000); auto s=d.process({x,x});
                require(good(s),"invalid saturation");
                if(amount==0) require(s.l==x,"dry damage changes samples");
                if(n>12000) { fundamental+=s.l*std::sin(2*pi*250*n/48000);third+=s.l*std::sin(2*pi*750*n/48000); }
            }
            return std::abs(third/fundamental);
        };
        render(0,0); const auto warm=render(0,1),broken=render(1,1);
        require(warm>.03 && broken>.03,"missing distortion harmonics");
        require(std::abs(warm-broken)>.005,"modes are identical");
    });
    test("Distance changes spectrum without muting, Motion changes stereo",[] {
        DistanceEngine d; d.prepare(48000); d.configure(1);
        MotionEngine m; m.prepare(48000); m.configure(.8f,.3f);
        double distant=0,stereo=0;
        for(int n=0;n<96000;++n) {
            const float x=.2f*std::sin(2*pi*400*n/48000);
            auto a=d.process({x,x}),b=m.process({x,x});
            if(n>24000) {distant+=energy(a);stereo+=std::abs(b.l-b.r);}
        }
        require(distant/72000>.001,"distance mutes source"); require(stereo/72000>.001,"motion is ineffective");
    });
    test("All sample rates/block sizes, silence, extreme automation, nonfinite input",[] {
        for(double sr:{44100.0,48000.0,96000.0}) for(int block:{64,128,512,1024}) {
            Engine e; e.prepare(sr,block); auto p=legacyDefaults(); e.setParameters(p);
            std::vector<float> l(size_t(block),0),r(size_t(block),0);
            e.process(l.data(),r.data(),block);
            for(auto x:l) require(x==0,"nonzero idle output");
            for(int b=0;b<90;++b) {
                for(size_t k=0;k<=Mix;++k) p[k]=(b%3==0?1.0f:0.0f);
                p[Capture]=float(b%2); p[EchoMs]=b%2?20.0f:2000.0f; p[Feedback]=.94f;
                p[Decay]=35; p[Drive]=30; p[Output]=6; p[Character]=float(b%2);
                e.setParameters(p,b%2?60:178);
                const int count=b%2?block:std::max(1,block-17);
                for(int i=0;i<count;++i) { l[size_t(i)]=.6f*std::sin(float(i+b*block)*.03f); r[size_t(i)]=l[size_t(i)]*.5f; }
                if(b==40) {l[0]=std::numeric_limits<float>::infinity();r[0]=std::numeric_limits<float>::quiet_NaN();}
                e.process(l.data(),r.data(),count);
                for(int i=0;i<count;++i) require(std::isfinite(l[size_t(i)]) && std::abs(l[size_t(i)])<=1.0001f,"invalid engine output");
            }
        }
    });
    test("No heap allocations in process, including capture and automation",[] {
        Engine e; e.prepare(48000,512); auto p=legacyDefaults();
        std::array<float,512> l{},r{};
        allocations=0; watchAllocations=true;
        for(int n=0;n<400;++n) {
            p[Capture]=float(n%2); p[Abyss]=float(n%10)*.1f; e.setParameters(p);
            e.process(l.data(),r.data(),512);
        }
        watchAllocations=false; require(allocations==0,"audio callback allocated");
    });
    test("Maximum reverb decay survives noise, sweep and a long tail",[] {
        ReverbEngine r;r.prepare(48000);Mapped m;m.decay=45;m.size=1;m.diffusion=1;m.reverbMod=1;m.damping=12000;r.configure(m);
        uint32_t random=71;double late=0;float peak=0;
        for(int n=0;n<48000*50;++n) {
            random=random*1664525u+1013904223u;
            const float noise=float(random&65535)/32768.0f-1;
            const float t=float(n)/48000;
            const Stereo in=n<96000?Stereo{noise*.12f,.1f*std::sin(2*pi*(40*t+2000*t*t))}:Stereo{};
            const auto out=r.process(in);require(good(out),"maximum reverb unstable");
            peak=std::max({peak,std::abs(out.l),std::abs(out.r)});
            if(n>48000*45) late+=energy(out);
        }
        require(peak<1,"excessive reverb gain");require(late<1e-3,"long tail failed to decay");
    });
    test("Variable block segmentation produces identical audio",[] {
        Engine a,b;a.prepare(48000,512);b.prepare(48000,1024);
        constexpr size_t length=32000;
        std::vector<float> al(length),ar(length),bl(length),br(length);
        uint32_t random=173;
        for(size_t n=0;n<length;++n) {
            random=random*1664525u+1013904223u;
            al[n]=bl[n]=(float(random&65535)/32768.0f-1)*.15f;
            ar[n]=br[n]=.2f*std::sin(float(n)*.051f);
        }
        for(size_t n=0;n<length;n+=128) a.process(al.data()+n,ar.data()+n,int(std::min(size_t(128),length-n)));
        for(size_t n=0;n<length;) {const size_t count=std::min(size_t(17+n%983),length-n);b.process(bl.data()+n,br.data()+n,int(count));n+=count;}
        for(size_t n=0;n<length;++n) require(std::abs(al[n]-bl[n])<1e-7f && std::abs(ar[n]-br[n])<1e-7f,"block boundary dependence");
    });
    test("Offline stage trace is sample-identical to production processing",[] {
        Engine realtime,offline; realtime.prepare(48000,257); offline.prepare(48000,257);
        std::array<float,257> l{},r{},ol{},orr{};
        std::array<TraceFrame,257> frames{};
        auto p=legacyDefaults();
        for(int block=0;block<200;++block) {
            p[Capture]=block>=80 && block<140?1.0f:0.0f;
            p[Damage]=float(block%40)/39.0f;
            p[Abyss]=float(block%50)/49.0f;
            realtime.setParameters(p); offline.setParameters(p);
            const int count=1+block%257;
            for(int n=0;n<count;++n) {
                l[size_t(n)]=ol[size_t(n)]=.17f*std::sin(float(block*257+n)*.071f);
                r[size_t(n)]=orr[size_t(n)]=.13f*std::cos(float(block*257+n)*.033f);
            }
            realtime.process(l.data(),r.data(),count);
            offline.processOffline(ol.data(),orr.data(),count,frames.data());
            for(int n=0;n<count;++n) {
                const auto i=size_t(n);
                require(l[i]==ol[i] && r[i]==orr[i],"trace changed production samples");
                require(frames[i].stages[Final].l==ol[i] && frames[i].stages[Final].r==orr[i],"trace missed output");
                for(const auto stage:frames[i].stages) require(good(stage),"invalid stage snapshot");
            }
        }
    });
    test("Mix zero / bypass are exact, echo/reverb level zero contribute nothing",[] {
        Engine a,b; a.prepare(48000,128); b.prepare(48000,128);
        auto p=legacyDefaults(); p[Mix]=0; p[Output]=0; a.setParameters(p); p[Mix]=1; p[Bypass]=1;b.setParameters(p);
        std::array<float,128> l{},r{},lb{},rb{};
        for(size_t n=0;n<128;++n) l[n]=r[n]=lb[n]=rb[n]=.3f*std::sin(float(n)*.07f);
        a.process(l.data(),r.data(),128);b.process(lb.data(),rb.data(),128);
        for(size_t n=0;n<128;++n) require(l[n]==.3f*std::sin(float(n)*.07f) && lb[n]==l[n],"dry/bypass mismatch");
        p=legacyDefaults();p[EchoLevel]=p[ReverbLevel]=0;
        auto m=mapParameters(p,120);require(m.echoLevel==0 && m.reverbLevel==0,"zero levels not respected");
    });
    test("Dry output automation reaches unity smoothly",[] {
        for(double sr:{44100.0,48000.0,96000.0}) {
            Engine e; e.prepare(sr,256); auto p=legacyDefaults();
            p[Mix]=0; p[Output]=-24; e.setParameters(p);
            std::array<float,256> l{},r{}; l.fill(.25f); r.fill(-.125f);
            e.process(l.data(),r.data(),256);
            float previous=l.back(),maxStep=0;
            p[Output]=0; e.setParameters(p);
            for(int block=0;block<int(sr)/256;++block) {
                l.fill(.25f); r.fill(-.125f); e.process(l.data(),r.data(),256);
                for(const auto x:l) { maxStep=std::max(maxStep,std::abs(x-previous)); previous=x; }
            }
            require(maxStep<.005f,"unity shortcut bypassed output smoothing");
            require(l.back()==.25f && r.back()==-.125f,"unity did not settle exactly");
        }
    });
    test("Reverb fine level zero mutes the returned tail with Distance active",[] {
        Engine e; e.prepare(48000,128); auto p=legacyDefaults();
        p[Distance]=.8f; p[ReverbLevel]=0; p[Abyss]=1; p[Echo]=0; p[Motion]=0;
        p[Damage]=0; p[Freeze]=0; p[Mix]=1; p[Output]=0; e.setParameters(p);
        std::array<float,128> l{},r{};
        std::array<TraceFrame,128> frames{};
        double internalTail=0,leakedTail=0;
        for(int block=0;block<375;++block) {
            l.fill(0); r.fill(0); if(block==0) l[0]=r[0]=.5f;
            e.processOffline(l.data(),r.data(),128,frames.data());
            for(const auto& f:frames) {
                internalTail+=energy(f.stages[ReverbReturn]);
                if(block>0) leakedTail+=energy(f.stages[WetBeforeDistance]);
            }
        }
        require(internalTail>1e-5,"test failed to excite reverb");
        require(leakedTail<1e-15,"Distance bypassed the reverb return level");
    });
    test("Linear Mix preserves midpoint for correlated, partial and frozen signals",[] {
        for(int scenario:{0,1,2}) {
            Engine wet,half; wet.prepare(48000,128); half.prepare(48000,128);
            auto p=legacyDefaults(); p[Damage]=scenario==2?1.0f:0.0f; p[Drive]=scenario==2?30.0f:6.0f;
            p[Freeze]=scenario==2?1.0f:0.0f; p[FreezeLevel]=6;
            p[Echo]=scenario==1?.7f:0.0f;
            p[Abyss]=scenario==1?1.0f:0.0f; p[Distance]=0; p[Motion]=0; p[Output]=0;
            p[Mix]=1; wet.setParameters(p); p[Mix]=.5f; half.setParameters(p);
            std::array<float,128> l{},r{},hl{},hr{},dryL{},dryR{};
            double xy=0,xx=0,yy=0;
            uint32_t random=8173;
            for(int block=0;block<800;++block) {
                if(scenario==2 && block==200) {
                    p[Capture]=1; p[Mix]=1; wet.setParameters(p); p[Mix]=.5f; half.setParameters(p);
                }
                for(size_t n=0;n<l.size();++n) {
                    const auto time=float(block*128+int(n));
                    random=random*1664525u+1013904223u;
                    const float noise=(float(random&65535)/32768.0f-1)*.03f;
                    dryL[n]=l[n]=hl[n]=scenario==2?noise:.03f*std::sin(time*.071f);
                    dryR[n]=r[n]=hr[n]=scenario==1?.02f*std::cos(time*.103f):dryL[n];
                }
                wet.process(l.data(),r.data(),128); half.process(hl.data(),hr.data(),128);
                for(size_t n=0;n<l.size();++n) {
                    require(std::abs(hl[n]-.5f*(dryL[n]+l[n]))<1e-7f
                        && std::abs(hr[n]-.5f*(dryR[n]+r[n]))<1e-7f,"mix midpoint is not linear");
                    if(block>400) { xy+=double(dryL[n])*l[n]; xx+=double(dryL[n])*dryL[n]; yy+=double(l[n])*l[n]; }
                }
            }
            const double correlation=xy/std::sqrt(xx*yy);
            if(scenario==0) require(correlation>.99,"correlated mix case not correlated");
            if(scenario==2) require(std::abs(correlation)<.5,"frozen mix case insufficiently decorrelated");
        }
    });
    test("Macro progression and documented endpoint ranges",[] {
        auto p=legacyDefaults();
        for(size_t macro=Freeze;macro<=Motion;++macro) {
            Mapped prior{};
            for(int step=0;step<=4;++step) {
                p=legacyDefaults(); p[macro]=float(step)*.25f; const auto m=mapParameters(p,120);
                if(step>0) {
                    switch(macro) {
                        case Freeze: require(m.freezeLevel>prior.freezeLevel,"Freeze dead zone"); break;
                        case Damage: require(m.damageDrive>prior.damageDrive && m.damageBlend>prior.damageBlend
                            && m.damageTone<prior.damageTone,"Damage progression"); break;
                        case Distance: require(m.distance>prior.distance,"Distance progression"); break;
                        case Echo: require(m.echoLevel>prior.echoLevel && m.feedback>prior.feedback
                            && m.echoTone<prior.echoTone,"Echo progression"); break;
                        case Abyss: require(m.reverbLevel>prior.reverbLevel && m.size>prior.size
                            && m.decay>prior.decay && m.damping<prior.damping,"Abyss progression"); break;
                        case Motion: require(m.motionDepth>prior.motionDepth && m.drift>prior.drift
                            && m.reverbMod>prior.reverbMod,"Motion progression"); break;
                        default: break;
                    }
                }
                prior=m;
            }
        }
        p=legacyDefaults(); p[Freeze]=p[Damage]=p[Echo]=p[Abyss]=p[Motion]=0;
        auto m=mapParameters(p,120);
        require(m.freezeLevel==0 && m.damageBlend==0 && m.echoLevel==0 && m.reverbLevel==0
            && m.motionDepth==0,"macro zero return endpoints");
        p[Freeze]=p[Damage]=p[Echo]=p[Abyss]=p[Motion]=1;
        p[Drive]=30; p[Size]=1; p[Decay]=35; p[MotionDepth]=1; p[Feedback]=.94f;
        m=mapParameters(p,120);
        require(std::abs(20*std::log10(m.damageDrive)-54)<1e-4f && m.broken==1
            && m.damageBlend==1,"Damage cannot reach documented maximum");
        require(m.freezeLevel==1 && m.reverbLevel==1 && m.size==1 && m.decay==45
            && m.motionDepth==1 && m.feedback==.94f,"maximum macro endpoints");
        for(float level:{0.0f,.25f,.5f,.75f,1.0f}) {
            p[Mix]=level; m=mapParameters(p,120); require(m.mix==level,"Mix normalized endpoint/progression");
        }
        float previous=0;
        for(float db:{-24.0f,-16.5f,-9.0f,-1.5f,6.0f}) {
            p[Output]=db; m=mapParameters(p,120);
            require(m.output>previous && std::abs(20*std::log10(m.output)-db)<1e-4f,"Output gain calibration");
            previous=m.output;
        }
    });

    test("V2 neutral wet and dry unity at every supported rate",[] {
        for(double rate:{44100.0,48000.0,96000.0}) for(float mix:{0.0f,.5f,1.0f}) {
            Engine e; auto p=defaults();for(size_t k=Freeze;k<=Motion;++k)p[k]=0;p[Mix]=mix;
            e.setParameters(p);e.prepare(rate,128); std::array<float,128> l{},r{};
            for(int b=0;b<50;++b) {
                for(int i=0;i<128;++i){l[size_t(i)]=.25f*std::sin(float(b*128+i)*.071f);r[size_t(i)]=-.7f*l[size_t(i)];}
                const auto a=l,c=r;e.process(l.data(),r.data(),128);
                require(l==a && r==c,"neutral wet has hidden attenuation or phase change");
            }
        }
    });
    test("V2 live grains retain stereo, replace source, and clear on silence",[] {
        for(double rate:{44100.0,48000.0,96000.0}) {
            LiveTextureEngine live;live.prepare(rate);live.configure(400,1);
            double early=0,replaced=0,cleared=0;
            for(int n=0;n<int(rate*5);++n) {
                const auto y=live.process(n<int(rate)?Stereo{.2f,0}:n<int(2*rate)?Stereo{0,.2f}:Stereo{});
                require(good(y),"bad live output");
                if(n>rate*.8 && n<rate) early+=y.l*y.l;
                if(n>rate*1.8 && n<2*rate) replaced+=y.r*y.r;
                if(n>4*rate) cleared+=energy(y);
            }
            require(early>1 && replaced>1,"live texture failed to follow real source");
            require(cleared==0,"live source retained stale material after silence");
        }
    });
    test("V2 held texture feeds Damage and Echo only in processed routing",[] {
        auto render=[](bool routed,bool damaged) {
            Engine e;auto p=defaults();p[Freeze]=1;p[FreezeMode]=0;p[FreezeRouting]=routed?1.0f:0.0f;
            p[Damage]=damaged?1.0f:0.0f;p[Character]=1;p[Echo]=.8f;p[EchoSync]=0;p[EchoMs]=100;
            p[Distance]=p[Abyss]=p[Motion]=0;p[Mix]=1;e.setParameters(p);e.prepare(48000,128);
            std::array<float,128> l{},r{};std::array<TraceFrame,128> trace{};double shape=0,echo=0;
            for(int b=0;b<1500;++b) {
                if(b==500){p[Capture]=1;e.setParameters(p);}
                for(int n=0;n<128;++n)l[size_t(n)]=r[size_t(n)]=b<500?.1f*std::sin(float(b*128+n)*.04f):0;
                e.processOffline(l.data(),r.data(),128,trace.data());
                if(b>1000) for(const auto& frame:trace) {
                    shape+=energy(frame.stages[DamageOut]);echo+=energy(frame.stages[EchoReturn]);
                }
            }
            return std::array<double,2>{shape,echo};
        };
        const auto clean=render(false,true),routed=render(true,true),unshaped=render(true,false);
        require(clean[0]==0 && clean[1]<1e-6,"clean grains leaked into nonlinear route");
        require(routed[0]>1 && routed[1]>1,"held grains did not reach Damage/Echo");
        require(std::abs(routed[0]-unshaped[0])>1,"Damage cannot transform held texture");
    });
    test("V2 safety/automation and no allocation across full rate/block matrix",[] {
        for(double rate:{44100.0,48000.0,96000.0})for(int block:{1,64,128,256,512,1024}) {
            Engine e;auto p=defaults();e.setParameters(p);e.prepare(rate,block);
            std::vector<float> l(size_t(block),0),r(size_t(block),0);
            allocations=0;bool bounded=true;watchAllocations=true;
            for(int b=0;b<std::max(120,int(rate*2)/block);++b) {
                const int tick=int(double(b*block)/(rate*.08));
                for(size_t k=Freeze;k<=Mix;++k)p[k]=tick%3?1.0f:0.0f;
                p[Capture]=float(tick%2);p[FreezeMode]=float((tick/3)%2);p[FreezeRouting]=float((tick/7)%2);
                p[Bypass]=tick%11==0?1.0f:0.0f;p[Input]=12;p[Output]=6;p[Drive]=30;p[Feedback]=.94f;p[Decay]=35;
                p[EchoMs]=tick%2?20.0f:2000.0f;e.setParameters(p);
                for(int n=0;n<block;++n){l[size_t(n)]=.5f*std::sin(float(b*block+n)*.117f);r[size_t(n)]=-.7f*l[size_t(n)];}
                e.process(l.data(),r.data(),block);
                for(int n=0;n<block;++n)bounded=bounded && std::isfinite(l[size_t(n)]) && std::isfinite(r[size_t(n)]) && std::abs(l[size_t(n)])<=1 && std::abs(r[size_t(n)])<=1;
            }
            watchAllocations=false;require(allocations==0,"V2 callback allocated");require(bounded,"V2 automation unstable");
        }
    });
    test("V2 maximum Warm and Broken remain distinct after level matching",[] {
        DamageEngine warm,broken;warm.prepare(48000);broken.prepare(48000);
        auto p=defaults();p[Damage]=1;p[Character]=0;warm.configure(mapParameters(p,120));
        p[Character]=1;broken.configure(mapParameters(p,120));double aa=0,bb=0,ab=0;
        for(int n=0;n<48000;++n){const float x=.125f*std::sin(2*pi*1000*n/48000);const auto a=warm.process({x,x}),b=broken.process({x,x});if(n>12000){aa+=a.l*a.l;bb+=b.l*b.l;ab+=a.l*b.l;}}
        require(ab/std::sqrt(aa*bb)<.999,"Warm/Broken difference is only gain");
    });
    test("V2 maximum hall remains finite and decays over a long silent tail",[] {
        ReverbEngine reverb;reverb.prepare(48000);auto p=defaults();p[Abyss]=p[Size]=p[ReverbMod]=1;p[Damping]=0;p[Decay]=35;
        reverb.configure(mapParameters(p,120));double early=0,late=0;float peak=0;
        for(int n=0;n<48000*90;++n) {
            const auto y=reverb.process(n<48000?Stereo{.12f*std::sin(n*.157f),.1f*std::sin(n*.077f)}:Stereo{});
            require(good(y),"V2 long reverb invalid");peak=std::max({peak,std::abs(y.l),std::abs(y.r)});
            if(n>48000*5 && n<48000*10)early+=energy(y);
            if(n>48000*85)late+=energy(y);
        }
        require(peak<2,"V2 reverb excessive raw energy");require(late<early*.001,"V2 tail fails to decay");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed\n"; return failed?1:0;
}
