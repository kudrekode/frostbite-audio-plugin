# FROSTBITE UI feedback repair - 20 October 2026

The static-control failure was reproduced and the repair was verified in Studio One 6.6.4.102451 on Windows, using Windows Audio at 48 kHz / 480 samples in a separate blank test song. All eight main controls now visibly change their indicators, active arcs and numbers when dragged. The installed and built VST3 binaries have the same SHA256: `80255DEA67DFDA6E262A5806ACE0281D78FB06BB847CDE2FE421D825C978DE40`.

The confirmed failure is native repaint delivery. Before the repair, dragging Damage produced 86.1% in Studio One and 0.861000061 in the real JUCE slider's accessibility value, while the visible control remained at 12.0%. Direct component rendering passed the same control's pixel checks. The active loaded module path was verified, so this was not merely a stale test binary or an unattached parameter.

Selecting the Windows software renderer alone was insufficient: a second installed candidate still displayed the restored 86.1% while the host and slider changed to 52.4%. The pinned JUCE 9 Windows source explains the dependency. In `external/JUCE/modules/juce_gui_basics/native/juce_Windowing_windows.cpp`, the Direct2D path presents on vblank; the GDI/software path also queues dirty rectangles until `onVBlank()` calls `InvalidateRect`. GDI's `performAnyPendingRepaintsNow()` flushes those existing rectangles and services pending native painting. The final repair selects software rendering when the host peer attaches or is recreated, then calls that supported peer method from GUI-thread control callbacks. Native Studio One controls immediately update with this combination. The evidence identifies the stalled vblank-dependent repaint path; the underlying host/display/driver reason for the missing presentation was not separately instrumented.

JUCE attachments remain authoritative. Each of the 29 sliders has a persistent APVTS attachment with correct construction/destruction order; the three buttons and two choices also have persistent attachments. JUCE copies each parameter's range, skew and snapping to its slider, and marshals worker-thread changes to the GUI thread. Baseline tests already passed the attachment, normalisation and actual arc/indicator pixel checks. No fixed-value drawing, duplicate parameter authority, stale visual cache or ID mismatch was found.

Two additional defects were repaired:

| Defect | Evidence and repair |
|---|---|
| Motion Rate lost precision | Its 0.01 Hz interval was formatted to one decimal: 0.03 Hz appeared as 0.0 Hz. Sub-0.1 intervals now retain two decimals. The baseline regression failed before this correction. |
| Installer backups were scanned as plugins | Studio One scanned a marked backup before the active bundle and reported the second class as already cached. Backups now live outside the VST3 scan tree; only marked project backups are migrated. The original backup and active binary had identical hashes, so this installation defect is not claimed as the original graphical root cause. |

The visual refinements preserve the graphite/ice appearance: one clamped JUCE proportional position drives the arc and indicator, with a stronger active stroke and pointer dot, focused-control title feedback, and non-zero macro markers. Capture/Release and bypass text respond directly to attached state. The editor is opaque. The 24 Hz timer invalidates only waveform and footer regions with scaled coordinates and flushes pending native painting; fundamental knob value changes flush from their own callbacks without requiring that timer. GUI work never moves to the audio thread, and no new audio/GUI lock is introduced.

DSP sources, macro mappings, parameter IDs/ranges/defaults/version hints, factory preset data, state schema, manufacturer codes, bundle ID and VST3 identity are unchanged. Capture remains intentionally unarmed after preset/state restoration.

Files changed:

| Files | Purpose |
|---|---|
| `Source/PluginEditor.cpp`, `Source/PluginEditor.h` | Software-peer selection and GUI-thread dirty-region flushing, stronger indicators, state/focus feedback, bounded animation invalidation. |
| `Source/Parameters.cpp` | Motion Rate numeric precision. |
| `Tests/UiTests.cpp`, `CMakeLists.txt` | Dedicated UI CTest, actual rendered-pixel assertions and native immediate-paint regression. |
| `Tests/HostCheck.cpp` | Worker-thread automation through the compiled VST3 host interface, state and editor reopening checks. |
| `Tests/InstallerTests.ps1`, `scripts/install.ps1` | External backups, marked legacy migration, running-host guard and isolated regression fixtures. |
| `scripts/build.ps1`, `scripts/package.ps1` | Current validation logs, installer checks and a separate repair package containing GUI evidence. |
| `readme.md`, `docs/COMPLETION.md`, this report | Repair and installation documentation. |

Native Studio One acceptance:

| Check | Observed result |
|---|---|
| All eight main controls dragged through native desktop mouse input | Freeze 81.7%, Damage 76.5%, Distance 38.7%, Echo 41.7%, Abyss 71.7%, Motion 36.7%, Mix 71.7%, Output 2.0 dB. Visible indicators, arcs and readouts changed and matched the host's selected parameter value. |
| Freeze endpoints, intermediate value and reset | 0.0%, 50.4%, 100.0%; double-click restored 65.0%, with the corresponding visible positions. |
| Representative advanced control | Details opened immediately; Grain Size changed from 180 ms to 262 ms with visible indicator/arc movement. |
| Presets/defaults | Glacial Echo and Distant Cathedral moved controls to their stored values; Init restored defaults. |
| Capture and bypass | Capture visibly became Release; bypass highlighted and the header displayed BYPASSED / DRY SIGNAL. |
| Saved song and fresh editor | Saved only `build/qa/Frostbite-ui-test/Frostbite-ui-test.song`, exited Studio One normally, restarted it and reopened that song. Distant Cathedral restored Freeze 45%, Damage 4%, Distance 68%, Echo 18%, Abyss 92%, Motion 18%, Mix 72%, Output -3 dB, with matching visible positions. |
| Studio One automation playback | Recorded a Freeze automation lane, switched to Read and played the repository's diagnostic dry WAV on the same track. Freeze visibly moved from 45.0% to 80.2%, including arc, indicator and number, matching the track's automation value. Waveform/meters also updated during playback. |

Native screenshot evidence is in `build/qa/studio-one/`: `all-eight-dragged.png`, `freeze-min.png`, `freeze-mid.png`, `freeze-max.png`, `freeze-reset.png`, `advanced-grain-dragged.png`, `preset-glacial-echo.png`, `capture-release-state.png`, `bypass-state.png`, `saved-song-restored.png`, `automation-read-start.png` and `automation-read-high.png`. These are captures of the live Studio One window, separate from offscreen component tests.

The first automation attempt used an empty audio track: during transport, Studio One's lane changed but the plugin stayed at its previous value until Stop. With the diagnostic clip active, automation and GUI values followed one another. This is consistent with the pinned JUCE VST3 wrapper's `Param::setNormalized` withholding processor updates while playing, with `processParameterChanges` delivering them through processing. No second parameter authority or GUI polling workaround was added for host-idle processing. Automation acceptance here uses active audio. The test song was left open, stopped, with automation Off for free editing.

The full `scripts/build.ps1` Release pipeline passed after the final repair: all three CTests (DSP, Plugin, UI), independent VST3 host validation, installer fixtures and diagnostic renders. All 13 DSP groups passed, including zero process-time heap allocation, extreme automation, capture, delay, reverb and exact dry/bypass. All ten current diagnostic WAVs are byte-for-byte identical to the original release package. Release standalone and VST3 both built successfully.

The UI suite covers every main knob and all 21 advanced rotary controls. `build/qa/ui/control-matrix.csv` records the individual results. Checks include initial state; worker-thread host minimum/midpoint/maximum; range/skew/step normalisation; real displayed label text and formatter round trips; synthetic JUCE mouse handlers and double-click reset; editable-label text commit; and GUI-thread listener execution. Image checks inspect the indicator at the expected angle, active/empty arc and changing readout pixels, including whole-editor compositing. Eighteen PNGs cover main and four detail pages at min/mid/max plus 800/1500/1600-pixel editor widths.

Further UI tests cover all three bool and both choice attachments, eight factory presets, preset files, defaults, worker-thread state restoration, editor reopening after changes while closed, and destruction with a pending attachment update. A real visible native wrapper verifies software selection on late peer attachment and recreation. A paint listener requires each synchronous slider change to paint before returning, without a dispatch-loop pump, snapshot, explicit test-side peer flush or animation timer. This regression targets the actual native repaint failure rather than only mathematical mapping.

The independent VST3 host scans and instantiates the compiled bundle, verifies 35 exported parameters (34 plugin parameters plus the wrapper program parameter), processes 12 sample-rate/block-size configurations, and creates/reopens the editor. It automates all eight macros plus Grain Size, Echo Time, Feedback, Decay, Echo Tone, Motion Rate and Extra Drive from a worker thread at minimum/midpoint/maximum, checks finite audio, and verifies state restoration. Native VST3 child-window controls are not traversable through the host wrapper's JUCE tree, so direct editor tests provide pixel assertions and Studio One provides the native mouse/preset/restoration evidence.

Remaining manual acceptance is physical keyboard numeric entry and an audible before/after audition. Editable-label commit passed for all 29 knobs, but desktop keyboard injection did not establish reliable focus in Studio One, so native numeric typing remains unverified. A diagnostic source was played for automation acceptance, but no audible capture or before/after listening pass is claimed. Studio One automation was checked for Freeze; the independent VST3 host and direct editor tests cover the broader parameter matrix. No numerical screen-latency measurement or coverage of every Windows display/GPU configuration is claimed.

Evidence logs: `build/final-validation.log`, `build/Testing/Temporary/LastTest.log`, `build/ui-results.txt`, `build/host-results.txt`, `build/installer-results.txt`, `build/install-results.txt`, `build/render-metrics.txt`, `build/dsp-render-comparison.txt`, `build/studio-one-frostbite-scan.txt` and `build/studio-one-postinstall-scan.txt`. Only FROSTBITE-related scanner entries are retained. The loaded module resolves to the active user bundle; the scan tree contains one FROSTBITE binary, with rollback copies outside it.

Build outputs:

- VST3: `build/Frostbite_artefacts/Release/VST3/FROSTBITE.vst3`
- Standalone: `build/Frostbite_artefacts/Release/Standalone/FROSTBITE.exe`
- Corrected package: `dist/FROSTBITE-1.0.0-UI-Repair-Windows-x64.zip`
- Installed bundle: `%LOCALAPPDATA%/Programs/Common/VST3/FROSTBITE.vst3`

The original release and the earlier unsuccessful candidate are preserved under separate names. Use the UI-Repair package above for this repair. Its validation directory includes automated logs, offscreen images and native Studio One screenshots; the SHA256 manifest covers packaged files.

The corrected binary has already been installed with Studio One closed and tested after restart. To install again from this workspace or the extracted repair package, close Studio One and FROSTBITE hosts, then run:

```powershell
.\scripts\install.ps1 -Scope User -Replace
```

The installer verifies the binary hash and keeps marked rollback copies under `%LOCALAPPDATA%/Frostbite/Plugin Backups` (System scope uses `%PROGRAMDATA%`). Restart Studio One and rescan if old cached backup paths remain. The script does not edit unrelated plugins or Studio One cache/settings files.
