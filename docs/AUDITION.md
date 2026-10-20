# Diagnostic listening session

Run `.\scripts\build.ps1`, or:

```powershell
.\build\frostbite_render_artefacts\Release\frostbite_render.exe --diagnostics "$PWD\renders"
```

The ten 24-bit stereo WAVs use generated signals only. No commercial recordings are included. Dry files last 4 seconds; wet files include 18 seconds of additional tail.

| Pair | Listen for |
|---|---|
| 01 Frozen Cello | Harmonic source becomes a suspended, stereo granular layer. Capture at 1.5 s, release at 9 s. Listen for grain seams and capture/release clicks. |
| 02 Ruined Piano | Synthetic piano-like notes with rounded attacks, harmonic grit, dotted echoes and a long tail. |
| 03 Glacial Echo | An impulse excites alternating echoes and a softer diffuse tail. |
| 04 Industrial Fog | Stronger nonlinear texture, darker direct signal, large reverberant space. |
| 05 Distant Cathedral | Asymmetric stereo source; check that the result stays wide, diffuse and balanced. |

Import a dry/wet pair into separate DAW tracks and solo alternately. Start at low monitor level. Match perceived loudness before comparing tone; peak normalization alone is insufficient. The terminal prints peak, RMS, DC, last sample above -60 dBFS, render time and an approximate single-thread real-time CPU ratio. This measures algorithm cost, not DAW deadline reliability.

The synthetic "cello" and "piano" are diagnostic waveforms, not physically modeled instruments. Test a real cello, guitar, vocal and piano recording after these checks. Assess transient smearing, high-frequency aliasing, dense-chord buildup, mono compatibility and apparent loudness. No subjective quality claim follows from automated tests.

Process your own file:

```powershell
.\build\frostbite_render_artefacts\Release\frostbite_render.exe "C:\Audio\source.wav" "C:\Audio\frostbite.wav" 7 20
```

Preset numbers are 0–7 in the editor's order. The optional final argument is tail seconds. General file rendering does not trigger capture; the generated Frozen Cello diagnostic has the documented capture sequence. Inputs up to 10 minutes are accepted.
