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
            Engine e; e.prepare(sr,block); auto p=defaults(); e.setParameters(p);
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
        Engine e; e.prepare(48000,512); auto p=defaults();
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
    test("Mix zero / bypass are exact, echo/reverb level zero contribute nothing",[] {
        Engine a,b; a.prepare(48000,128); b.prepare(48000,128);
        auto p=defaults(); p[Mix]=0; p[Output]=0; a.setParameters(p); p[Mix]=1; p[Bypass]=1;b.setParameters(p);
        std::array<float,128> l{},r{},lb{},rb{};
        for(size_t n=0;n<128;++n) l[n]=r[n]=lb[n]=rb[n]=.3f*std::sin(float(n)*.07f);
        a.process(l.data(),r.data(),128);b.process(lb.data(),rb.data(),128);
        for(size_t n=0;n<128;++n) require(l[n]==.3f*std::sin(float(n)*.07f) && lb[n]==l[n],"dry/bypass mismatch");
        p=defaults();p[EchoLevel]=p[ReverbLevel]=0;
        auto m=mapParameters(p,120);require(m.echoLevel==0 && m.reverbLevel==0,"zero levels not respected");
    });
    std::cout<<passed<<" passed, "<<failed<<" failed\n"; return failed?1:0;
}
