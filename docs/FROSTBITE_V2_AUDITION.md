# FROSTBITE V2 audition

The audio is deterministic synthetic instrument-like material, not recorded piano, cello or guitar. No subjective listening verdict is claimed. Use Studio One with your own recordings for the final judgment.

## Install and open

Close Studio One and other FROSTBITE hosts. Extract the Windows release package. From its directory run:

```powershell
.\scripts\install.ps1 -Scope User -Replace
```

For a first installation, omit `-Replace`. The installer verifies hashes and preserves a marked prior installation outside the VST3 scan path. It refuses an unmarked existing bundle. The user install location is `%LOCALAPPDATA%\Programs\Common\VST3\FROSTBITE.vst3`; add that location to Studio One if needed, then rescan/restart. The complete `.vst3` directory is the plugin bundle; do not move only its inner binary. System installation uses `-Scope System` from an elevated PowerShell.

The package also includes `FROSTBITE.exe` and `frostbite_render.exe`. The current installed working plugin was not changed by this implementation.

## Start here

New instances show V2 and LIVE TEXTURE. Start with Output 0 dB and moderate monitoring level. Raise Freeze while playing; Capture is optional. The original presets are labelled Legacy. The ten V2 presets are listed after them.

| Preset | Audition focus |
|---|---|
| Frozen Cello II | Smooth live sustain; Capture, then release back to evolving texture |
| Glacial Echo II | Wide dotted repeats and a clear source |
| Distant Cathedral II | Predelay and a large diffuse tail after each note |
| Industrial Fog II | Asymmetric grit with an audible atmospheric return |
| Broken Transmission II | Narrow triplet repeats, short grains and faster motion |
| Black Ice II | Short grains and brighter, smaller metallic space |
| Endless Winter II | Long refreshed grains and slow, extensive decay |
| Ruined Piano II | Jagged transients and rhythmic trails |
| Whiteout | Luminous dense cloud; Capture for sustained frozen material |
| Cataclysm | Heavy Broken saturation and controlled modulation |

Factory settings all work without Capture. Quiet sections and intentional Distance remain dynamic; the presets are not forced to equal RMS.

## Original level and matched comparisons

Full evidence is in `renders/v2`. `legacy` holds the before renders, `after` the full V2 audit, and `audition` contains all ten V2 presets on piano, cello, guitar, synth and short transients. The package contains a smaller `Audio` selection.

Files ending `_rms_minus24.wav` are scaled to common ?24 dBFS RMS: preset examples use their four-second active input window, isolated Damage uses two seconds, and held-sequence comparisons use the full ten seconds. Short-transient comparisons instead share ?30 dBFS RMS (`_rms_minus30`) to preserve their crest factor without clipping. They are **not** LUFS/perceived-loudness matched. Tail gain uses the same scaling as its active source. Start with original-level pairs to assess insertion gain, then compare the matched pairs at unchanged monitor gain to judge timbre and space without a simple level advantage.

Useful pairs: Frozen Cello / Frozen Cello II cello; Industrial Fog / Industrial Fog II cello; Ruined Piano / Ruined Piano II piano; Endless Winter / Endless Winter II cello. Whiteout and Cataclysm have no V1 equivalents. Input files are supplied beside the examples. Do not peak-normalize the files independently.

## The essential held-texture experiment

`hold_legacy.wav`, `hold_clean_V2.wav` and `hold_destroyed_V2.wav` share this timing (events align to a 256-sample block):

1. 0?2 s: real input-derived cello harmonics.
2. 1.5 s: Capture on; input stops at 2 s.
3. 3 s: Damage rises to 100%.
4. 4 s: Echo rises to 90%.
5. 5 s: Abyss rises to 100%.

The processed V2 route makes Damage and Echo act on the held material. Clean routing retains an independent granular layer. No capture/reverb feedback loop is introduced. Reproduce this with a cello chord, vocal phrase or guitar in Studio One, then automate Damage, Echo and Abyss after stopping playback.

## Remaining Studio One acceptance

Verify old projects recall Legacy and their original sound; new instances use V2; automate the added selectors and all macros; test mono/stereo and host bypass; close/reopen the editor; save/reopen a session and a user preset. Check tail transitions when changing voicing or presets. Compare real recordings at their normal track levels, including mono fold-down and high-frequency aliasing at extreme Broken settings. The independent host and native UI tests do not substitute for this real Studio One audition.
