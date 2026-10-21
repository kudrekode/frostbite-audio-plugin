# Studio One verification

Do this in a new empty song. Keep existing projects untouched.

1. Build/test, then install the complete `FROSTBITE.vst3` directory with `scripts/install.ps1`.
2. Restart Studio One. Look for **FROSTBITE**, vendor **Frostbite Audio**, in the Effects browser.
3. If missing, check **Studio One > Options > Locations > VST Plug-ins** and include the user VST3 directory. Enable startup scanning. If FROSTBITE was blocked, use **Reset blocklist** and restart. This rescans other blocked plugins too.
4. Import `renders/01-frozen-cello-dry.wav` on an audio track and insert FROSTBITE.
5. Play for at least a second, then press **Capture**. Stop the source; confirm the held source continues. Press **Release** and confirm the gradual fade and continuing spatial tail.
6. On **Glacial Echo**, turn Abyss down, use 100% Mix, and play an impulse. Confirm alternate left/right repeats. Change song tempo and compare quarter/dotted/triplet divisions.
7. Try all eight presets. Confirm audible distinctions, sensible gain, and matching displayed parameters. Save/load a user preset.
8. Automate Mix, Damage, Echo time, Abyss and Capture. Listen for clicks and verify Capture only triggers on rising edges.
9. Save the test song in a new folder, close it, reopen it. Check parameter values and preset name. Capture should be off; captured audio is intentionally not saved.
10. Compare bypass and Mix=0 / Output=0 dB with the original track. Check mono and stereo sources.
11. Test 44.1, 48 and 96 kHz; buffer sizes 64, 128, 512 and 1024. Watch host CPU peaks, change buffer sizes, stop/start, loop and seek.
12. Export a WAV offline and compare with playback. Freeze automation must have source audio before its trigger.
13. Resize the editor, test fine adjustment (Shift), double-click reset, text value entry and keyboard arrows.

Record the Studio One version, audio device/driver, sample rate and buffer size, and which steps actually passed. An independent VST3 host check is not a substitute for these DAW checks.

References: [PreSonus scanning guidance](https://support.presonus.com/hc/en-us/articles/360045544271-How-can-I-get-my-3rd-party-plug-ins-to-show-up-in-Studio-One), [blocklist behavior](https://support.presonus.com/hc/en-us/articles/360004055292-What-is-the-difference-between-skip-and-disable-in-the-new-plug-in-scan-and-how-do-I-manage-my-plug-ins-now).
