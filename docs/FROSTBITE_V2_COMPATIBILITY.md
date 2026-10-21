# FROSTBITE V2 compatibility

21 October 2026. Windows x64, C++20, JUCE 9.0.4.

## Existing sessions and presets

The VST3 identity is unchanged: manufacturer `Frba`, plugin `Frbt`, bundle `audio.frostbite.effect`, product `FROSTBITE`. Component CID `ABCDEF019182FAEB4672626146726274`; controller CID `ABCDEF011234ABCD4672626146726274`. Version is now 2.0.0.

The original 34 parameter IDs, ordering, version hints (1), ranges, steps, skew and choice labels remain unchanged. Original factory programs 0?7 and all eight original XML files remain unchanged. V2 factory programs are appended at 8?17, in the requested order. The menu labels their collections Legacy and V2; persisted original names remain intact.

New instances and INIT use V2, Output 0 dB, Live Texture and Into Damage + Echo. Loading an older state does **not** adopt these new defaults.

## Schema 2

The root remains `FrostbiteState`, with `PARAM` children storing stable `id` and physical `value`. Writers now emit `schema="2"`, preset name and program metadata. Readers accept schema 1, schema 2 and missing schema (treated as 1). Unknown parameter children are ignored. Invalid roots and future schemas are rejected before changing state.

| Appended ID | Values | New instance | State without this ID |
|---|---|---:|---:|
| `voicing` | 0 Legacy, 1 V2 | 1 | 0 |
| `freeze_mode` | 0 Hold, 1 Live Texture | 1 | 0 |
| `freeze_routing` | 0 Clean, 1 Damage + Echo | 1 | 0 |

**A missing explicit voicing always means Legacy**, even if the XML root says schema 2. Restore starts from historical defaults, including Output ?3 dB. Present IDs then overwrite those defaults. An explicit voicing of 1 deliberately selects V2; existing macro values are retained, so switching an old preset is an intentional reinterpretation rather than an automatic migration.

All three added parameters are host-automatable choices with version hint 1. Routing blends over the existing 25 ms smoothing. Live/Hold and Capture use smoothing and the existing capture envelope. Voicing switches apply a brief continuity correction; internal histories are retained, so switching during a tail can change that tail. New instances still report zero fixed latency. ADAA averages the nonlinear branch over adjacent samples; this is a frequency-dependent coloration, not a new compensatable whole-plugin delay.

Legacy ignores the new Freeze mode/routing choices. Its manual Capture, grain sequence, Damage, Echo, FDN, Distance, linear Mix, Output and bypass behavior are preserved. The prior Reverb Level mute and Output smoothing repairs remain in both paths. Freeze address wrapping now uses constant-cost modulo; ordinary legacy renders are checked against the preserved post-fix executable.

## Captured audio and bypass

State and preset files contain settings only. Captured audio is never serialized. Capture is written as zero and reopens unarmed, without changing the running Capture button during save. In V2 Live Texture, pressing Capture holds the preceding source; Release resumes refreshing. In Hold mode, Release fades the held grains. An empty Hold does not duck the direct signal.

Mix 0 / Output 0 and settled bypass retain the original sample-exact dry contract. Input trim affects the wet branch; Output affects the mixed signal. Global Mix remains linear. The existing output safety knee remains; exact dry/bypass intentionally bypass that protection for valid input samples.

No existing user preset files or installed plugin were replaced during development. User preset directory remains `%APPDATA%/Frostbite/Presets` (JUCE user application data location). New V2 XML files live in `Presets/Factory/V2`.

## Baseline and rollback

Baseline revision: `5659632aa91eaf518fdc541d96ed6b2f28de743f`.

- `build/v2-baseline/source-5659632.zip`: clean source archive.
- `build/v2-baseline/frostbite_render_legacy.exe`: preserved post-audit-fix renderer.
- `build/v2-baseline/FROSTBITE.vst3`: preserved plugin bundle.
- `renders/v2/legacy`: untouched baseline renders.
- `renders/v2/legacy_check`: current binary forced to Legacy on the same input/timing.

The older `build/audit/frostbite_render_before.exe` predates the two audit fixes and is not the sonic compatibility reference for V2. V1 cannot read schema 2 files; retain originals when intentionally switching a session to V2. The installer preserves marked prior installations outside DAW scan folders.
