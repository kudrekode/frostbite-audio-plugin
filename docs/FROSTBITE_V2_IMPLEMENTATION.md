# FROSTBITE V2 implementation

21 October 2026. Windows x64 Release, C++20, JUCE 9.0.4. This is an implemented and built sonic revision; subjective Studio One acceptance remains with the user.

## What changed

V2 is the default for new instances. The previous corrected engine remains selectable as Legacy. Baseline source and executable were preserved before editing; the original parameter definitions, eight factory presets and VST3 identity remain intact. State without an explicit voicing loads Legacy. See [compatibility](FROSTBITE_V2_COMPATIBILITY.md).

| Stage | V2 behavior and gain ownership |
|---|---|
| Input / neutral | Wet-only Input trim; Output defaults to 0 dB. Neutral direct coefficient is 1 instead of .82. Neutral wet avoids the former whole-wet DC filter. |
| Damage | Existing soft Warm curve plus a bounded asymmetric clip and one localized fold for Broken. Double-precision analytic first-order ADAA, modest high-frequency pre-emphasis and post-tone/DC filtering. Drive reaches 51.5 dB with maximum Extra Drive; compensation is `.62 / drive^0.18`, separate from macro blend. Warm retains at least 35% of its soft curve at maximum Damage. |
| Freeze | Four shared-stereo Hann grains read a dedicated 1.5 s rolling input ring. Grain origins refresh from recent history with bounded speed and lag; stale input clears on silence. Capture overrides Live Texture with Hold; Release resumes the selected behavior. Short V2 captures use a length-aware seam taper; constant-cost modulo replaces repeated wrap subtraction. |
| Routing | Grains blend into the direct source before Damage and Echo, or return as a clean parallel layer. Capture always reads original trimmed input, so routing introduces no capture/echo/reverb feedback loop. Direct duck follows actual texture availability; an empty Hold does not attenuate the source. |
| Echo | Existing contractive ping-pong engine retained, with stronger return progression and less coupled darkening. Fine feedback, sync, division, width, tone and return remain available. |
| Abyss | Eight-line FDN retained. Normalized Hadamard feedback spreads energy across all lines; longer spacing, diffuse early return, stronger injection/readout and a damping shelf preserve more upper-band sustain. DC normalization bounds per-line filter gain. Size smoothing is sample-rate aware. Decay, damping and return no longer counteract one another through the old macro mapping. |
| Distance / Motion | Distance remains an intentional tonal/phase recession with a less severe high-frequency endpoint. Motion has a more accessible depth curve. Fine controls remain active. |
| Mix / protection | Linear global Mix retained. Direct/tail balance uses smaller direct attenuation; Abyss 0 and fine Reverb Level 0 mute its return. Existing safety knee and exact-dry/bypass exceptions remain. No automatic level follower or loudness pumping was added. |
| UI | Visible Legacy/V2, Live Texture/Hold and clean/processed-routing selectors. Parameter-driven ice halos and warmer Damage arc intensity; no flashing or particle engine. Existing popup/native repaint repairs retained. |

Ten V2 factory presets were added at programs 8?17. They operate without Capture. Original factory files/programs remain unchanged. No installed plugin or user preset was overwritten.

## Measured design iteration

1. Preserved the clean post-audit-fix revision `5659632aa91eaf518fdc541d96ed6b2f28de743f`, its renderer and VST3.
2. Established schema/voicing compatibility, Output 0 dB and neutral unity; built the DSP foundation and ran the original regressions.
3. Implemented Damage, FDN and real-input granular/routing changes; built the Windows targets and expanded the tests and renderer.
4. Rendered candidate 1. It exposed identical maximum Warm/Broken shapes and mild protection on several hot steady tones, especially Whiteout and Endless Winter II. Preserved its measurements in `renders/v2/candidate1`.
5. Capped Warm's morph, added a level-matched shape-difference regression, and applied measured factory trims (Frozen Cello II ?0.5, Glacial Echo II ?0.5, Black Ice II ?1, Endless Winter II ?1.5, Whiteout ?3 dB). These are fixed preset trims, not input-dependent normalization.
6. A transient comparison exceeded full scale only after offline RMS matching; the raw effect render was safe. Transient comparisons now share ?30 dBFS RMS, preserving crest factor; other comparisons use ?24 dBFS RMS. The tool refuses clipping instead of limiting comparisons.
7. Rebuilt, reran the full suite and independent host check, then regenerated final evidence.

Default active-window RMS insertion on the shared ?18 dBFS peak synthetic inputs:

| Source | Legacy | V2 |
|---|---:|---:|
| Piano | ?4.82 dB | ?0.76 dB |
| Cello | ?3.82 dB | ?0.72 dB |
| 1 kHz sine | ?5.13 dB | ?0.60 dB |
| White noise | ?6.04 dB | ?1.20 dB |

Neutral fully wet gain is 0 dB. Dry and bypass retain their exact-sample contracts. Presets intentionally retain different dynamics and levels; the evidence does not equate RMS with perceived loudness.

Maximum-Abyss raw impulse measurements at 48 kHz:

| Metric | Legacy | V2 |
|---|---:|---:|
| Integrated return energy relative to impulse | ?16.60 dB | ?6.63 dB |
| Broadband T20-extrapolated RT60 | 9.34 s | 14.45 s |
| Low / mid / high band decay | 9.87 / 9.30 / 2.42 s | 13.64 / 15.85 / 11.90 s |
| Stereo correlation | ?0.039 | ?0.014 |

These are finite 40-second, 24-bit IR estimates, not listening verdicts. Early/late balance, density proxies, resonant-mode prominence and fit quality are in `reverb.csv`; full-chain tails are in `chain_ir.csv`. More return energy alone does not establish better spatial quality; the changed decay spectrum and matrix are separately measured.

## Distortion quality decision

`Tools/compare_damage_quality.py` evaluates the exact Broken transfer curve against ordinary point shaping at 2?/4? and a 32? FIR reference. At 48 kHz, 7301 Hz and ?18/?6 dBFS input, nonharmonic residual is ?26.17/?25.81 dBc for ADAA, ?18.67/?16.24 for 2? and ?33.60/?23.67 for 4?. ADAA was retained: ordinary 2? is worse on this probe, and 4? is not consistently better at hard input. The test FIR would add approximately 128 base-rate samples of causal up/down-filter delay; no oversampling was introduced into production.

This is a float64 transfer-curve simulation, not a C++ oversampling CPU measurement. It does not prove superiority over every oversampling design. Extreme bright Broken signals retain audible-risk aliasing; the response is not alias-free and its residual can exceed Legacy. In-engine harmonic sweeps, high-frequency probes, two character choices and multitone product measurements accompany original-level and common-RMS audio. No listening test was fabricated.

## Verification

- Windows Release VST3, standalone and renderer built. Both VST3 CIDs unchanged.
- 24 DSP tests pass, including original regressions, neutral unity, live source replacement/silence clearing, held routing, distinct level-matched maximum shapes, 90-second reverb stability and callback allocation checks.
- V2 stress covers 44.1/48/96 kHz ? 1/64/128/256/512/1024 samples, about two seconds or longer per combination, timed automation/recapture, bypass, maximum drive/decay/feedback and extreme trims.
- Integration tests cover original parameter identities/order/version hints, all 18 factory files, schema 1/2/missing/future states, user preset roundtrips, exact mono/stereo dry/bypass and rate transitions. Two passes through all V2 presets with hot sustained input and retained tails peak at .6583/.6753/.6613 at 44.1/48/96 kHz, with no full-scale or invalid samples.
- Native UI tests cover 29 rotary controls, three buttons, five choices, off-thread automation, rendered values, actual popup rows/ticks/keyboard operation, editor lifecycle and scaling. Generated screenshots were inspected.
- Independent VST3 host scans/instantiates the compiled bundle, verifies 38 wrapper parameters (37 APVTS plus wrapper parameter), state restore, 18 rate/block combinations, off-thread automation of 18 controls and editor reopen.
- Installer fixtures verify guarded replacement, copy/hash checks and backup placement without altering the real installation.

## Final evidence and performance

The final executable reproduces **244/244 Legacy WAVs exactly** (maximum PCM difference 0). The final audit contains 718 output rows / 10,052 stage rows; the separate V2 preset matrix adds all 10 presets ? 5 sources ? 2 input levels (100 rows). Across Legacy, final audit and five-source matrix, all 1,326 output rows have zero nonfinite and zero full-scale samples. Maximum output is .960417 in the wider stress/reference matrix.

The five-source V2 preset matrix has maximum pre-safety peak **.865655** (?1.25 dBFS) and **0 dB safety-stage RMS change**. All ten V2 presets also remain below the .92 safety knee across the audit's hot sine/noise/instrument probes after calibration; worst is Cataclysm at ?0.826 dBFS. These measurements apply to the specified inputs, not arbitrary hot tracks.

Final audio: 244 preserved baseline WAVs, 244 parity WAVs, 294 audit WAVs and 151 audition/probe WAVs. The audition set includes 50 original-level preset/source renders, 50 matched companions, five inputs, six held-sequence comparisons and 40 multitone character/amount/level probes. Presets and routing are demonstrably different from Legacy without requiring Capture. `analysis/intermodulation.csv`, `analysis/levels.csv`, `analysis/frequency_response.csv`, `analysis/preset_headroom.csv` and `analysis/hold_stages.csv` retain the detailed measurements.

At maximum Damage on the ?18 dBFS, 1 kHz probe (48 kHz, each voicing's default fine controls), third/fifth/seventh harmonics relative to the fundamental change from **?15.73/?26.28/?34.98 dBc** to **?10.86/?17.23/?22.35 dBc**. This demonstrates changed harmonic ratios independent of overall gain. The separate Warm/Broken regression compares normalized waveforms; it rejects a difference that is merely gain. These are objective distinctions, not a subjective quality score.

CPU was measured on a 13th Gen Intel Core i9-13900H (14 cores / 20 logical processors), with no concurrent build/render job. The benchmark times parameter sanitization plus Engine processing, excluding GUI, audio-device and host-wrapper overhead. Values below are arithmetic means over block sizes 1/64/128/256/512/1024; the full 108-row CSV includes silence, default and extreme automation, plus median/p99/maximum callback times.

| Sample rate | Legacy default CPU / real time | V2 default | V2 extreme automation |
|---|---:|---:|---:|
| 44,100 Hz | 1.97% | 2.37% | 2.81% |
| 48,000 Hz | 2.10% | 2.50% | 3.02% |
| 96,000 Hz | 4.80% | 5.14% | 6.05% |

At 48 kHz / 256 samples, measured default processing is 1.96% Legacy versus 2.47% V2, an increase of 0.51 percentage points. V2 default median/p99/maximum block times are 123/297.5/304.8 ?s against a 5,333 ?s block period. This is one wall-clock run on a shared desktop, with scheduling noise; it is not a hard real-time guarantee or a Studio One dropout test. All measured maximum block times at sizes 64?1024 remained within their block periods. One-sample timing contains outliers in both voicings: the largest V2 observation is 234.1 ?s at 96 kHz against a 10.42 ?s period (median 0.6 ?s, p99 1 ?s). Wall-clock timing cannot separate DSP execution from OS interruptions; one-sample deadline compliance is therefore not verified. No production oversampling was added, so there is no new oversampling latency/CPU stage to report.

## Reproduce and inspect

```powershell
.\scripts\build.ps1 -Configuration Release -Jobs 1 -SkipRenders
$render = "$PWD\build\frostbite_render_artefacts\Release\frostbite_render.exe"
& $render --audit-legacy "$PWD\renders\v2\legacy_check"
& $render --audit "$PWD\renders\v2\after"
& $render --v2 "$PWD\renders\v2\audition"
python Tools/analyse_audit.py renders/v2/after --before renders/v2/legacy
python Tools/analyse_v2.py renders/v2
python Tools/compare_damage_quality.py
& $render --benchmark "$PWD\renders\v2\analysis"
.\scripts\package.ps1
```

Use a fresh output root to preserve an existing comparison run. `--audit-legacy` regenerates all original cases with the current executable; the preserved baseline renderer produces `legacy`. Python analysis needs NumPy, SciPy and Matplotlib, only offline. Production callbacks add no allocation, lock, filesystem or network work. Buffers are preallocated in prepare, loops are bounded, and the JUCE audio callback retains denormal protection.

The package contains the VST3 bundle, updated standalone/renderer, complete project source/tests/tools, 18 presets, fresh validation evidence, these guides and a curated audio set. The full five-source set and trace CSVs remain in the repository. See [installation and audition](FROSTBITE_V2_AUDITION.md).

## What remains unverified

Studio One session recall and a real-instrument subjective audition of this V2 build remain user acceptance tasks. Automated native-editor tests do not prove Studio One repaint behavior on every display/driver. Wall-clock DSP timings do not prove DAW dropout performance. True-peak/LUFS, hardware loopback, macOS and perceptual ABX were not tested. Echo retains the original M/S ping-pong topology, including its hard-panned mono-fold characteristics. Captured audio is transient and is not saved. None of these boundaries is presented as a completed listening test.
