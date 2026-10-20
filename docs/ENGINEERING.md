# FROSTBITE engine notes

## Signal path

Dry input is retained separately. The wet path is:

```
Input trim -> warm/broken Damage -> direct layer ----+
Input trim -> rolling capture -> granular Freeze ---+-> Motion -> Abyss -> Distance -> DC filter
Damage -> cross-feedback Echo ----------------------+                               |
Dry input ----------------------------------------------------------> linear Mix -> Output
```

The output safety knee is linear up to 0.92 amplitude, then approaches 1.0 smoothly. This is overload protection; feedback stability comes from bounded loop gains. Default output trim is -3 dB. Dry Mix=0 with Output=0 dB and settled bypass reproduce finite input samples exactly, including signals above full scale. Invalid input values become zero. Finite external input is bounded at +/-32.

The plugin reports zero latency. Motion, predelay, and granular playback are musical delays in the wet path. Damage uses first-order antiderivative antialiasing (ADAA), with a half-sample averaging character in the wet nonlinear path. It does not claim oversampling or complete removal of aliasing. A linear correlated wet/dry law avoids the +3 dB centre gain of an equal-power mix.

## Real-time contract

`Source/dsp` is portable C++20 without JUCE or GUI dependencies. Preparation allocates every ring buffer. Processing uses fixed arrays, scalar state, and preallocated vectors. Capture swaps buffer roles; it never copies the 1.5-second window in the callback. There is no process-time allocation, file access, locking, GUI work, or network access. DSP tests instrument heap allocation, including capture and automation.

The APVTS parameter atomics are cached on construction. Continuous controls use 25 ms one-pole smoothing; engine coefficient updates happen every 32 samples. Capture, sync, division and bypass targets have explicit discrete handling. Delay-time transitions crossfade over 60 ms; room size has additional smoothing. The callback accepts arbitrary block lengths without scratch resizing. JUCE disables denormals around it; internal filters also clear tiny state.

UI visualization reads 128 independent lock-free atomic samples plus atomic peak levels, at 24 Hz only while the editor is open. It cannot stall audio. The GUI uses vector drawing and scales from 800x560 to 1600x1120.

## Capture semantics

On a **rising edge**, Capture freezes up to the immediately preceding 1.5 seconds of stereo input. Four Hann-windowed grains play with common stereo read positions, interpolated samples, randomized offsets, and small pitch variations. Grain sizes run 40–400 ms. Capture attack/crossfade is 60 ms and release is 180 ms. Sustained high Capture does not recapture. Release, play more material, and press Capture again to recapture.

Capturing before more than eight samples have arrived captures nothing. It does not silently capture later. The editor reports this condition; release and recapture after playing. Very short windows remain valid but can sound tonal. Presets and DAW state store controls, **not audio**; Capture reopens off. Offline renders that require identical freezes must include source audio and capture automation.

## Echo

The mono sum enters the left delay and the stereo difference enters the right delay. Cross-feedback alternates the mid signal between channels. Side information remains audible rather than collapsing the input. Width transforms the returned mid/side balance. Feedback includes DC removal, lowpass filtering and `tanh`, whose small-signal slope never exceeds one. Feedback is capped at 0.94. No reverb-to-echo feedback loop exists.

Free time: 20–2000 ms. Sync: sixteenth, eighth, eighth triplet, quarter, dotted eighth, quarter triplet, half, dotted quarter, whole. A fixed 12.1-second allocation supports a whole note at 20 BPM. Invalid or missing BPM falls back to 120. Transport stops do not discard tails; prepare/reset clears buffers.

## Abyss

Two input diffusion chains, each containing four allpasses, feed eight unequal delay lines. The feedback mixing matrix is the orthogonal Householder reflection `I - 2 vv^T`, with every component of `v` equal to `1/sqrt(8)`. Per-line loop gains derive from delay length and target RT60, and are below one. Damping and 35 Hz highpass filters suppress harsh and low-frequency buildup. Phase-offset modulation perturbs read positions by less than 0.5 ms.

Reported decay values are design RT60 targets; filters, modulation and input spectrum change the measured tail. The host tail estimate is 120 seconds, but held granular audio has an unbounded duration until Capture is released.

## Macro mapping

Exact expressions live in `mapParameters`, in `Source/dsp/Engine.cpp`. For a macro normalized to `m`:

| Macro | Coordinated behavior |
|---|---|
| Freeze | Layer gain `m^1.25`, times frozen-level trim; independently captured |
| Damage | Extra `24m` dB drive, nonlinear blend `m^0.7`, tone reduced by `1-0.5m`, automatic Warm-to-Broken morph above 55%; gain compensation `1/sqrt(drive)` |
| Distance | Exponential 16 kHz to 1.04 kHz lowpass, 70 Hz bandwidth restriction, up to 42% allpass diffusion; direct contribution reduced and diffuse contribution raised |
| Echo | Wet `0.85m^1.15`, feedback scaled by `0.65+0.35m`, tone scaled by `1-0.55m` |
| Abyss | Wet `m^0.8`, size combined 65% fine control / 35% macro, decay multiplier `0.35+1.15m`, increased diffusion and darkness |
| Motion | Pitch-delay depth times fine depth; also raises granular drift and reverb modulation |

Fine engine level zero mutes that engine's returned path; it may keep running internally so automation can reveal its tail. Distance can add diffuse signal independently of the Abyss macro. Damage happens before Echo; its internal feedback saturation has a separate stability/decay purpose.

## State and reuse

34 stable parameter IDs, version hint 1. XML state root `FrostbiteState`, schema 1. `.frostbite` presets use readable XML. Invalid state roots/schema are rejected, unknown parameters ignored, missing parameters defaulted and numeric values clamped. No captured audio or absolute sample paths are stored. Factory names and mappings are in `Parameters.cpp`.

DSP engines can be extracted into a future PatchForge node system; no graph runtime is required today. Memory is sample-rate bounded (maximum supported engine rate 384 kHz). Host support tested only at the rates listed in the completion report.
