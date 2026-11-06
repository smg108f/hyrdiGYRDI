# hÿrdiGYRDI — design notes (v0.2)

How each control in the original spec became sound, what was assumed, and what's still open.

## How the sound is made

Each string is a **bowed waveguide** (in the tradition of Perry Cook's STK *Bowed* model). It is made of two delay lines meeting at the contact point, plus a stick-slip friction curve. On a hurdy-gurdy the bow is the wheel, so:

- **wheel surface speed = bow velocity.** Faster cranking makes the sound louder, grittier and more "caught". Pitch is never affected.
- **bow pressure** comes from the wheel surface, the pads, the handler and key velocity.
- **loudness follows crank speed.** The friction model on its own barely gets louder when bowed faster, so an explicit speed-to-loudness curve sits after the strings. As on a real hurdy-gurdy, cranking harder is the volume control.
- **tuning is exact for every material.** The loop's total delay (delay lines + damping filter + stiffness allpasses + bow junction) is computed so each string lands within a few cents, whatever its material. Before this, materials disagreed by up to ±40 cents.
- **every material has its own pressure map.** Too little bow pressure makes a string flip into a higher mode; too much causes sub-harmonic "anomalous low frequency" tones. Each material's stable zone was found by search, and the string also starts each note as if the wheel were already at speed, so it settles into its fundamental.
- the **sympathetic** string isn't bowed. It is a tuned resonator fed by the other strings.

The signal chain is: strings → mixer → pads → body resonance → cover (lid filter) → EQ → output. The cover's creak and slam sounds go through their own reverb.

## Spec → implementation

| Spec control | What it does in v0.2 |
|---|---|
| **body**: woods / metals / organics / scrap-plastics | 5-mode resonator per material: woods are warm, metals are high-Q and ringing, organics are hollow and gourd-like, plastics are boxy with a rattle. *Body Resonance* at 0% makes it a purely visual choice, as the spec asked. Also changes the background, the lid colour and the cover creak pitch. |
| **body**: decorations | Visual only: None / Sigils / Runes / Stickers. |
| **body**: 7/6/4/3 strings | *Body Build* choice (Body menu → Build). Strings that don't exist in a build are silent and hidden. |
| **handler**: ghost / devil / seer / witch | Shapes bow speed, bow pressure, wheel noise and direction over each crank revolution. **Ghost**: light, breathy bow with slow tidal swells (about 9 s) plus a swell every turn, and now and then a turn that nearly fades out. **Devil**: two violent shoves per turn (the percussive "coup"); each one surges, growls with added grit, and kicks the familiar. **Seer**: calm, even and clean, the reference drone. **Witch**: rocks the crank back and forth, so the sound stalls and rasps. *Handler Influence* at 0 means pure manual control. Measured loudness swing with one note held: Seer ~3 dB, Devil ~11 dB, Ghost ~15 dB, Witch ~29 dB. |
| **grind**: speed (fader) × multiplier (dial) | Speed 0–1 × RPM multiplier 0.25–4×, for roughly 0–480 RPM. Mod wheel (CC1) adds to speed. |
| **grind**: direction | −1 (pull) … 0 (stall) … +1 (push). Pitch bend down reverses it. |
| **wheel**: rosin on wood / blood on stone / polished metal / chalkboard | Changes friction and noise. **Rosin** is the classic sound. **Blood on stone** is high pressure with gritty rumble and jitter. **Polished metal** is slippery, glassy and thin. **Chalkboard** is very high pressure with random stick-slip squeal. |
| **pads**: cotton / bone / glass / titanium | Pressure offset plus tone. **Cotton** is soft and dark. **Bone** is a woody mid-bump. **Glass** adds a ringing shimmer. **Titanium** is hard, bright and slightly driven. A **heat** value rises the longer you play and drives the visuals: cotton smoulders, glass glows, titanium throws sparks. |
| **strings**: gut-string / steel-wire / nylon-cord / fiber-light / hemp-rope | Each material is designed to complement a familiar (suggested pairs: Shiba-Soul + Gut-String, Gremlin + Nylon-Cord, Troll + Steel-Wire, Fairy + Fiber-Light, Bees-Nest + Hemp-Rope). **Gut-String**: warm and woody, with a soft low-mid glow. **Steel-Wire**: cold and metallic, with stretched (inharmonic) overtones that ring; the stretch eases off up the range, as on real wire. **Nylon-Cord**: synthetic, hollow and nasal. **Fiber-Light**: bowed near the bridge for a glassy, airy tone, with a slow shimmer. **Hemp-Rope**: coarse, dark and fuzzy, with the fibres catching (pitch wobble and grain). Each string slot can be toggled by clicking it. |
| **familiar**: shiba-soul / gremlin / troll / fairy / bees-nest | The *chien* (buzzing "dog" bridge) on the familiar strings. Each one has its own rhythm, locked to crank revolutions, and its own voice. It **only wakes up above a wheel-speed threshold**. **Shiba-Soul**: fire crackling on a cold night; a warm buzz with random pops and embers. **Gremlin**: electrical interference; bit-crushed, glitching, syncopated and random. **Troll**: water under the bridge; each hit swells in and washes out like a tide, with a dark undertow an octave down and the odd bubble. **Fairy**: celestial glass bells tuned to the instrument's key, with a slow shimmer. **Bees-Nest**: a fast, light, buzzing swarm. The **Exciter** dial goes down to *sleep* (off). Pressing a key, or a Devil shove, kicks it. At the default Exciter of 50% each sits roughly 9–10 dB below the instrument. |
| **keys**: 24 notes | MIDI notes from *Low Key* to *Low Key* + 23. The melody is monophonic with last-note priority, like real tangents. |
| **cover**: open/close fader | Lid openness lowpasses and attenuates the instrument. Moving the fader makes a creak (pitch set by the body material, volume set by the *Sound* knob). |
| **cover**: close on the 25th key | Slams the lid with a thump. **The reverb size and amount depend on how open the lid was before the slam.** The lid stays shut until you move the Open fader again. |
| **tuners** | *Key* (the drone tonality, default G), *Drone Octave*, *Melody Transpose*, and *Detune*, which puts paired strings out of tune with each other for chorus/beating. |
| **mixer** | Melody / Familiar / Bass / Sympathy volumes. |
| **EQ-knobs** | Low shelf 180 Hz, mid peak 1 kHz, high shelf 5 kHz, ±12 dB. |

## Changes in v0.2

- Maker name shows as **voluntyr** in hosts; the plugin window shows "by völuntyr".
- Button to show/hide the controls pane (saved with the session), plus tooltips on every knob.
- **Handlers** reworked to sound clearly different (speed, pressure, noise and direction each shaped per handler).
- **Crank speed now controls loudness**, as on a real hurdy-gurdy.
- **Familiars** much louder (about 9–10 dB below the instrument at default, previously 21–29 dB) and each given its own voice. **Bees-Nest** added (the old Fairy sound); **Fairy** is now celestial glass bells.
- **String materials** renamed and rebuilt to pair with the familiars; **Hemp-Rope** added.
- **Tuning fixed**: materials previously disagreed by up to ±40 cents; every material is now within a few cents.
- **String stability fixed**: no more octave jumps or sub-harmonic undertones on certain notes.

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

## Verification

- v0.1 and v0.2 build cleanly as VST3 + Standalone on Linux (JUCE 8.0.9). The CMake also produces AU on macOS, and the user's Mac build of v0.1 succeeded.
- Passes **pluginval** at strictness level 8, including state save/restore, automation, parameter fuzzing and opening the editor.
- Offline render test covers every handler, familiar, wheel, pad, string, body and build, plus the cover, reverse crank and tempo sync. There were no NaNs and no clipping.
- v0.2 tuning/stability sweep: all five string materials on both melody strings, MIDI notes 36–76, in normal (Seer) and fast-Devil playing. Each is within a few cents, with the fundamental dominant and no sub-harmonic undertones (except Devil's intended pulsing sidebands).
