# FROSTBITE sonic development roadmap

This roadmap separates demonstrated engineering corrections from artistic proposals. The current audit changes two narrowly scoped DSP behaviors, adds offline measurement instrumentation and improves UI feedback. It does **not** retune factory presets, replace the reverb, increase master gain, or introduce a new distortion algorithm. No subjective listening result is claimed here. Numerical evidence, signal flow and current limitations are recorded in `DSP_AUDIT.md`; reproducible measurements and audition material are under `renders/audit`.

Priorities below describe the recommended development order. Difficulty is relative to this small codebase. CPU estimates are directional until profiled in the Windows VST3 at 44.1, 48 and 96 kHz. Musical proposals need explicit agreement before implementation.

## 1. Fundamental corrections and calibration

### Completed: honor the reverb return level

- **Objective:** Make the fine Reverb Level control govern every returned reverb component, including the ambience added by Distance.
- **Technique:** Multiply Distance's separate reverb send-to-output coefficient by the same fine level. Keep Distance's independence from the Abyss macro.
- **Expected effect:** Reverb Level zero now mutes the reverb return; intermediate settings attenuate the Distance contribution consistently. This is routing correctness, not a claim of a stronger reverb.
- **Difficulty / CPU:** Low; one extra scalar coefficient multiplication.
- **Risk:** Existing sessions using Distance with Reverb Level below one will return less ambience than the defective behavior. Defaults and all current factory presets use fine level one and retain their voicing.
- **Validation:** Failing-before/passing-after impulse regression inspects the actual return and wet summing stages. Compare the consistent before/after WAVs. Check smooth automation at nonzero Distance.
- **Preset compatibility:** Existing parameter IDs, ranges, serialized values and plugin identity remain intact. Affected sessions deliberately receive the corrected routing.

### Completed: preserve Output smoothing when the dry path reaches unity

- **Objective:** Prevent a dry-path level jump when Output is automated from a nonzero trim to 0 dB.
- **Technique:** Enter the exact-dry shortcut only when the smoothed Output value is also sufficiently close to unity.
- **Expected effect:** The existing 25 ms smoothing continues through the return to 0 dB; the settled dry signal remains exact.
- **Difficulty / CPU:** Low; one comparison.
- **Risk:** The transient behavior changes for this specific automation case, as intended. No settled gain or preset voicing changes.
- **Validation:** Failed-before/passed-after continuity regression at 44.1, 48 and 96 kHz; compare the fixed-input output-automation renders. Verify exact settled dry samples and bypass.
- **Preset compatibility:** No state or parameter changes.

### Priority 1: calibrate neutral wet gain and default insertion level

- **Objective:** Establish an understandable baseline without disguising attenuation as extra master volume.
- **Technique:** Treat the hard-coded direct-layer gain `0.82`, direct-to-tail balance, linear global mix and default Output trim as separate calibration decisions. First measure spectrum, RMS, peaks and dry/wet correlation for each stage. Then propose a neutral wet reference and an explicit headroom budget. Keep the global linear mix while its correlated direct component remains substantial; investigate a different mix law only if a later topology removes that correlation.
- **Expected effect:** More predictable insertion level and a clearer distinction between intentional darkening, direct-signal recession and avoidable gain loss. No universal loudness normalization is promised.
- **Difficulty / CPU:** Medium for calibration and listening; negligible DSP cost if only coefficients change. Dynamic loudness compensation would be a separate, higher-complexity proposal.
- **Risk:** Restoring energy may engage the existing 0.92-amplitude safety knee sooner. Equal-power mixing would boost correlated signals. Matching RMS alone can hide spectral or transient changes.
- **Listening validation:** Blind, randomized unity-dry/default/neutral-wet comparisons on real cello, piano, clean guitar and synth at several levels. Present both original-level and level-matched versions; measure crest factor and overload duration. Check mono and stereo.
- **Preset compatibility:** Any coefficient, default trim or preset adjustment changes sound. Retain a legacy voicing path or introduce explicitly versioned presets before changing defaults in released projects.

### Priority 2: resolve small timing and control-rate discrepancies

- **Objective:** Make time controls and automation behavior consistent across sample rates.
- **Technique:** Specify delay read/write semantics explicitly. Positive reverb predelay currently reads after pushing, giving a delay one sample shorter than the requested sample count; zero predelay intentionally gives immediate delivery to the diffusers. Reverb size uses a fixed `0.004` coefficient every 32 samples, so its additional time constant is about 181 ms at 44.1 kHz, 166 ms at 48 kHz and 83 ms at 96 kHz. Replace that coefficient with a sample-rate-derived time constant only after choosing the intended transition time. Inspect similar fixed update-rate coefficient behavior and fractional-delay attenuation.
- **Expected effect:** Reproducible predelay timing and room-size transitions; these changes should not be presented as a remedy for weak character.
- **Difficulty / CPU:** Low to medium; negligible steady-state CPU change.
- **Risk:** Altering size transition speed changes pitch movement during automation. An exact predelay repair shifts the wet impulse by one sample. The latter is small but still requires coverage.
- **Listening validation:** Automated size/predelay sweeps at all supported test rates, transient continuity checks, impulse timing tests and deliberate comparisons of slow room-size automation.
- **Preset compatibility:** Static parameter/state compatibility is preserved; timing and automated transitions change. These issues are documented and deferred in this audit.

### Priority 2: bound the cost of extremely short captures

- **Objective:** Keep Freeze work per sample bounded even for the shortest legal captured windows.
- **Technique:** `FreezeEngine::read` repeatedly subtracts the capture length from a grain position that grows until respawn. Nine-sample captures with long grains can therefore perform thousands of loop iterations per read. Measure this corner case, then wrap the maintained grain position once per step, or use another bounded wrap operation, with numerical parity checks.
- **Expected effect:** Predictable callback cost for rapid capture/release automation. No intended sonic change.
- **Difficulty / CPU:** Low to medium; likely large CPU reduction in this corner case, small or neutral cost for normal captures.
- **Risk:** Incorrect wrapping can move the capture seam, break stereo alignment or change interpolation. Wall-clock thresholds alone are fragile tests; combine operation bounds, parity checks and timing measurements.
- **Listening validation:** Nine-sample, short-transient and full-window captures at 40/400 ms grain sizes; rapid automation and maximum supported rates. Confirm identical stereo timing and seam behavior.
- **Preset compatibility:** No state changes expected. This is a code-derived performance concern, not a measured audio-dropout claim; it remains deferred.

## 2. Distortion-character development

### Priority 1: redesign the compensation curve before adding drive

- **Objective:** Preserve audible nonlinear transformation as Damage increases instead of progressively hiding it with post-shaper attenuation.
- **Technique:** Measure both saturators across input levels, then compare a bounded, calibrated compensation curve with the current `1/sqrt(drive)` rule. Current drive can reach 54 dB, while compensation can remove 27 dB after an already bounded shaper. Keep drive, output calibration and harmonic character conceptually separate. Evaluate a fixed compensation table first; automatic input-level conditioning should be a separate option with documented dynamics.
- **Expected effect:** More consistent distortion prominence and more useful progression at high Damage. A fair comparison must establish whether the preference remains after level matching.
- **Difficulty / CPU:** Medium tuning effort; low CPU for a static curve/table. Envelope-based conditioning costs more and changes dynamics.
- **Risk:** Excessive compensation increases the safety limiter's contribution, raises noise and overfeeds Echo/Reverb. It can erase the intended recession of distant presets.
- **Listening validation:** Low/medium/high Damage and Extra Drive at -18 and -6 dBFS, plus real instruments. Compare level-matched harmonics, transients, intermodulation and noise; repeat with ambience disabled and enabled.
- **Preset compatibility:** Strong tonal and gain changes to damaged presets. Retain legacy compensation for existing states or version the voicing explicitly.

### Priority 2: give Broken a deliberately distinct transfer characteristic

- **Objective:** Develop a controlled industrial extreme beyond the current two smooth bounded saturation curves.
- **Technique:** Prototype one candidate at a time: a sharper asymmetric waveshaper with a tractable antiderivative, restrained pre-emphasis into saturation followed by de-emphasis, or a dedicated hard-clipping/foldback experiment behind a development switch. The present Broken curve `x/(1+abs(x))` is still soft saturation, with bias and gain. Do not stack foldback, bit reduction, sample-rate reduction and feedback distortion in one unreviewable change.
- **Expected effect:** Stronger harmonic/intermodulation contrast, different transient deformation and a meaningful Warm-to-Broken progression.
- **Difficulty / CPU:** Medium for a new analytic shaper; high for multiband or feedback designs. A single shaper/pre-emphasis pair can remain inexpensive; multiband processing increases both CPU and phase-management work.
- **Risk:** Harsh aliasing, DC, excessive low-end loss, unpleasant note beating or a useful soft mode becoming unavailable because the macro forces Broken above 55%.
- **Listening validation:** Same-level and level-matched sustained cello, low guitar chords, piano attacks and complex synths. Compare isolated Damage before ambience; document whether character differences survive the default mix and Distance filtering.
- **Preset compatibility:** Existing Character is a two-state parameter and must keep its ID/meaning for old states. New algorithms require explicit versioning or a future additional selector; silently replacing its curves changes existing presets.

### Priority 2: evaluate oversampling against ADAA on the chosen shaper

- **Objective:** Reduce unwanted foldover without removing the intended harshness by simply filtering everything dark.
- **Technique:** Compare current first-order ADAA with a bounded 2x/4x oversampling prototype and, where appropriate, a better-conditioned analytic implementation. Measure high-frequency sine and multitone residuals at 44.1/48/96 kHz using a high-rate reference. Oversample only the nonlinear subsystem initially, not the whole long-delay network.
- **Expected effect:** Cleaner upper spectrum and a wider useful range of tone settings, particularly under strong nonlinear drive.
- **Difficulty / CPU:** Medium to high; a multiple of the shaper/filter work plus resampling filters. Measure the actual callback budget on Windows.
- **Risk:** Oversampling introduces latency and changes phase. Dry and parallel wet contributions must be aligned consistently; report any fixed plugin latency to the host. Enabling it can alter the sound even below audible aliasing levels.
- **Listening validation:** Blind level-matched comparisons of high notes, bright transients and chords; inspect spectra and null residuals while keeping drive identical. Check host bypass, latency compensation and automation.
- **Preset compatibility:** Treat quality/algorithm changes as versioned behavior. Preserve old rendering when loading existing states unless the user deliberately opts in.

## 3. Reverb-character development

### Priority 1: separate reverb level, decay and spectral recession

- **Objective:** Make high Abyss clearly spacious while preserving intentional darkness and direct-signal distance.
- **Technique:** Calibrate the raw FDN return and its feed before changing the algorithm. Audit the fixed input `0.24`, output `0.44`, direct attenuation and global Mix jointly. Reconsider the combined Abyss-driven wet/size/decay/damping curve after measured low/mid/high-band decay and real-source listening. Keep low-frequency buildup bounded, but separate its management from broad masking of the late tail.
- **Expected effect:** A more audible, spectrally balanced tail with a sensible direct-to-reverberant ratio. Avoid claiming that more return gain alone creates a larger room.
- **Difficulty / CPU:** Medium tuning effort; little extra CPU if the eight-line structure remains.
- **Risk:** Less damping increases metallic modes and feedback energy. A nominal 35/45-second setting does not guarantee that audible frequency bands sustain that long. Return-gain boosts can engage downstream protection.
- **Listening validation:** Level-matched cello and piano phrases with pauses, raw wet-only impulse tails, frequency-dependent decay estimates and dry/wet ratio sweeps. Include both short and long notes and listen at a consistent monitor level.
- **Preset compatibility:** Existing ambience will change. Version macro curves and presets; preserve the current FDN path for old states if new voicing ships.

### Priority 2: improve density development before replacing the topology

- **Objective:** Build a smoother, larger impression with less audible mode structure and clearer transition from onset to tail.
- **Technique:** Measure density and prominent decay modes in the current four-allpass-per-channel diffuser plus eight-line Householder FDN. Test revised delay distributions, diffuser gains and injection/readout vectors individually. Consider a modest early-reflection network only if listening and onset measurements identify that absence as a limitation. Compare alternatives against the current orthogonal matrix rather than assuming a new matrix is inherently better.
- **Expected effect:** Better early/late integration and less persistent ringing, if the measurements and listening support those hypotheses.
- **Difficulty / CPU:** Medium for coefficient/topology tuning; higher for additional delay stages. Small reflection networks add memory/read work; a larger FDN increases continuous callback cost.
- **Risk:** Excess diffusion softens piano/guitar attacks; denser structures can sound smaller or flatter. Extra taps can create comb filtering. Preserve feedback contraction after every change.
- **Listening validation:** Sparse impulses and handclap-like attacks, repeated piano notes, plucked guitar and sustained cello. Compare mono and stereo, bypassed modulation and maximum decay; inspect the decay envelope and narrow spectral peaks.
- **Preset compatibility:** Delay or diffuser changes alter every reverb preset, including recalled tails. Version the algorithm if adopted.

### Priority 3: make modulation more expressive without unstable pitch

- **Objective:** Add evolving atmosphere at high Abyss/Motion while keeping low settings stable and useful.
- **Technique:** Prototype calibrated modulation depth/rate curves and slow, bounded modulation variants. First characterize the current eight phase-offset sinusoidal delay taps, linear interpolation loss and interaction with size automation. Keep modulation separate from any automatic loudness compensation.
- **Expected effect:** More variation in the late tail and less static ringing; the desired amount of pitch movement needs listening approval.
- **Difficulty / CPU:** Medium; low to modest CPU depending on interpolation and modulation source.
- **Risk:** Excess pitch warble, low-frequency beating, loss of stereo focus and modulation-induced decay changes. Moving delay taps affect effective feedback energy.
- **Listening validation:** Held cello intervals, sustained synths and piano chords at slow/fast Motion; compare onset and late tail separately. Test headphones, speakers and mono fold-down.
- **Preset compatibility:** Changing modulation curves alters existing presets; keep legacy mappings for old states or publish new presets only.

## 4. Freeze and atmospheric development

### Priority 2: make capture level and texture more predictable

- **Objective:** Retain the harmonic identity of captured material while producing an intentional, audible frozen layer.
- **Technique:** Measure four-grain overlap cancellation across coherent tones, noise and transients. Compare restrained normalization tied to grain-window energy with the existing sum-of-window normalization. Investigate onset selection, grain-offset distributions and stereo-preserving phase relationships independently. The present common left/right grain positions preserve stereo timing; retain that property unless a width mode is explicitly approved.
- **Expected effect:** More consistent captured-layer prominence and a clearer progression from stable harmonic sustain to diffuse texture.
- **Difficulty / CPU:** Medium to high tuning effort; low for normalization, potentially higher for onset/period analysis. Any analysis must have a bounded callback budget or occur outside the callback with safe handoff.
- **Risk:** Noise gain, pumping, overly static looping, unintended transient repeats or collapse of stereo ambience. Normalization cannot guarantee perceived loudness across arbitrary captured content.
- **Listening validation:** Real cello vibrato, piano sustain/attack mixtures, guitar harmonics and pads; multiple capture moments per phrase. Level-match the freeze layer independently and test release/recapture transitions.
- **Preset compatibility:** Grain selection and gain changes alter deterministic freeze rendering even with the same controls. Version the algorithm; captured audio remains excluded from host state unless separately authorized.

### Priority 3: consider explicit routing for destroyed frozen textures

- **Objective:** Allow the distorted extreme to act on the held layer when desired.
- **Technique:** The current Freeze branch taps Input before Damage and joins after Echo, so neither Damage nor Echo processes the held layer. Prototype a clearly defined routing option, such as pre-Damage freeze injection or a separate post-freeze coloration stage, before deciding on any production feature.
- **Expected effect:** A held capture could become industrial or echoing independently of continued live input, addressing an architectural limit of the current destroyed texture.
- **Difficulty / CPU:** Medium; reuse may add little CPU, while a separate shaper/delay adds processing and state.
- **Risk:** Recirculating capture/reverb interactions, drastic level changes and loss of the existing clean-freeze contrast. Avoid introducing an uncontrolled feedback loop.
- **Listening validation:** Capture once, stop the source, then sweep Damage/Echo. Compare against the current parallel topology at matched levels and test maximum feedback/release.
- **Preset compatibility:** This is a new feature, not an authorized audit correction. Keep current routing as the legacy/default state for existing presets; introduce a separate option only after approval.

## 5. Macro and preset tuning

### Priority 1 after calibration: tune macro progressions with listening anchors

- **Objective:** Make 0/25/50/75/100% represent useful, distinguishable musical steps without sacrificing endpoints.
- **Technique:** Use the measured mapping tables to identify coupled controls, then establish perceptual anchors. Review Damage's drive/blend/character transition, Abyss's simultaneous return/damping/size changes, Distance's lowpass/highpass/direct attenuation and Motion's coupling into Freeze drift and reverb modulation. Keep fine controls meaningful. Freeze remains inaudible until a capture exists; that is state dependence, not a macro mapping failure.
- **Expected effect:** A clearer progression between beautiful and destroyed textures, with fewer controls simultaneously counteracting each other.
- **Difficulty / CPU:** Medium to high listening/tuning effort; negligible CPU for revised scalar curves.
- **Risk:** A smoother mathematical curve is not necessarily a better musical control. Distance is intentionally lossy; equalizing its loudness could undo distance perception. Avoid changing ranges solely to make a graph look linear.
- **Listening validation:** Randomized quarter-step sweeps on the same real phrases at several input levels, captured and uncaptured. Rate intensity, darkness, space and usefulness separately from loudness. Repeat with global Mix at 50% and 100%.
- **Preset compatibility:** Revised macro curves affect every recalled normalized value. Prefer a new voicing version with explicit migration rather than silently interpreting old controls differently.

### Priority 2: curate factory presets for distinct roles and transparent level differences

- **Objective:** Make each preset demonstrate a specific useful extreme while retaining headroom and predictable insertion behavior.
- **Technique:** Group presets using measured spectrum, correlation, direct/tail ratio, decay and input/output gain, then audition suspected overlaps. Tune only after the underlying gain and character decisions. Include explicit captured/uncaptured demos because presets do not contain frozen audio. Define whether output trim preserves intentional distance or matches a chosen reference; do not force every preset to an identical peak.
- **Expected effect:** A smaller set of clearly differentiated roles, or revised versions of the current eight presets, with fewer accidental level surprises.
- **Difficulty / CPU:** Medium curation effort; no runtime CPU cost unless new DSP options are adopted.
- **Risk:** Objective similarity does not prove redundant sound; different capture timing and program material can reverse the comparison. Loudness matching can disguise an intentionally distant design.
- **Listening validation:** Real instrument loops, randomized preset order, matched and original levels, mono checks and short/long phrases. Keep a written decision for each preset rather than claiming improvement from a louder render.
- **Preset compatibility:** Preserve the current factory files and user preset directory. Publish revised presets with versioned names or preserve legacy mappings so old projects retain their sound.

### Priority 2: resolve Echo's channel asymmetry before wider stereo claims

- **Objective:** Offer an intentional stereo/mono result for left-only, right-only and wide sources.
- **Technique:** The current encoder sends mid to the left delay and `right-minus-left` side to the right, then cross-feeds them. This gives intentional mono ping-pong, but a left-only first repeat is anti-phase while a right-only first repeat is in-phase. Measure fold-down and compare a conventional stereo cross-feedback topology, a decoded mid/side design or a separately selected mono ping-pong mode.
- **Expected effect:** More predictable mono compatibility and channel symmetry if a revised mode is chosen.
- **Difficulty / CPU:** Medium design/testing effort; low CPU for matrix/routing changes.
- **Risk:** Correcting asymmetry can remove the current mono bounce or alter stereo width. The current behavior is explicitly documented, so choosing a different topology is a sonic design decision rather than a silent audit fix.
- **Listening validation:** Channel-specific impulses, hard-panned guitar/piano, anti-phase stereo tests and mono fold-down. Check each repeat, not only aggregate stereo correlation.
- **Preset compatibility:** A routing change affects Echo presets. Retain legacy behavior or add an explicit mode in a later approved version.

## Release gates for a future voicing pass

1. Agree the sonic objective and permitted compatibility change before implementing a candidate.
2. Preserve reproducible input, original-level renders and level-matched renders. Matching peak is not loudness matching; matching RMS is only a first approximation.
3. Verify signal flow, per-stage headroom, nonfinite handling, long-tail stability, dry unity, interpolation/mix behavior, no callback allocations and bounded callback cost.
4. Run Release DSP/integration/UI tests and the independent VST3 host check at the supported sample rates and representative block sizes. Measure fixed latency if oversampling or a new alignment stage is introduced.
5. Audition the candidate in Studio One with real cello, piano, guitar and synth, including mono/stereo and automation. Only that listening pass can validate claims such as heavier, more spacious, more beautiful or more distinctive.
