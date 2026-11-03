# hÿrdiGYRDI

a synth hurdy-gurdy

hÿrdiGYRDI is a playable synthesizer modelled on the hurdy-gurdy. It runs as a **VST3 / AU plugin** in your DAW (macOS, Windows, Linux) and also as a **standalone app**. It uses no samples: every string is a physical model of a rosined wheel bowing a string. Crank speed changes the bow speed and grit, and the pitch stays where it is.

> **Status: v0.1, playable prototype.** The sound engine and every control from the design doc work. The visuals are vector placeholders until real artwork exists. See [`docs/DESIGN-NOTES.md`](docs/DESIGN-NOTES.md) for how the spec was interpreted and what's still open.

## Building on macOS

You need:
- **Xcode Command Line Tools**: `xcode-select --install`
- **CMake** 3.22 or newer: `brew install cmake`

Then, from this folder:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
```

The first configure downloads JUCE 8.0.9, which takes about a minute. The build produces a universal binary (Apple Silicon + Intel) and **copies the plugins straight into place**:

- `~/Library/Audio/Plug-Ins/VST3/hyrdiGYRDI.vst3`
- `~/Library/Audio/Plug-Ins/Components/hyrdiGYRDI.component` (AU)
- the standalone app is at `build/hyrdiGYRDI_artefacts/Release/Standalone/hyrdiGYRDI.app`

**Ableton Live:** open Settings → Plug-Ins, turn on *Use VST3 Plug-in System Folders* (and/or *Use Audio Units*), then click **Rescan**. hÿrdiGYRDI shows up under Plug-Ins → voluntyr.

Windows (Visual Studio 2022) and Linux use the same two commands.

## Playing it

- **Keys:** 24 keys starting at *Low Key* (default C3 = MIDI 48) set the melody pitch. The **25th key** slams the cover shut.
- **The crank turns while a key is held.** Set *Crank Mode* to *Always turning* for a continuous drone.
- **Mod wheel** speeds up the crank and **pitch bend down** reverses it. Both can be switched off.
- **Sync to tempo** locks crank revolutions to the DAW's BPM, so the familiar's buzz rhythm stays in time. In this mode *Speed* sets how hard the wheel is pushed.
- Click parts of the instrument to change them: body, wheel, crank/handler, pads, pegbox (string material), the familiar and the cover. Click a string to mute or unmute it.
- Every parameter can be automated and MIDI-mapped. To use a miniLAB 3 in Ableton, use *MIDI Map* mode (⌘M) to assign its faders and knobs.

## Testing without a DAW

```sh
cmake -B build -DHYRDI_BUILD_TESTS=ON && cmake --build build --target hyrdiRender
./build/hyrdiRender_artefacts/Release/hyrdiRender renders/
```

This renders a short tune for every handler, familiar and material to WAV files. It also checks the output for NaNs and clipping, and checks that the melody string is in tune.

## Layout

```
Source/dsp/Primitives.h   bowed-string waveguide, sympathetic string, body modes, filters
Source/dsp/Engine.h       the instrument: crank, handler, wheel, pads, familiar, cover, mixer, EQ
Source/Params.*           all parameters (IDs are saved in sessions, so don't rename them)
Source/PluginProcessor.*  JUCE glue + MIDI
Source/PluginEditor.*     the clickable instrument + knob panel
tests/RenderTest.cpp      offline render test
docs/DESIGN-NOTES.md      spec → implementation map, assumptions, open questions
```

## License note

JUCE is dual-licensed (AGPLv3 or commercial). If you distribute builds, the project needs to be AGPLv3-compatible open source, or you need a JUCE commercial licence. This repo doesn't have a license file yet; that choice is yours.
