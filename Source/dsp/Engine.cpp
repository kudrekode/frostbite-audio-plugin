#include "Engine.h"
#include <limits>

namespace frostbite {
float DelayLine::read(float delay) const noexcept {
    delay=std::clamp(delay, 1.0f, float(data.size()-3));
    double p=double(pos)-double(delay);
    if(p<0) p+=double(data.size());
    const auto i=size_t(p);
    const auto j=i+1==data.size()?0:i+1;
    return data[i]+float(p-double(i))*(data[j]-data[i]);
}
Values legacyDefaults() {
    return { .65f,.12f,.22f,.25f,.55f,.20f,.55f,-3,
        0,0,180,.22f,0, 420,1,3,.46f,5500,1,1,
        .60f,9,.50f,.78f,.30f,28,1, 6,0,7500,1,.16f,.35f,0, 0,0,0 };
}
Values defaults() {
    auto v=legacyDefaults();
    v[Voicing]=1; v[FreezeMode]=1; v[FreezeRouting]=1;
    v[Freeze]=.18f; v[Damage]=.08f; v[Distance]=.08f; v[Echo]=.12f;
    v[Abyss]=.3f; v[Motion]=.15f; v[Mix]=.45f; v[Output]=0; v[Drive]=0;
    return v;
}
Values sanitise(Values v) {
    const auto d=v[Voicing]>.5f?defaults():legacyDefaults();
    for(size_t i=0;i<Count;++i) if(!std::isfinite(v[i])) v[i]=d[i];
    for(size_t i=0;i<=Mix;++i) v[i]=std::clamp(v[i],0.0f,1.0f);
    v[Output]=std::clamp(v[Output],-24.0f,6.0f);
    v[Capture]=v[Capture]>=.5f?1.0f:0.0f; v[Bypass]=v[Bypass]>=.5f?1.0f:0.0f;
    v[Texture]=std::clamp(v[Texture],40.0f,400.0f);
    v[Drift]=std::clamp(v[Drift],0.0f,1.0f);
    v[FreezeLevel]=std::clamp(v[FreezeLevel],-24.0f,6.0f);
    v[EchoMs]=std::clamp(v[EchoMs],20.0f,2000.0f);
    v[EchoSync]=v[EchoSync]>=.5f?1.0f:0.0f;
    v[EchoDivision]=std::clamp(std::round(v[EchoDivision]),0.0f,8.0f);
    v[Feedback]=std::clamp(v[Feedback],0.0f,.94f);
    v[EchoTone]=std::clamp(v[EchoTone],600.0f,12000.0f);
    v[Width]=std::clamp(v[Width],0.0f,1.5f);
    v[EchoLevel]=std::clamp(v[EchoLevel],0.0f,1.0f);
    v[Size]=std::clamp(v[Size],0.0f,1.0f);
    v[Decay]=std::clamp(v[Decay],.5f,35.0f);
    v[Damping]=std::clamp(v[Damping],0.0f,1.0f);
    v[Diffusion]=std::clamp(v[Diffusion],0.0f,1.0f);
    v[ReverbMod]=std::clamp(v[ReverbMod],0.0f,1.0f);
    v[Predelay]=std::clamp(v[Predelay],0.0f,150.0f);
    v[ReverbLevel]=std::clamp(v[ReverbLevel],0.0f,1.0f);
    v[Drive]=std::clamp(v[Drive],0.0f,30.0f);
    v[Character]=std::clamp(v[Character],0.0f,1.0f);
    v[DamageTone]=std::clamp(v[DamageTone],600.0f,16000.0f);
    v[DamageBlend]=std::clamp(v[DamageBlend],0.0f,1.0f);
    v[MotionRate]=std::clamp(v[MotionRate],.03f,2.0f);
    v[MotionDepth]=std::clamp(v[MotionDepth],0.0f,1.0f);
    v[Input]=std::clamp(v[Input],-24.0f,12.0f);
    for(auto key:{Voicing,FreezeMode,FreezeRouting}) v[key]=v[key]>=.5f?1.0f:0.0f;
    return v;
}
double divisionBeats(int i) {
    constexpr double beats[]{.25,.5,1.0/3.0,1,.75,2.0/3.0,2,1.5,4};
    return beats[std::clamp(i,0,8)];
}
float delayMilliseconds(float ms,bool sync,int division,double bpm) {
    if(!std::isfinite(bpm) || bpm<20 || bpm>400) bpm=120;
    return std::clamp(sync?float(60000.0/bpm*divisionBeats(division)):ms,20.0f,12000.0f);
}
Mapped mapParameters(const Values& p,double bpm) {
    Mapped m; m.v2=p[Voicing]>.5f; m.freezeAmount=p[Freeze]; m.freezeRouting=p[FreezeRouting];
    m.freezeLevel=std::pow(p[Freeze],1.25f)*dbGain(p[FreezeLevel]);
    m.grainMs=p[Texture]; m.drift=std::min(1.0f,p[Drift]+.25f*p[Motion]);
    m.damageDrive=dbGain(p[Drive]+p[Damage]*24);
    m.damageBlend=p[DamageBlend]*std::pow(p[Damage],.7f);
    m.broken=std::max(p[Character],std::clamp((p[Damage]-.55f)/.45f,0.0f,1.0f));
    m.damageTone=p[DamageTone]*(1-.50f*p[Damage]);
    m.echoMs=delayMilliseconds(p[EchoMs],p[EchoSync]>.5f,int(std::round(p[EchoDivision])),bpm);
    m.feedback=std::min(.94f,p[Feedback]*(.65f+.35f*p[Echo]));
    m.echoTone=p[EchoTone]*(1-.55f*p[Echo]);
    m.width=p[Width]; m.echoLevel=p[EchoLevel]*std::pow(p[Echo],1.15f)*.85f;
    m.size=std::clamp(.65f*p[Size]+.35f*p[Abyss],0.0f,1.0f);
    m.decay=std::clamp(p[Decay]*(.35f+1.15f*p[Abyss]),.3f,45.0f);
    m.damping=1000+11000*std::pow(1-p[Damping],2.0f)*(1-.45f*p[Abyss]);
    m.diffusion=std::clamp(p[Diffusion]*(.7f+.3f*p[Abyss]),0.0f,1.0f);
    m.reverbMod=std::min(1.0f,p[ReverbMod]+.35f*p[Motion]);
    m.predelay=p[Predelay];
    m.reverbLevel=p[ReverbLevel]*std::pow(p[Abyss],.8f);
    m.reverbLevelTrim=p[ReverbLevel];
    m.distance=p[Distance]; m.motionDepth=p[MotionDepth]*p[Motion];
    m.motionRate=p[MotionRate]; m.mix=p[Mix];
    m.output=dbGain(p[Output]); m.input=dbGain(p[Input]);
    if(m.v2) {
        m.freezeLevel=std::pow(p[Freeze],.9f)*dbGain(p[FreezeLevel])*1.45f;
        m.damageDrive=dbGain(.65f*p[Drive]+32*p[Damage]);
        m.damageBlend=p[DamageBlend]*std::pow(p[Damage],.85f);
        m.broken=std::max(p[Character],.65f*std::clamp((p[Damage]-.35f)/.65f,0.0f,1.0f));
        m.damageTone=p[DamageTone]*(1-.12f*p[Damage]);
        m.echoLevel=p[EchoLevel]*std::pow(p[Echo],.95f)*.9f;
        m.echoTone=p[EchoTone]*(1-.3f*p[Echo]);
        m.feedback=std::min(.94f,p[Feedback]*(.55f+.45f*p[Echo]));
        m.size=std::clamp(.55f*p[Size]+.45f*p[Abyss],0.0f,1.0f);
        m.decay=std::clamp(p[Decay]*(.55f+1.4f*p[Abyss]),.3f,60.0f);
        // Damping controls colour independently; Abyss no longer progressively hides the top.
        m.damping=3500+13500*std::pow(1-p[Damping],1.4f);
        m.diffusion=std::clamp(.2f+.8f*p[Diffusion],0.0f,1.0f);
        m.reverbLevel=p[ReverbLevel]*std::pow(p[Abyss],1.1f)*1.15f;
        m.motionDepth=p[MotionDepth]*std::pow(p[Motion],.8f);
    }
    return m;
}

void DamageEngine::prepare(double s) { sr=s; for(auto& d:dc) d.prepare(sr); reset(); }
void DamageEngine::reset() { previous.fill(0); for(auto& f:tone) f.reset(); for(auto& f:emphasis) f.reset(); for(auto& d:dc) d.reset(); }
void DamageEngine::configure(const Mapped& m) {
    v2=m.v2; gain=m.damageDrive; amount=m.damageBlend; broken=m.broken;
    compensation=v2?.62f/std::pow(std::max(1.0f,gain),.18f):1/std::sqrt(std::max(1.0f,gain));
    for(auto& f:emphasis) f.tune(sr,1400);
    for(auto& f:tone) f.tune(sr,m.damageTone);
}
float DamageEngine::shape(float x,bool hard) {
    return hard?x/(1+std::abs(x)):x/std::sqrt(1+x*x);
}
float DamageEngine::integral(float x,bool hard) {
    return hard?std::abs(x)-std::log1p(std::abs(x)):std::sqrt(1+x*x);
}
// Bounded asymmetric clip plus a single fold inside the transition region.
// Analytic first-order ADAA integrates every segment; double precision avoids
// cancellation at low levels. There is no oversampling buffer or host latency.
double DamageEngine::ruinShape(double x) {
    constexpr double limit=1.2;
    return std::clamp(x+.18,-.72,.92)-.18
        +(std::abs(x)<limit?.12*std::sin(3.141592653589793*x/limit):0);
}
double DamageEngine::ruinIntegral(double x) {
    const double t=x+.18;
    const double clip=t<-.72?-.72*t-.5*.72*.72:t>.92?.92*t-.5*.92*.92:.5*t*t;
    constexpr double limit=1.2, k=3.141592653589793/limit;
    const double fold=std::abs(x)<limit?(1-std::cos(k*x))/k:2/k;
    return clip-.18*x+.12*fold;
}
Stereo DamageEngine::process(Stereo in) noexcept {
    float v[]{in.l,in.r};
    for(int c=0;c<2;++c) {
        const float low=v2?emphasis[size_t(c)].process(v[c]):0;
        const float input=v2?v[c]+.6f*broken*(v[c]-low):v[c];
        const float x=std::clamp(input*gain,-100.0f,100.0f), old=previous[size_t(c)];
        auto adaa=[&](bool hard,float bias) {
            const float a=x+bias,b=old+bias;
            return (std::abs(a-b)<.01f?shape((a+b)*.5f,hard):(integral(a,hard)-integral(b,hard))/(a-b))-shape(bias,hard);
        };
        const double delta=double(x)-old;
        const float warm=v2?float(std::abs(delta)<1e-5?shape((x+old)*.5f,false):
            (std::sqrt(1+double(x)*x)-std::sqrt(1+double(old)*old))/delta):adaa(false,0);
        const float harsh=v2?float(std::abs(delta)<1e-5?ruinShape((double(x)+old)*.5):
            (ruinIntegral(x)-ruinIntegral(old))/delta):adaa(true,.36f)*1.65f;
        previous[size_t(c)]=x;
        const float wet=tone[size_t(c)].process(dc[size_t(c)].process((warm*(1-broken)+harsh*broken)*compensation));
        v[c]=v[c]*(1-amount)+wet*amount;
    }
    return {v[0],v[1]};
}

void EchoEngine::prepare(double s) {
    sr=s; for(auto& l:lines) l.prepare(size_t(sr*12.1)); for(auto& d:dc) d.prepare(sr,25);
    fadeStep=1/float(sr*.06); reset();
}
void EchoEngine::reset() {
    for(auto& l:lines) l.reset(); for(auto& f:filters) f.reset(); for(auto& d:dc) d.reset();
    initial=true; fade=1;
}
void EchoEngine::configure(float ms,float fb,float hz,float w) {
    requestedDelay=std::clamp(ms*.001f*float(sr),1.0f,float(sr*12));
    if(initial) { currentDelay=nextDelay=requestedDelay; initial=false; }
    feedback=std::clamp(fb,0.0f,.94f); width=std::clamp(w,0.0f,1.5f);
    for(auto& f:filters) f.tune(sr,hz);
}
Stereo EchoEngine::process(Stereo in) noexcept {
    if(fade>=1 && std::abs(requestedDelay-currentDelay)>.5f) { nextDelay=requestedDelay; fade=0; }
    const float a=std::min(1.0f,fade);
    float l=lines[0].read(currentDelay)*(1-a)+lines[0].read(nextDelay)*a;
    float r=lines[1].read(currentDelay)*(1-a)+lines[1].read(nextDelay)*a;
    if(fade<1) { fade+=fadeStep; if(fade>=1) currentDelay=nextDelay; }
    // Mid enters L; the side signal enters R. A mono signal therefore bounces.
    const float mid=(in.l+in.r)*.5f, side=(in.r-in.l)*.5f;
    const float fl=filters[0].process(dc[0].process(l));
    const float fr=filters[1].process(dc[1].process(r));
    // tanh(x) has slope <= 1, so this feedback loop remains contractive.
    lines[0].push(mid+feedback*std::tanh(fr));
    lines[1].push(side+feedback*std::tanh(fl));
    const float centre=(l+r)*.5f, spread=(l-r)*.5f*width;
    return {centre+spread,centre-spread};
}

void ReverbEngine::prepare(double s) {
    sr=s;
    for(auto& l:lines) l.prepare(size_t(sr*.35));
    for(auto& d:dc) d.prepare(sr,35);
    for(auto& p:pre) p.prepare(size_t(sr*.16));
    constexpr float times[]{4.7f,7.9f,12.7f,17.3f};
    for(size_t c=0;c<2;++c) for(size_t i=0;i<4;++i) diffusers[c][i].prepare(sr,times[i]*(c==0?1.0f:1.113f));
    reset();
}
void ReverbEngine::reset() {
    for(auto& l:lines) l.reset(); for(auto& p:pre) p.reset();
    for(auto& f:filters) f.reset(); for(auto& d:dc) d.reset();
    for(auto& ch:diffusers) for(auto& a:ch) a.reset();
    for(size_t i=0;i<8;++i) phase[i]=float(i)*.79f;
    initial=true;
}
void ReverbEngine::configure(const Mapped& m) {
    v2=m.v2;
    constexpr float times[]{29.7f,37.1f,41.1f,43.7f,53.1f,61.7f,71.9f,83.3f};
    if(initial) { size=m.size; initial=false; }
    // Continuous slow time movement rather than resetting buffers on automation.
    size+=(v2?1-std::exp(-32.0f/(float(sr)*.7f)):.004f)*(m.size-size);
    for(size_t i=0;i<8;++i) {
        lengths[i]=times[i]*.001f*float(sr)*(v2?.8f+2.7f*size:.55f+2.0f*size);
        gains[i]=std::min(.9985f,std::pow(.001f,lengths[i]/float(sr)/std::max(.3f,m.decay)));
        filters[i].tune(sr,m.damping);
    }
    diffusion=.15f+.58f*m.diffusion;
    modDepth=m.reverbMod*float(sr)*(v2?.0008f:.00048f);
    preSamples=std::max(1.0f,m.predelay*.001f*float(sr));
}
Stereo ReverbEngine::process(Stereo in) noexcept {
    float v[]{in.l,in.r};
    for(size_t c=0;c<2;++c) {
        pre[c].push(v[c]); v[c]=pre[c].read(preSamples);
        for(auto& ap:diffusers[c]) v[c]=ap.process(v[c],diffusion);
    }
    std::array<float,8> taps{};
    float sum=0;
    for(size_t i=0;i<8;++i) {
        phase[i]+=2*pi*(.071f+float(i)*.013f)/float(sr);
        if(phase[i]>2*pi) phase[i]-=2*pi;
        const float raw=lines[i].read(lengths[i]+modDepth*std::sin(phase[i]));
        const float dark=filters[i].process(raw);
        taps[i]=dc[i].process(v2?.78f*raw+.22f*dark:dark);
        if(v2) taps[i]*=.5f*(1+dc[i].a); // normalized DC blocker: loop gain cannot exceed unity
        sum+=taps[i];
    }
    if(v2) {
        auto feedback=taps;
        // Normalized Hadamard: every line feeds every other line each trip.
        for(size_t stride=1;stride<8;stride*=2)
            for(size_t base=0;base<8;base+=2*stride)
                for(size_t j=0;j<stride;++j) {
                    const float a=feedback[base+j],b=feedback[base+j+stride];
                    feedback[base+j]=a+b; feedback[base+j+stride]=a-b;
                }
        for(size_t i=0;i<8;++i) {
            const float injection=(i%2==0?v[0]:v[1])*(i<4?.30f:-.30f);
            lines[i].push(injection+feedback[i]*.353553390593f*gains[i]);
        }
        return {.58f*(taps[0]+taps[2]-taps[4]-taps[6])+.16f*v[0],
                .58f*(taps[1]+taps[3]-taps[5]-taps[7])+.16f*v[1]};
    }
    for(size_t i=0;i<8;++i) {
        const float injection=(i%2==0?v[0]:v[1])*(i<4?.24f:-.24f);
        // Householder reflection I - 2 vv^T is orthogonal (v_i = 1/sqrt(8)).
        lines[i].push(injection+(taps[i]-.25f*sum)*gains[i]);
    }
    return { .44f*(taps[0]+taps[2]-taps[4]-taps[6]), .44f*(taps[1]+taps[3]-taps[5]-taps[7]) };
}

void FreezeEngine::prepare(double s) {
    sr=s;
    for(auto& b:buffers) for(auto& ch:b) ch.assign(size_t(sr*1.5)+1,0);
    attack=1/float(sr*.06); release=1/float(sr*.18); reset();
}
void FreezeEngine::reset() {
    for(auto& b:buffers) for(auto& ch:b) std::fill(ch.begin(),ch.end(),0.0f);
    rolling=0; frozen=1; write=recorded=frozenStart=frozenCount=0;
    held=false; envelope=0; transition=1; previous={}; transitionFrom={}; seed=0x1874ab23u;
}
void FreezeEngine::configure(float ms,float d,bool v2) { modern=v2; grainMs=std::clamp(ms,40.0f,400.0f); drift=std::clamp(d,0.0f,1.0f); }
float FreezeEngine::random() noexcept {
    seed^=seed<<13; seed^=seed>>17; seed^=seed<<5;
    return float(seed&0x00ffffff)/float(0x01000000);
}
void FreezeEngine::spawn(Grain& g,float age) noexcept {
    g.length=std::max(32.0f,grainMs*.001f*float(sr));
    g.age=age*g.length;
    g.speed=std::pow(2.0f,(random()-.5f)*drift*.035f);
    g.position=double(random())*double(frozenCount);
}
float FreezeEngine::read(int channel,double position) const noexcept {
    if(frozenCount<2) return 0;
    if(position>=double(frozenCount) || position<0) {
        position=std::fmod(position,double(frozenCount));
        if(position<0) position+=double(frozenCount);
    }
    const auto i=size_t(position);
    const auto cap=buffers[size_t(frozen)][size_t(channel)].size();
    const auto a=(frozenStart+i)%cap;
    const auto b=(frozenStart+(i+1)%frozenCount)%cap;
    const auto& data=buffers[size_t(frozen)][size_t(channel)];
    // A short taper makes the circular capture seam inaudible to long grains.
    const float edge=float(std::min(position,double(frozenCount)-position));
    const float taper=modern?std::max(1.0f,std::min(float(sr*.005),float(frozenCount)*.1f)):float(sr*.005);
    const float seam=std::min(1.0f,edge/taper);
    return (data[a]+float(position-double(i))*(data[b]-data[a]))*seam;
}
Stereo FreezeEngine::process(Stereo in,bool capture) noexcept {
    const auto cap=capacity();
    if(capture && !held && recorded>8) {
        std::swap(rolling,frozen);
        frozenCount=recorded; frozenStart=recorded==cap?write:0;
        write=0; recorded=0;
        transitionFrom=previous; transition=0;
        for(size_t i=0;i<4;++i) spawn(grains[i],float(i)*.25f);
    }
    held=capture;
    buffers[size_t(rolling)][0][write]=in.l; buffers[size_t(rolling)][1][write]=in.r;
    if(++write==cap) write=0; recorded=std::min(cap,recorded+1);
    envelope=capture?std::min(1.0f,envelope+attack):std::max(0.0f,envelope-release);
    Stereo out{};
    if(frozenCount>8 && envelope>0) {
        float weight=0;
        for(auto& g:grains) {
            const float w=.5f-.5f*std::cos(2*pi*g.age/g.length);
            out.l+=w*read(0,g.position); out.r+=w*read(1,g.position); weight+=w;
            g.position+=g.speed;
            if(++g.age>=g.length) spawn(g);
        }
        out=out*(envelope/std::max(.5f,weight));
    }
    if(transition<1) { out=blend(transitionFrom,out,transition); transition=std::min(1.0f,transition+attack); }
    previous=out;
    return out;
}

void LiveTextureEngine::prepare(double s) {
    sr=s; for(auto& channel:data) channel.assign(size_t(sr*1.5)+1,0); reset();
}
void LiveTextureEngine::reset() {
    for(auto& channel:data) std::fill(channel.begin(),channel.end(),0.0f);
    write=0; started=false; seed=0x4167ac23u;
}
void LiveTextureEngine::spawn(Grain& g,float age) noexcept {
    auto random=[&] { seed^=seed<<13; seed^=seed>>17; seed^=seed<<5; return float(seed&0xffffff)/16777216.0f; };
    g.length=std::max(32.0f,grainMs*.001f*float(sr)); g.age=age*g.length;
    g.speed=std::pow(2.0f,(random()-.5f)*wander*.12f);
    // The reader remains behind the writer for the entire grain, including at 96 kHz.
    const double lag=sr*(.08+.65*random())+double(g.length)*.15;
    g.position=double(write)-lag;
    if(g.position<0) g.position+=double(data[0].size());
}
Stereo LiveTextureEngine::process(Stereo input) noexcept {
    const auto cap=data[0].size();
    data[0][write]=input.l; data[1][write]=input.r;
    if(!started) { for(size_t i=0;i<4;++i) spawn(grains[i],float(i)*.25f); started=true; }
    Stereo out{}; float weights=0;
    for(auto& g:grains) {
        const auto a=size_t(g.position), b=(a+1==cap?0:a+1);
        const float fraction=float(g.position-double(a));
        const float w=.5f-.5f*std::cos(2*pi*g.age/g.length);
        out.l+=(data[0][a]+fraction*(data[0][b]-data[0][a]))*w;
        out.r+=(data[1][a]+fraction*(data[1][b]-data[1][a]))*w;
        weights+=w; g.position+=g.speed;
        if(g.position>=double(cap)) g.position-=double(cap);
        if(++g.age>=g.length) spawn(g);
    }
    if(++write==cap) write=0;
    return out*(1/std::max(.5f,weights));
}

void MotionEngine::prepare(double s) { sr=s; for(auto& l:lines) l.prepare(size_t(sr*.03)); reset(); }
void MotionEngine::reset() { for(auto& l:lines) l.reset(); phase=0; }
Stereo MotionEngine::process(Stereo in) noexcept {
    lines[0].push(in.l); lines[1].push(in.r);
    phase+=2*pi*hz/sr; if(phase>=2*pi) phase-=2*pi;
    const float base=.008f*float(sr), depth=.0025f*amount*float(sr);
    Stereo shifted{lines[0].read(base+depth*float(std::sin(phase))),lines[1].read(base+depth*float(std::sin(phase+1.7)))};
    return blend(in,shifted,amount*.5f);
}
void DistanceEngine::prepare(double s) {
    sr=s; scatter[0].prepare(sr,11.3f); scatter[1].prepare(sr,14.9f);
    for(auto& f:highpass) f.prepare(sr,70); reset();
}
void DistanceEngine::reset() { for(auto& f:tone) f.reset(); for(auto& d:highpass) d.reset(); for(auto& a:scatter) a.reset(); }
void DistanceEngine::configure(float a,bool modern) {
    amount=a; v2=modern;
    const float hz=16000*std::pow(v2?.10f:.065f,amount);
    for(auto& f:tone) f.tune(sr,hz);
}
Stereo DistanceEngine::process(Stereo in) noexcept {
    float v[]{in.l,in.r};
    for(size_t c=0;c<2;++c) {
        const float dark=tone[c].process(highpass[c].process(v[c]));
        const float soft=scatter[c].process(dark,.60f);
        v[c]=v[c]*(1-amount)+(dark*(1-.42f*amount)+soft*.42f*amount)*amount;
    }
    return {v[0],v[1]};
}

void Engine::prepare(double s,int maximumBlockSize) {
    (void)maximumBlockSize;
    sr=std::clamp(s,8000.0,384000.0);
    damage.prepare(sr); echo.prepare(sr); reverb.prepare(sr); freeze.prepare(sr); liveTexture.prepare(sr);
    distance.prepare(sr); motion.prepare(sr); for(auto& f:dc) f.prepare(sr,10);
    smoothing=1-std::exp(-1.0f/(float(sr)*.025f));
    prepared=true; reset();
}
void Engine::reset() {
    if(!prepared) return;
    damage.reset(); echo.reset(); reverb.reset(); freeze.reset(); liveTexture.reset(); holdBlend=0; textureActivity=0; lastOutput={}; voiceCorrection={}; distance.reset(); motion.reset();
    for(auto& f:dc) f.reset();
    smooth=target; bypassFade=target[Bypass]; controlCounter=0; inPeak=outPeak=0; firstBlock=true;
}
void Engine::setParameters(const Values& p,double tempo) { target=sanitise(p); bpm=tempo; }
void Engine::configure() {
    mapped=mapParameters(smooth,bpm);
    damage.configure(mapped); echo.configure(mapped.echoMs,mapped.feedback,mapped.echoTone,mapped.width);
    reverb.configure(mapped); freeze.configure(mapped.grainMs,mapped.drift,mapped.v2);
    liveTexture.configure(mapped.grainMs,mapped.drift);
    distance.configure(mapped.distance,mapped.v2); motion.configure(mapped.motionDepth,mapped.motionRate);
}
void Engine::process(float* left,float* right,int samples) noexcept {
    processImpl<false>(left,right,samples,nullptr);
}
void Engine::processOffline(float* left,float* right,int samples,TraceFrame* frames) noexcept {
    if(frames==nullptr) { process(left,right,samples); return; }
    processImpl<true>(left,right,samples,frames);
}
template<bool trace>
void Engine::processImpl(float* left,float* right,int samples,TraceFrame* frames) noexcept {
    if(!prepared || left==nullptr || right==nullptr) return;
    if(firstBlock) { smooth=target; bypassFade=target[Bypass]; configure(); holdBlend=target[Capture]>.5f || target[FreezeMode]<.5f?1.0f:0.0f; textureActivity=1-holdBlend; lastVoicing=mapped.v2; firstBlock=false; }
    inPeak=outPeak=0;
    for(int i=0;i<samples;++i) {
        Stereo dry{finite(left[i]),finite(right[i])};
        if constexpr(trace) frames[i].stages[Dry]=dry;
        inPeak=std::max(inPeak,std::max(std::abs(dry.l),std::abs(dry.r)));
        for(size_t p=0;p<Count;++p) smooth[p]+=smoothing*(target[p]-smooth[p]);
        // Switches and note division are discrete, all continuous values are smoothed.
        smooth[Voicing]=target[Voicing];
        smooth[EchoSync]=target[EchoSync]; smooth[EchoDivision]=target[EchoDivision];
        if((controlCounter++ & 31u)==0) configure();
        Stereo source=dry*mapped.input;
        if constexpr(trace) frames[i].stages[InputGain]=source;
        const Stereo held=freeze.process(source,target[Capture]>.5f);
        const Stereo live=mapped.v2?liveTexture.process(source):Stereo{};
        const float holding=target[Capture]>.5f || target[FreezeMode]<.5f?1.0f:0.0f;
        holdBlend+=smoothing*(holding-holdBlend);
        Stereo ice=(mapped.v2?blend(live,held,holdBlend):held)*mapped.freezeLevel;
        if constexpr(trace) frames[i].stages[Frozen]=ice;
        const float active=holding<.5f || (target[Capture]>.5f && freeze.hasCapture())?1.0f:0.0f;
        textureActivity+=smoothing*(active-textureActivity);
        const float direct=mapped.v2?1-.8f*mapped.freezeAmount*textureActivity:1;
        const float route=mapped.v2?mapped.freezeRouting:0;
        Stereo coloured=damage.process(source*direct+ice*route);
        if constexpr(trace) frames[i].stages[DamageOut]=coloured;
        Stereo repeats=echo.process(coloured);
        if constexpr(trace) frames[i].stages[EchoReturn]=repeats;
        Stereo layer=coloured*(mapped.v2?1.0f:.82f)+ice*(mapped.v2?1-route:.80f)+repeats*mapped.echoLevel;
        if constexpr(trace) frames[i].stages[LayerBeforeMotion]=layer;
        layer=motion.process(layer);
        if constexpr(trace) frames[i].stages[LayerAfterMotion]=layer;
        Stereo tail=reverb.process(layer);
        if constexpr(trace) frames[i].stages[ReverbReturn]=tail;
        // Distance changes the direct/diffuse ratio as well as the bandwidth.
        Stereo wet=mapped.v2 ? layer*(1-.12f*mapped.reverbLevel-.08f*mapped.distance)+tail*mapped.reverbLevel
            : layer*(1-.38f*mapped.reverbLevel-.22f*mapped.distance)
            +tail*(mapped.reverbLevel*(.8f+.35f*mapped.distance)+.22f*mapped.distance*mapped.reverbLevelTrim);
        if constexpr(trace) frames[i].stages[WetBeforeDistance]=wet;
        wet=distance.process(wet);
        if constexpr(trace) frames[i].stages[WetAfterDistance]=wet;
        const Stereo blocked{dc[0].process(wet.l),dc[1].process(wet.r)};
        if(!mapped.v2) wet=blocked;
        if constexpr(trace) frames[i].stages[WetAfterDC]=wet;
        auto protect=[](float x) {
            x=finite(x);
            const float a=std::abs(x);
            return a<=.92f?x:std::copysign(.92f+.08f*std::tanh((a-.92f)/.08f),x);
        };
        // Correlated linear mix, with exact original samples at Mix=0.
        Stereo out=blend(dry,wet,mapped.mix)*mapped.output;
        if constexpr(trace) frames[i].stages[Mixed]=out;
        out={protect(out.l),protect(out.r)};
        if constexpr(trace) frames[i].stages[Protected]=out;
        // Decaying continuity correction prevents a step when changing architectures.
        // It is dormant during steady voicing, so legacy renders remain unchanged.
        if(lastVoicing!=mapped.v2) {
            voiceCorrection={lastOutput.l-out.l,lastOutput.r-out.r};lastVoicing=mapped.v2;
        }
        if(voiceCorrection.l!=0 || voiceCorrection.r!=0) {
            out={std::clamp(out.l+voiceCorrection.l,-1.0f,1.0f),std::clamp(out.r+voiceCorrection.r,-1.0f,1.0f)};
            voiceCorrection=voiceCorrection*(1-smoothing);
            if(std::abs(voiceCorrection.l)+std::abs(voiceCorrection.r)<1e-7f)voiceCorrection={};
        }
        if(target[Mix]==0 && smooth[Mix]<1e-5f && target[Output]==0 && std::abs(smooth[Output])<1e-5f) out=dry;
        bypassFade+=smoothing*(target[Bypass]-bypassFade);
        if(target[Bypass]>.5f && bypassFade>.9999f) out=dry;
        else out=blend(out,dry,bypassFade);
        left[i]=finite(out.l); right[i]=finite(out.r); lastOutput={left[i],right[i]};
        if constexpr(trace) frames[i].stages[Final]={left[i],right[i]};
        outPeak=std::max(outPeak,std::max(std::abs(left[i]),std::abs(right[i])));
    }
}
} // namespace frostbite
