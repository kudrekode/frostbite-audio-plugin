# Dependencies and distribution

FROSTBITE uses JUCE **9.0.4**, pinned release commit `3fb5cbc0340758596dac39f7c8de47d562b4896e`. JUCE supplies the VST3 SDK headers and its normal OS/media/graphics dependencies.

JUCE is dual licensed under AGPLv3 and the commercial JUCE license. The source checkout includes the authoritative `external/JUCE/LICENSE.md` and `external/JUCE/JUCE.spdx.json` inventory. This local engineering build does not establish that its owner holds a commercial JUCE license. Before distributing a closed-source binary, ensure the applicable JUCE license is in place; an AGPL distribution must satisfy that license's source and other obligations. No commercial license was purchased by this project setup.

Primary sources checked for this build:

- [JUCE 9.0.4 release](https://github.com/juce-framework/JUCE/releases/tag/9.0.4)
- [JUCE licensing options](https://juce.com/get-juce/)
- [JUCE 9 terms](https://juce.com/legal/juce-9-licence/)
- [JUCE dependency inventory](https://github.com/juce-framework/JUCE/blob/9.0.4/JUCE.spdx.json)

Keep the bundled dependency notices with redistributed binaries. No proprietary artwork, fonts, samples or recordings are bundled. The editor requests system fonts and falls back through JUCE. The DSP and diagnostic signals are original source in this repository. No project-wide open-source license has been assigned on the owner's behalf.
