# FROSTBITE 1.0.0 — completion report

Build and verification date: **20 October 2026**. Target: Windows x64.

## Delivered

- Genuine x64 VST3 audio effect and native JUCE standalone.
- Separate portable DSP modules: four-voice granular capture, dark cross-feedback echo, eight-line diffuse reverb, Warm/Broken ADAA damage, distance filtering/diffusion, and slow stereo motion.
- Six coordinated macros; 34 stable automatable controls, with one additional VST3 wrapper program parameter.
- Custom scalable vector editor, audio-driven display/meters, engine detail pages, reset/fine adjustment, bypass, preset controls.
- Eight designed factory presets, readable user-preset XML, DAW state restore.
- Native WAV rendering CLI and a file-audition host with device selection and the same editor.
- Thirteen DSP test groups, plugin integration tests, independent VST3 loading/processing host.
- Ten generated 24-bit stereo diagnostic WAVs, listening guide, installer, packaging script, reproducible CMake build, Studio One checklist and future macOS notes.

## What actually compiled and ran

The complete documented `.\scripts\build.ps1` workflow completed successfully. Toolchain: Visual Studio 2022 Build Tools, MSVC 19.42.34436, Windows SDK 10.0.22621, C++20, JUCE 9.0.4 at commit `3fb5cbc0340758596dac39f7c8de47d562b4896e`.

Release outputs built: `Frostbite_VST3`, `Frostbite_Standalone`, `frostbite_dsp_tests`, `frostbite_tests`, `frostbite_host_check`, `frostbite_render`. PE inspection confirms **8664 / x64, PE32+, DLL**. Its import table contains Windows system libraries, with no separately installed JUCE or MSVC DLL requirement.

CTest: **2/2 test executables passed**. The DSP executable contains **13 passing test groups**, covering:

1. Stereo granular capture, sustained output during silence, seam/capture continuity, finite bounds, release and fixed capacity.
2. Empty/short capture, repeated hold and rapid capture edges.
3. Exact impulse timing, alternating echo channels and feedback decay.
4. Tempo, dotted/triplet timing, missing-BPM fallback and maximum sync length.
5. Maximum echo feedback and abrupt delay-time changes.
6. Stereo reverb impulse tail, decay dependence and DC.
7. Distortion harmonics, distinct modes and exact dry blend.
8. Distance gain retention and stereo motion.
9. 44.1/48/96 kHz, 64/128/512/1024 samples, extremes, automation, silence and NaN/Infinity input.
10. **Zero heap allocations** in engine processing, including capture and automation.
11. Fifty seconds of maximum-decay reverb excited by deterministic noise and a sweep.
12. Identical output under different block segmentation.
13. Exact dry mix/bypass and zero engine-level mappings.

Plugin integration passed defaults (within float rounding), percent text entry, eight preset restores, preset file saving/loading, DAW state roundtrip, capture reopening unarmed, malformed state rejection, sample-rate changes, mono-to-stereo and rejected surround buses, bypass, editor construction and six rendered editor snapshots. Screenshots were visually reviewed; overlapping macro captions were corrected.

The independent **VST3 host actually loaded the compiled bundle**, verified effect metadata and stereo buses, found 35 exported parameters, processed audio across 12 rate/block configurations including variable lengths, serialized/restored state, and created the editor. Maximum measured output in that run: **0.118258**.

Early test assumptions corrected during development: JUCE manifest-only scanning reports channel counts as zero until instantiation; APVTS defaults require a small float-rounding tolerance. These changes did not waive audio acceptance criteria. A missing renderer include and editable percentage conversion were also corrected before the final successful run.

## Diagnostic render measurements

All wet files are 22 seconds at 48 kHz; dry files are four seconds. These are synthetic diagnostics, not subjective listening results.

| Preset | Peak amplitude | RMS | Mean/DC | Last sample above -60 dBFS |
|---|---:|---:|---:|---:|
| Frozen Cello | 0.166150 | 0.024468 | 7.98e-9 | 13.527 s |
| Ruined Piano | 0.207311 | 0.021188 | -3.90e-7 | 9.547 s |
| Glacial Echo | 0.279368 | 0.000260 | 1.62e-7 | 5.250 s |
| Industrial Fog | 0.140005 | 0.018130 | 4.13e-9 | 8.585 s |
| Distant Cathedral | 0.209491 | 0.026920 | 1.55e-9 | 15.382 s |

The maximum diagnostic peak is about -11.1 dBFS. Offline elapsed-time ratios varied with machine load: roughly 1.8–2.3% in the first render run and 6.6–13.4% during the final workflow. These are wall-clock observations, not an isolated CPU benchmark or a guarantee that a 64-sample device deadline will always be met.

## Studio One / live audio status

Studio One **6.6.4.102451 Win x64** was launched. Its scanner log records FROSTBITE being scanned and completing with result `0x00000000`, **class data: YES**. User-folder VST3 discovery therefore passed on this installation.

Insertion into a song, live audio monitoring, DAW automation recording, song save/reopen and DAW offline export have **not yet been verified**. Studio One startup was slow and reported unavailable external MIDI devices. The user stopped Computer Use with the physical Escape key before song-level testing; all desktop interaction then stopped. The separate file-audition app launched with the generated WAV, but UI inspection timed out waiting for application access; live device playback is **unverified**.

The independent VST3 host and CLI audio results above are verified. No claim is made that the effect was subjectively auditioned, that all Studio One checks passed, or that macOS was built.

## Exact locations and commands

Workspace:

`C:\Users\jadcr\Documents\Code\frostbite-plugin`

Built VST3 bundle:

`C:\Users\jadcr\Documents\Code\frostbite-plugin\build\Frostbite_artefacts\Release\VST3\FROSTBITE.vst3`

Actual binary:

`C:\Users\jadcr\Documents\Code\frostbite-plugin\build\Frostbite_artefacts\Release\VST3\FROSTBITE.vst3\Contents\x86_64-win\FROSTBITE.vst3`

Installed bundle (copy verified by SHA256):

`C:\Users\jadcr\AppData\Local\Programs\Common\VST3\FROSTBITE.vst3`

Binary SHA256:

`D699C2B2C09310BBAD737CEA395ECD9057272CB9D37F044126E96A0F00C96347`

Rebuild from the workspace:

```powershell
.\scripts\build.ps1
```

First installation:

```powershell
.\scripts\install.ps1 -Scope User
```

Update the installation already created during this build, preserving a backup:

```powershell
.\scripts\install.ps1 -Scope User -Replace
```

File audition:

```powershell
.\build\frostbite_render_artefacts\Release\frostbite_render.exe --audition
```

Evidence: `build/final-validation.log`, `build/Testing/Temporary/LastTest.log`, `build/host-results.txt`, `build/render-metrics.txt`, `build/qa/*.png`, and `renders/*.wav`. The release ZIP includes the validation logs.

## Remaining work and limits

Follow the [Studio One checklist](STUDIO_ONE_CHECKLIST.md) for actual song and device validation. Perform the first listening/tuning session with real instruments. Captured audio is not serialized; the 120-second reported tail cannot represent an indefinitely held capture. Nonlinear aliasing is reduced with ADAA, not eliminated. File-audition resampling is simple linear interpolation; use matching sample rates for critical comparison. The build is unsigned. Review JUCE licensing before distribution; no commercial license was purchased. macOS, AU, code signing, notarization, graph editing and oversampling remain future work.
