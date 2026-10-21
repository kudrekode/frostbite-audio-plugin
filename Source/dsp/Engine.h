#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace frostbite {
constexpr float pi = 3.14159265358979323846f;
struct Stereo {
    float l = 0, r = 0;
    Stereo operator+(Stereo b) const { return {l+b.l, r+b.r}; }
    Stereo operator*(float v) const { return {l*v, r*v}; }
};
inline float finite(float x) noexcept { return std::isfinite(x) ? std::clamp(x, -32.0f, 32.0f) : 0.0f; }
inline float dbGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }
inline Stereo blend(Stereo a, Stereo b, float t) { return a*(1-t)+b*t; }

class DelayLine {
public:
    void prepare(size_t samples) { data.assign(std::max(size_t(8), samples+4), 0); pos=0; }
    void reset() { std::fill(data.begin(), data.end(), 0.0f); pos=0; }
    void push(float x) noexcept { data[pos]=x; if (++pos==data.size()) pos=0; }
    float read(float delay) const noexcept;
private:
    std::vector<float> data;
    size_t pos=0;
};
struct Lowpass {
    float a=0, z=0;
    void tune(double sr, float hz) { a=std::exp(-2*pi*std::clamp(hz, 10.0f, float(sr*.45))/float(sr)); }
    float process(float x) noexcept { z=(1-a)*x+a*z; if(std::abs(z)<1e-25f) z=0; return z; }
    void reset() { z=0; }
};
struct DCBlock {
    float x1=0, y1=0, a=.995f;
    void prepare(double sr, float hz=15) { a=std::exp(-2*pi*hz/float(sr)); reset(); }
    float process(float x) noexcept { const float y=x-x1+a*y1; x1=x; y1=std::abs(y)<1e-25f?0:y; return y1; }
    void reset() { x1=y1=0; }
};
struct Allpass {
    DelayLine line;
    float delay=1;
    void prepare(double sr, float ms) { delay=float(sr)*ms*.001f; line.prepare(size_t(delay)+8); }
    float process(float x, float g) { float d=line.read(delay); float y=d-g*x; line.push(x+g*y); return y; }
    void reset() { line.reset(); }
};

// These stable indexes are shared by the plugin, presets, tests and CLI.
enum Param : size_t {
    Freeze, Damage, Distance, Echo, Abyss, Motion, Mix, Output,
    Capture, Bypass, Texture, Drift, FreezeLevel,
    EchoMs, EchoSync, EchoDivision, Feedback, EchoTone, Width, EchoLevel,
    Size, Decay, Damping, Diffusion, ReverbMod, Predelay, ReverbLevel,
    Drive, Character, DamageTone, DamageBlend, MotionRate, MotionDepth, Input,
    Count
};
using Values=std::array<float, Count>;
Values defaults();
Values sanitise(Values);
struct Mapped {
    float freezeLevel=0, grainMs=180, drift=.2f;
    float damageDrive=1, damageTone=6000, damageBlend=0, broken=0;
    float echoMs=400, feedback=.4f, echoTone=5000, width=1, echoLevel=0;
    float size=.5f, decay=6, damping=4000, diffusion=.7f, reverbMod=.2f, predelay=20, reverbLevel=0, reverbLevelTrim=1;
    float distance=0, motionDepth=0, motionRate=.15f, mix=.5f, output=1, input=1;
};
double divisionBeats(int index);
float delayMilliseconds(float ms, bool sync, int division, double bpm);
Mapped mapParameters(const Values&, double bpm);

class DamageEngine {
public:
    void prepare(double sampleRate);
    void reset();
    void configure(const Mapped&);
    Stereo process(Stereo) noexcept;
private:
    double sr=48000;
    float gain=1, compensation=1, amount=0, broken=0;
    std::array<float,2> previous{};
    std::array<Lowpass,2> tone;
    std::array<DCBlock,2> dc;
    static float shape(float x, bool hard);
    static float integral(float x, bool hard);
};

class EchoEngine {
public:
    void prepare(double sampleRate);
    void reset();
    void configure(float milliseconds, float feedback, float toneHz, float stereoWidth);
    Stereo process(Stereo) noexcept;
private:
    double sr=48000;
    std::array<DelayLine,2> lines;
    std::array<Lowpass,2> filters;
    std::array<DCBlock,2> dc;
    float currentDelay=1, nextDelay=1, requestedDelay=1, fade=1, fadeStep=1, feedback=.4f, width=1;
    bool initial=true;
};

class ReverbEngine {
public:
    void prepare(double sampleRate);
    void reset();
    void configure(const Mapped&);
    Stereo process(Stereo) noexcept;
private:
    double sr=48000;
    std::array<DelayLine,8> lines;
    std::array<Lowpass,8> filters;
    std::array<DCBlock,8> dc;
    std::array<float,8> lengths{}, gains{}, phase{};
    std::array<std::array<Allpass,4>,2> diffusers;
    std::array<DelayLine,2> pre;
    float diffusion=.7f, modDepth=0, preSamples=1, size=.5f;
    bool initial=true;
};

class FreezeEngine {
public:
    void prepare(double sampleRate);
    void reset();
    void configure(float grainMilliseconds, float driftAmount);
    Stereo process(Stereo, bool capture) noexcept;
    bool hasCapture() const { return frozenCount>8; }
    size_t capacity() const { return buffers[0][0].size(); }
private:
    struct Grain { double position=0; float age=0, length=1, speed=1; };
    double sr=48000;
    std::array<std::array<std::vector<float>,2>,2> buffers;
    std::array<Grain,4> grains;
    int rolling=0, frozen=1;
    size_t write=0, recorded=0, frozenStart=0, frozenCount=0;
    bool held=false;
    float envelope=0, attack=0, release=0, grainMs=180, drift=.2f, transition=1;
    Stereo previous{}, transitionFrom{};
    uint32_t seed=0x1874ab23u;
    float random() noexcept;
    void spawn(Grain&, float age=0) noexcept;
    float read(int channel, double position) const noexcept;
};

class MotionEngine {
public:
    void prepare(double sampleRate);
    void reset();
    void configure(float depth, float rate) { amount=depth; hz=rate; }
    Stereo process(Stereo) noexcept;
private:
    std::array<DelayLine,2> lines;
    double sr=48000, phase=0;
    float amount=0, hz=.15f;
};
class DistanceEngine {
public:
    void prepare(double sampleRate);
    void reset();
    void configure(float amount);
    Stereo process(Stereo) noexcept;
private:
    double sr=48000;
    float amount=0;
    std::array<Lowpass,2> tone;
    std::array<DCBlock,2> highpass;
    std::array<Allpass,2> scatter;
};

// Offline diagnostics retain the actual signal path, without adding work to the
// production process specialization. Callers own and preallocate these frames.
enum TraceStage : size_t {
    Dry, InputGain, Frozen, DamageOut, EchoReturn, LayerBeforeMotion,
    LayerAfterMotion, ReverbReturn, WetBeforeDistance, WetAfterDistance,
    WetAfterDC, Mixed, Protected, Final, TraceCount
};
struct TraceFrame { std::array<Stereo, TraceCount> stages{}; };

class Engine {
public:
    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void setParameters(const Values&, double bpm=120);
    void process(float* left, float* right, int samples) noexcept;
    void processOffline(float* left, float* right, int samples, TraceFrame* frames) noexcept;
    bool hasCapture() const { return freeze.hasCapture(); }
    float inputPeak() const { return inPeak; }
    float outputPeak() const { return outPeak; }
private:
    DamageEngine damage;
    EchoEngine echo;
    ReverbEngine reverb;
    FreezeEngine freeze;
    DistanceEngine distance;
    MotionEngine motion;
    std::array<DCBlock,2> dc;
    Values target=defaults(), smooth=defaults();
    Mapped mapped;
    double sr=48000, bpm=120;
    float smoothing=0, bypassFade=0, inPeak=0, outPeak=0;
    unsigned controlCounter=0;
    bool prepared=false, firstBlock=true;
    void configure();
    template<bool trace> void processImpl(float*, float*, int, TraceFrame*) noexcept;
};
} // namespace frostbite
