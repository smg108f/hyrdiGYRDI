# hÿrdiGYRDI — design notes (v0.1)

How each control in the original spec became sound, what was assumed, and what's still open.

## How the sound is made

Each string is a **bowed waveguide** (in the tradition of Perry Cook's STK *Bowed* model). It is made of two delay lines meeting at the contact point, plus a stick-slip friction curve. On a hurdy-gurdy the bow is the wheel, so:

- **wheel surface speed = bow velocity.** Faster cranking makes the sound louder, grittier and more "caught". Pitch is never affected.
- **bow pressure** comes from the wheel surface, the pads and key velocity.
- the **sympathetic** string isn't bowed. It is a tuned resonator fed by the other strings.

The signal chain is: strings → mixer → pads → body resonance → cover (lid filter) → EQ → output. The cover's creak and slam sounds go through their own reverb.

## Spec → implementation

| Spec control | What it does in v0.1 |
|---|---|
| **body**: woods / metals / organics / scrap-plastics | 5-mode resonator per material: woods are warm, metals are high-Q and ringing, organics are hollow and gourd-like, plastics are boxy with a rattle. *Body Resonance* at 0% makes it a purely visual choice, as the spec asked. Also changes the background, the lid colour and the cover creak pitch. |
| **body**: decorations | Visual only: None / Sigils / Runes / Stickers. |
| **body**: 7/6/4/3 strings | *Body Build* choice (Body menu → Build). Strings that don't exist in a build are silent and hidden. |
| **handler**: ghost / devil / seer / witch | Modulates wheel speed and direction over each crank revolution. **Ghost** drifts and swells. **Devil** gives two hard pushes per turn (the percussive, Guilhem-Desq-style "coup"). **Seer** is almost perfectly steady. **Witch** rocks the crank back and forth, so the sound stops and restarts. *Handler Influence* at 0 means pure manual control. |
| **grind**: speed (fader) × multiplier (dial) | Speed 0–1 × RPM multiplier 0.25–4×, for roughly 0–480 RPM. Mod wheel (CC1) adds to speed. |
| **grind**: direction | −1 (pull) … 0 (stall) … +1 (push). Pitch bend down reverses it. |
| **wheel**: rosin on wood / blood on stone / polished metal / chalkboard | Changes friction and noise. **Rosin** is the classic sound. **Blood on stone** is high pressure with gritty rumble and jitter. **Polished metal** is slippery, glassy and thin. **Chalkboard** is very high pressure with random stick-slip squeal. |
| **pads**: cotton / bone / glass / titanium | Pressure offset plus tone. **Cotton** is soft and dark. **Bone** is a woody mid-bump. **Glass** adds a ringing shimmer. **Titanium** is hard, bright and slightly driven. A **heat** value rises the longer you play and drives the visuals: cotton smoulders, glass glows, titanium throws sparks. |
| **strings**: gut / steel / nylon / fiber | Loop damping and brightness: gut is warm, steel is bright and long-sustaining, nylon sits in between, fiber is dark and breathy. Each string slot can be toggled by clicking it. |
| **familiar**: shiba-soul / gremlin / troll / fairy | The *chien* (buzzing "dog" bridge). It buzzes on rhythm patterns locked to crank revolutions, and **only wakes up above a wheel-speed threshold**. Shiba is a steady friendly bark. Gremlin is syncopated, nasty and random. Troll gives one heavy low stomp per turn. Fairy is a light triplet sparkle. The **Exciter** dial goes down to *sleep* (off). Pressing a key also gives it a little kick. |
| **keys**: 24 notes | MIDI notes from *Low Key* to *Low Key* + 23. The melody is monophonic with last-note priority, like real tangents. |
| **cover**: open/close fader | Lid openness lowpasses and attenuates the instrument. Moving the fader makes a creak (pitch set by the body material, volume set by the *Sound* knob). |
| **cover**: close on the 25th key | Slams the lid with a thump. **The reverb size and amount depend on how open the lid was before the slam.** The lid stays shut until you move the Open fader again. |
| **tuners** | *Key* (the drone tonality, default G), *Drone Octave*, *Melody Transpose*, and *Detune*, which puts paired strings out of tune with each other for chorus/beating. |
| **mixer** | Melody / Familiar / Bass / Sympathy volumes. |
| **EQ-knobs** | Low shelf 180 Hz, mid peak 1 kHz, high shelf 5 kHz, ±12 dB. |

## Assumptions to confirm or change

1. **Drone tuning.** For key *K*: bass = K2 and the fifth below it; familiar = K3 and the fifth above it; sympathy = K4. (With the default key G, that's G2/D2, G3/D4 and G4.) That's a conventional G/C-style setup, but it was my choice, not the spec's.
2. **Open melody string** (heard in *Always turning* mode with no key held) sounds one semitone below the lowest key, like a real hurdy-gurdy where the first key is a half step above the open string.
3. **Default keyboard** starts at C3 (MIDI 48), so the miniLAB 3's 25 keys cover 24 notes plus the slam key. If your miniLAB sends a different octave, change *Low Key*.
4. **"Grind" speed and direction are separate**, and direction at 0 means a stalled wheel (silence). The spec's "push/pull" could also mean something else, like bow-pressure push.
5. **Presets.** The spec's "presets trigger from popout lists" is read as each part's options being its presets: clicking a part opens its list. A full preset browser for saving whole-instrument snapshots isn't built yet. The cover nameplate shows the current Handler & Familiar.

## Not done yet

- **Artwork.** The floating cosplay instrument, per-body backgrounds and familiar creatures are drawn with simple vector shapes. The UI is built so painted art can replace each `draw…` function in `PluginEditor.cpp` one part at a time.
- **Mobile standalone app** (iOS/Android). JUCE can target both, but it needs a touch-first layout and Apple/Google developer setup.
- **miniLAB 3 template.** Right now you map it in the DAW. A built-in MIDI-learn or a fixed CC map could come next.
- **Saved presets / preset browser.**
- **Optional recorded textures** (key thunks, real wood creaks) could be layered in from CC0 sources such as Freesound, if the synthesized ones aren't enough.

## Verification done for v0.1

- Builds cleanly as VST3 + Standalone on Linux (JUCE 8.0.9). The CMake also produces AU on macOS.
- Passes **pluginval** at strictness level 8, including state save/restore, automation, parameter fuzzing and opening the editor.
- Offline render test covers every handler, familiar, wheel, pad, string, body and build, plus the cover, reverse crank and tempo sync. There were no NaNs and no clipping, and the melody string tunes to A4 = 439.5 Hz (about 2 cents flat).
