// hÿrdiGYRDI — the instrument engine. Header-only, no JUCE dependency except
// juce::Reverb for the cover-slam tail (passed in by the processor).
#pragma once

#include "Primitives.h"
#include <atomic>

namespace hg
{
//==============================================================================
enum StringSlot { Mel1, Mel2, Fam1, Fam2, Bass1, Bass2, Sym, NumSlots };

/** which string slots exist for each "build" (7, 6, 4, 3 strings) */
inline bool slotExists (int build, int slot)
{
    static const bool table[4][NumSlots] = {
        { 1, 1, 1, 1, 1, 1, 1 }, // 7: 2 melody, 2 familiar, 2 bass, 1 sympathy
        { 1, 1, 1, 1, 1, 1, 0 }, // 6: 2 melody, 2 familiar, 2 bass
        { 1, 1, 1, 0, 1, 0, 0 }, // 4: 2 melody, 1 familiar, 1 bass
        { 1, 0, 1, 0, 1, 0, 0 }, // 3: 1 melody, 1 familiar, 1 bass
    };
    return table[std::clamp (build, 0, 3)][std::clamp (slot, 0, NumSlots - 1)];
}

/** string materials — each one is paired with a familiar it complements */
struct StringMaterial
{
    float pole44, loopGain;     // loop brightness (at 44.1 kHz) and sustain
    float betaScale;            // bow position relative to each slot's default (smaller = nearer the bridge)
    int   apStages; float apCoef; // stiffness / inharmonicity
    float pressure;             // added bow pressure
    float rough;                // fibrous friction noise
    float jitterCents;          // fast random pitch wobble
    float vibCents, vibHz;      // slow shimmer
    int   colorType; float colorHz, colorDb, colorQ;   // 0 none, 1 peak, 2 high shelf, 3 low shelf
    int   color2Type; float color2Hz, color2Db, color2Q;
    float warmth;               // gentle asymmetric saturation (2nd harmonic glow)
    float tuneTrim = 0.0f;      // measured tuning correction, samples
    float betaSpread = 1.0f;    // how much the slots' bow positions differ from each other (keeps all slots off bad fractions)
    // pressure map into the model's stable zone (found by search: fundamental dominant, no sub-harmonic "ALF" tones)
    float pBase = 0.62f, pRange = 0.36f, pBoost = 0.12f;
};

inline const StringMaterial& stringMaterial (int m)
{
    static const StringMaterial table[5] = {
        //  pole   gain   bow   ap  apCoef  press  rough  jit   vib   vibHz  colour 1                    colour 2                    warmth trim   spread  pBase pRange pBoost
        // Gut-String (Shiba-Soul): warm, woody, a soft glow in the low mids
        { 0.68f, 0.955f, 1.0f,  0, 0.0f,    0.0f,  0.0f,  0.0f, 0.0f, 0.0f,  1, 320.0f,  3.0f, 0.8f,   2, 5000.0f, -3.0f, 0.7f,  0.35f, 0.0f,  1.0f,   0.70f, 0.30f, 0.00f },
        // Steel-Wire (Troll): cold, metallic, stretched ringing overtones
        { 0.35f, 0.990f, 1.1f,  3, -0.45f,  0.05f, 0.0f,  0.0f, 0.0f, 0.0f,  2, 4500.0f, 4.0f, 0.7f,   3, 200.0f, -2.0f, 0.7f,   0.0f, -0.45f, 1.0f,  0.34f, 0.10f, 0.00f },
        // Nylon-Cord (Gremlin): synthetic and hollow, nasal, a little wired
        { 0.55f, 0.965f, 1.2f,  0, 0.0f,   -0.05f, 0.0f,  0.0f, 0.0f, 0.0f,  1, 1300.0f, 6.0f, 2.5f,   1, 3200.0f, 3.0f, 4.0f,   0.0f, 0.0f,  0.4f,   0.52f, 0.20f, 0.00f },
        // Fiber-Light (Fairy): bowed near the bridge — glassy, airy, shimmering
        { 0.50f, 0.975f, 0.70f, 0, 0.0f,   -0.10f, 0.0f,  0.0f, 6.0f, 0.23f, 2, 6000.0f, 5.0f, 0.7f,   3, 250.0f, -5.0f, 0.7f,   0.0f, 0.0f,  1.0f,   0.70f, 0.30f, 0.10f },
        // Hemp-Rope (Bees-Nest): coarse, fuzzy, dark, the fibres catching
        { 0.78f, 0.945f, 1.0f,  0, 0.0f,    0.15f, 0.10f, 9.0f, 0.0f, 0.0f,  1, 650.0f,  3.0f, 1.0f,   2, 3500.0f, -4.0f, 0.7f,  0.45f, 0.0f,  1.0f,   0.70f, 0.30f, 0.05f },
    };
    return table[std::clamp (m, 0, 4)];
}

/** everything the engine needs from the parameter set, refreshed once per block */
struct Settings
{
    int body = 0, build = 0, handler = 0, wheel = 0, pads = 0, strings = 0, familiar = 0, crankMode = 0;
    float bodyAmt = 0.6f, handlerAmt = 0.7f, speed = 0.6f, mult = 1.0f, direction = 1.0f;
    float wheelAmt = 0.7f, padsAmt = 0.5f, exciter = 0.5f;
    bool sync = false, modToSpeed = true, bendToDirection = true;
    bool stringOn[NumSlots] = { true, true, true, true, true, true, true };
    int keyBase = 48, transpose = 0, tunerKey = 7, droneOctave = 0;
    float detuneCents = 6.0f;
    float mixMelody = 0.8f, mixFamiliar = 0.55f, mixBass = 0.6f, mixSympathy = 0.4f;
    float eqLow = 0, eqMid = 0, eqHigh = 0;
    float coverOpen = 1.0f, coverVolume = 0.5f, masterDb = 0.0f;
    double hostBpm = 120.0;
    bool hostBpmValid = false;
};

/** lock-free values the UI reads to animate the instrument */
struct Telemetry
{
    std::atomic<float> crankAngle { 0 }, wheelSpeed { 0 }, familiarGlow { 0 }, padHeat { 0 };
    std::atomic<float> lidOpen { 1 }, slamFlash { 0 }, bowDirection { 1 };
    std::atomic<int>   heldNote { -1 };
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        for (auto& s : strings) s.prepare (sampleRate);
        sympathetic.prepare (sampleRate);
        body.prepare (sampleRate);
        fxBody.prepare (sampleRate);
        noiseLp.setCutoff (3000.0f, fs);
        noiseHp.setCutoff (2500.0f, fs);
        jitterLp.setCutoff (7.0f, fs);
        creakRes.setBandpass (650.0f, 12.0f, fs);
        thumpNoiseLp.setCutoff (900.0f, fs);
        popLp.setCutoff (300.0f, fs);
        roughLp.setCutoff (1500.0f, fs);
        startBoostDecay = std::exp (-1.0f / (0.09f * fs));
        for (auto& j : jitter) j.setCutoff (25.0f, fs);
        subLp.setCutoff (400.0f, fs);
        thumpLike  = std::exp (-1.0f / (0.04f * fs));
        waveRise   = msToCoef (110.0f, fs);
        waveFall   = std::exp (-1.0f / (0.45f * fs));
        rpmAttack  = msToCoef (70.0f, fs);
        rpmRelease = msToCoef (260.0f, fs);
        smooth30   = msToCoef (30.0f, fs);
        smooth5    = msToCoef (5.0f, fs);

        // bow positions differ slightly per slot for timbral variety
        for (int i = 0; i < NumSlots; ++i) strings[(size_t) i].setBowPosition (slotBeta[i]);

        lastBody = lastPads = lastStrings = lastFamiliar = -1;
        eqDirty = true;
        reset();
    }

    void reset()
    {
        for (auto& s : strings) s.reset();
        sympathetic.reset();
        body.reset(); fxBody.reset();
        rpm = 0; heldCount = 0; famEnv = 0; thumpEnv = 0; thumpNoiseEnv = 0; creakAmt = 0;
        for (auto& b : eq) b.reset();
        padFilter.reset();
    }

    //==========================================================================
    // MIDI
    void noteOn (int note, float velocity, const Settings& s)
    {
        if (note == s.keyBase + 24) { slam (velocity); return; }  // the 25th key closes the cover
        removeHeld (note);
        if (heldCount < (int) held.size()) held[(size_t) heldCount++] = note;
        lastVelocity = velocity;
        startBoost = 1.0f;
        // a fresh key press gives the familiar a little kick, like a wrist "coup"
        famEnv = std::max (famEnv, 0.6f * velocity);
    }
    void noteOff (int note) { removeHeld (note); }
    void allNotesOff() { heldCount = 0; }
    void modWheel (float v01) { mod = v01; }
    void pitchBend (float bipolar) { bend = bipolar; }

    //==========================================================================
    void setSettings (const Settings& s, int blockSize)
    {
        st = s;
        if (s.body != lastBody)
        {
            body.configure (s.body); fxBody.configure (s.body);
            static const float creakF[4] = { 650.0f, 1900.0f, 380.0f, 1100.0f };
            static const float creakQ[4] = { 12.0f, 40.0f, 6.0f, 10.0f };
            creakRes.setBandpass (creakF[s.body], creakQ[s.body], fs);
            lastBody = s.body;
        }
        if (s.strings != lastStrings)
        {
            const auto& m = stringMaterial (s.strings);
            for (int i = 0; i < NumSlots; ++i)
            {
                auto& str = strings[(size_t) i];
                str.setMaterial (m.pole44, m.loopGain);
                str.setDispersion (m.apStages, m.apCoef);
                str.setBowPosition ((0.124f + (slotBeta[i] - 0.124f) * m.betaSpread) * m.betaScale);
                str.setTuneTrim (m.tuneTrim);
                str.setPressureMap (m.pBase, m.pRange, m.pBoost);
            }
            auto setColor = [this] (Biquad& b, int type, float hz, float db, float q)
            {
                switch (type)
                {
                    case 1:  b.setPeak (hz, q, db, fs); break;
                    case 2:  b.setHighShelf (hz, db, fs); break;
                    case 3:  b.setLowShelf (hz, db, fs); break;
                    default: b = Biquad(); break;
                }
                b.reset();
            };
            setColor (matColor[0], m.colorType, m.colorHz, m.colorDb, m.colorQ);
            setColor (matColor[1], m.color2Type, m.color2Hz, m.color2Db, m.color2Q);
            lastStrings = s.strings;
        }
        if (s.pads != lastPads)
        {
            switch (s.pads)
            {
                case 0:  padFilter.setLowpass (2200.0f, 0.7f, fs); break;      // cotton: soft
                case 1:  padFilter.setPeak (1800.0f, 1.2f, 4.0f, fs); break;   // bone: woody click
                case 2:  padFilter.setBandpass (3300.0f, 25.0f, fs); break;    // glass: ringing shimmer
                default: padFilter.setHighShelf (4000.0f, 5.0f, fs); break;    // titanium: hard + bright
            }
            padFilter.reset();
            lastPads = s.pads;
        }
        if (s.familiar != lastFamiliar)
        {
            const auto& f = familiarCfg (s.familiar);
            famLp.setCutoff (f.lpHz, fs);
            famHp.setCutoff (f.hpHz, fs);
            famDecay = std::exp (-1.0f / (0.001f * f.decayMs * fs));
            lastFamiliar = s.familiar;
        }
        if (eqDirty || s.eqLow != lastEq[0] || s.eqMid != lastEq[1] || s.eqHigh != lastEq[2])
        {
            eq[0].setLowShelf (180.0f, s.eqLow, fs);
            eq[1].setPeak (1000.0f, 0.8f, s.eqMid, fs);
            eq[2].setHighShelf (5000.0f, s.eqHigh, fs);
            lastEq[0] = s.eqLow; lastEq[1] = s.eqMid; lastEq[2] = s.eqHigh;
            eqDirty = false;
        }

        // pitches ----------------------------------------------------------
        const float det = std::pow (2.0f, s.detuneCents / 1200.0f);
        const float bass1 = (float) (36 + s.tunerKey + 12 * s.droneOctave);  // e.g. G2 for key G
        const int melNote = heldCount > 0 ? held[(size_t) heldCount - 1] : s.keyBase - 1; // open string = one below the first key
        const float mel = (float) (melNote + s.transpose);
        strings[Mel1].setFrequency (mtof (mel));
        strings[Mel2].setFrequency (mtof (mel) * det);
        strings[Fam1].setFrequency (mtof (bass1 + 12.0f));
        strings[Fam2].setFrequency (mtof (bass1 + 19.0f) * det);
        strings[Bass1].setFrequency (mtof (bass1));
        strings[Bass2].setFrequency (mtof (bass1 - 5.0f) / det);
        sympathetic.setFrequency (mtof (bass1 + 24.0f) * std::sqrt (det));

        // cover: creak from fader motion, slam release when the fader moves again
        const float blockSec = (float) blockSize / fs;
        const float coverVel = blockSec > 0 ? std::fabs (s.coverOpen - prevCover) / blockSec : 0.0f;
        creakTarget = clampf (coverVel * 0.9f, 0.0f, 1.0f);
        prevCover = s.coverOpen;
        if (slammed && std::fabs (s.coverOpen - coverAtSlam) > 0.02f) slammed = false;
    }

    //==========================================================================
    /** renders mono dry into outL/outR (added) and the reverb-bound fx into fxL */
    void render (float* outL, float* outR, float* fx, int start, int num)
    {
        const auto& s = st;
        const auto& fam = familiarCfg (s.familiar);
        const bool  crankAlways = s.crankMode == 1;

        const float effSpeed = clampf (s.speed + (s.modToSpeed ? mod : 0.0f), 0.0f, 1.0f);
        const float effDir   = clampf (s.direction + (s.bendToDirection ? bend * 2.0f : 0.0f), -1.0f, 1.0f);
        const bool  synced   = s.sync && s.hostBpmValid;
        const float rpmGoal  = synced ? (float) s.hostBpm * s.mult : effSpeed * 120.0f * s.mult;
        const float intensity = synced ? effSpeed : 1.0f;

        // wheel material: base pressure, noise amount, jitter, chalk stick-slip
        static const float wheelP[4]  = { 0.55f, 0.68f, 0.32f, 0.78f };
        static const float wheelN[4]  = { 0.006f, 0.05f, 0.002f, 0.03f };
        static const float wheelJ[4]  = { 0.0f, 0.3f, 0.0f, 0.0f };
        static const float wheelSH[4] = { 0.0f, 0.0f, 0.0f, 0.25f };
        static const float padP[4]    = { -0.12f, 0.04f, -0.04f, 0.12f };
        const float wA = s.wheelAmt, pA = s.padsAmt;
        const float basePressure = lerpf (0.5f, wheelP[s.wheel], wA) + padP[s.pads] * pA;
        const float noiseAmt = wheelN[s.wheel] * wA, jitAmt = wheelJ[s.wheel] * wA, shAmt = wheelSH[s.wheel] * wA;
        if (s.wheel == 1) noiseLp.setCutoff (500.0f, fs); else if (s.wheel == 2) noiseLp.setCutoff (8000.0f, fs); else noiseLp.setCutoff (3000.0f, fs);

        const float lidTarget = slammed ? 0.0f : s.coverOpen;
        const float master = dbToGain (s.masterDb);

        bool on[NumSlots];
        for (int i = 0; i < NumSlots; ++i) on[i] = s.stringOn[i] && slotExists (s.build, i);

        for (int n = start; n < start + num; ++n)
        {
            // --- crank / wheel ------------------------------------------------
            const bool gate = crankAlways || heldCount > 0;
            const float target = gate ? rpmGoal : 0.0f;
            rpm += (target - rpm) * (target > rpm ? rpmAttack : rpmRelease);
            dirSm += (effDir - dirSm) * smooth30;

            handlerPhase += rpm / 60.0f / fs;
            if (handlerPhase >= 1.0f) handlerPhase -= 1.0f;
            HandlerOut ho = handlerMod (s.handler, handlerPhase);
            const float hA = s.handlerAmt;
            const float m = 1.0f + (ho.speed - 1.0f) * hA;     // speed multiplier, applied after the soft curve
            const float d = 1.0f + (ho.direction - 1.0f) * hA;
            if (ho.kick) famEnv = std::max (famEnv, 0.95f * hA); // the devil's shove slaps the familiar awake

            const float dir = dirSm * d;
            const float wheelSpeed = std::tanh (std::max (0.0f, rpm) / 90.0f);
            const float push = wheelSpeed * m;                 // effective bow speed (can exceed 1 on a shove)
            crankAngle += twoPi * (rpm * m / 60.0f) / fs * dir;
            if (crankAngle > twoPi) crankAngle -= twoPi; else if (crankAngle < 0) crankAngle += twoPi;

            // --- bow (wheel surface) ---------------------------------------
            const float noise = noiseLp.tick (rng.bipolar());
            float bowVel = 0.27f * push * intensity * dir * (0.65f + 0.35f * lastVelocity);
            if (jitAmt > 0) bowVel *= 1.0f + jitAmt * 6.0f * jitterLp.tick (rng.bipolar());
            const float hNoise = 1.0f + (ho.noise - 1.0f) * hA;
            if (s.wheel == 3) bowVel += noiseAmt * hNoise * noiseHp.tick (rng.bipolar()) * push;
            else              bowVel += (noiseAmt * hNoise + 0.012f * (hNoise - 1.0f)) * noise * push;

            if (shAmt > 0 && (shCounter -= 1.0f) <= 0.0f) { shValue = rng.unipolar(); shCounter = fs / 45.0f; }
            // dig in at the start of each note / crank start so the string settles into its fundamental
            if (gate && ! lastGate) startBoost = 1.0f;
            lastGate = gate;
            startBoost *= startBoostDecay;
            const float pressure = clampf (basePressure + shAmt * shValue + 0.08f * lastVelocity + ho.pressure * hA
                                           + 0.6f * startBoost, 0.05f, 0.98f);

            // --- strings -----------------------------------------------------
            const auto& mat = stringMaterial (s.strings);
            // at the start of a note the string "feels" the wheel already at speed (a slow ramp lets some
            // materials lock into a higher mode); loudness still fades in with the crank via `expr`
            if (gate && startBoost > 0.01f)
            {
                const float atSpeed = 0.27f * std::tanh (std::max (0.0f, rpmGoal) / 90.0f) * m * intensity * dir
                                      * (0.65f + 0.35f * lastVelocity);
                if (std::fabs (atSpeed) * startBoost > std::fabs (bowVel)) bowVel = atSpeed * startBoost + bowVel * (1.0f - startBoost);
            }
            // keep the bow inside the model's stable zone; loudness from cranking harder comes from `expr` below
            bowVel = 0.22f * std::tanh (bowVel / 0.22f);
            if (mat.rough > 0) bowVel += mat.rough * roughLp.tick (rng.bipolar()) * push * intensity;
            if (mat.jitterCents > 0 || mat.vibCents > 0)
            {
                vibPhase += mat.vibHz / fs; if (vibPhase >= 1.0f) vibPhase -= 1.0f;
                for (int i = 0; i < NumSlots - 1; ++i)
                {
                    float cents = mat.vibCents * std::sin (twoPi * (vibPhase + 0.17f * (float) i));
                    // (25 Hz-filtered noise is small, ~0.023 rms; x18 makes jitterCents roughly "cents of wobble, rms / 2.4")
                    if (mat.jitterCents > 0) cents += mat.jitterCents * 18.0f * jitter[(size_t) i].tick (rng.bipolar());
                    strings[(size_t) i].setPitchMod (std::exp2 (cents / 1200.0f));
                }
            }
            float mel = 0, famS = 0, bass = 0;
            if (on[Mel1])  mel  += strings[Mel1].tick (bowVel, pressure);
            if (on[Mel2])  mel  += strings[Mel2].tick (bowVel * 0.97f, pressure);
            if (on[Fam1])  famS += strings[Fam1].tick (bowVel, pressure);
            if (on[Fam2])  famS += strings[Fam2].tick (bowVel * 0.95f, pressure);
            if (on[Bass1]) bass += strings[Bass1].tick (bowVel, pressure);
            if (on[Bass2]) bass += strings[Bass2].tick (bowVel * 0.95f, pressure);

            // The friction model saturates, so on its own the string barely gets louder with bow speed.
            // On a real hurdy-gurdy, cranking harder IS the volume control, so restore that response here.
            // Normalised so the default speed (~0.66) sits at unity.
            const float exprTarget = std::pow (clampf (push * intensity * std::fabs (dir) / 0.66f, 0.0f, 3.0f), 0.85f);
            expr += (exprTarget - expr) * smooth5;
            mel *= expr; famS *= expr; bass *= expr;
            if (const float g = ho.growl * hA; g > 0.01f)
            {
                // asymmetric drive: adds both odd and even harmonics, like a bow digging in
                auto snarl = [g] (float v) { return v + g * (0.55f * (std::tanh (6.0f * v + 0.35f) - 0.3364f) - v); };
                mel = snarl (mel); famS = snarl (famS); bass = snarl (bass);
            }

            // --- the familiar (buzzing bridge / "chien") ------------------
            const int step = (int) (handlerPhase * (float) fam.steps) % fam.steps;
            if (step != lastStep)
            {
                float acc = fam.pattern[step];
                if (fam.chaos > 0 && rng.unipolar() < fam.chaos) acc = acc > 0 ? 0.0f : 0.6f * rng.unipolar();
                if (acc > 0) { famEnv = std::max (famEnv, acc); hitAcc = acc; }
                lastStep = step;
            }
            famEnv *= famDecay;
            const float buzzGate = smoothstep (0.12f, 0.4f, push * intensity);
            const float awake = s.exciter * buzzGate;           // the familiar is up and paying attention
            const float e = famEnv * awake;                      // ...and this is how hard it's hitting right now
            const float hit = hitAcc * awake;                    // a new rhythmic hit landed this sample (0 = none)
            hitAcc = 0.0f;
            if (awake > 1.0e-4f || famVoiceTail > 1.0e-4f)
            {
                const float buzz = familiarVoice (s.familiar, famS, e, awake, hit);
                famS = famS * (1.0f - 0.2f * e) + buzz;
                famVoiceTail = std::max (awake, famVoiceTail * 0.9999f);
            }

            // --- sympathetic (un-bowed, excited by the others) ------------
            float sym = 0;
            if (on[Sym]) sym = 0.06f * sympathetic.tick (0.05f * (mel + famS + 0.5f * bass));

            float x = s.mixMelody * mel + s.mixFamiliar * famS + s.mixBass * bass + s.mixSympathy * sym;

            // material colour
            x = matColor[1].tick (matColor[0].tick (x));
            if (mat.warmth > 0) x += mat.warmth * (0.6f * (std::tanh (1.6f * x + 0.25f) - 0.2449f) - 0.96f * x);

            // --- pads, body, cover lid, EQ ------------------------------------
            float padded;
            switch (s.pads)
            {
                case 2:  padded = x + 2.5f * padFilter.tick (x); break;
                case 3:  padded = std::tanh (1.8f * padFilter.tick (x)) / 1.8f; break;
                default: padded = padFilter.tick (x); break;
            }
            x += pA * (padded - x);
            x = body.tick (x, s.bodyAmt);

            lidSm += (lidTarget - lidSm) * smooth30;
            lidLp.setCutoff (500.0f * std::pow (40.0f, lidSm), fs);
            x = lidLp.tick (x) * (0.35f + 0.65f * lidSm);

            for (auto& b : eq) x = b.tick (x);
            x = std::tanh (0.55f * x * master);

            outL[n] += x;
            outR[n] += x;

            // --- cover fx (creak + slam thump), goes to the reverb bus ----
            creakAmt += (creakTarget - creakAmt) * smooth5 * 0.2f;
            float fxs = 0;
            if (creakAmt > 1.0e-3f)
            {
                creakPhase += (25.0f + 140.0f * creakAmt) * (1.0f + 0.3f * rng.bipolar()) / fs;
                float imp = 0;
                if (creakPhase >= 1.0f) { creakPhase -= 1.0f; imp = creakAmt * (0.5f + 0.5f * rng.unipolar()); }
                fxs += 3.0f * creakRes.tick (imp + 0.05f * creakAmt * rng.bipolar());
            }
            if (thumpEnv > 1.0e-4f || thumpNoiseEnv > 1.0e-4f)
            {
                thumpFreq += (45.0f - thumpFreq) * 0.0006f;
                thumpPhase += thumpFreq / fs;
                if (thumpPhase >= 1.0f) thumpPhase -= 1.0f;
                const float th = thumpEnv * std::sin (twoPi * thumpPhase)
                               + thumpNoiseEnv * thumpNoiseLp.tick (rng.bipolar());
                fxs += fxBody.tick (th, 0.8f);
                thumpEnv *= thumpDecay; thumpNoiseEnv *= thumpNoiseDecay;
            }
            fx[n] += clampf (fxs * s.coverVolume * master, -1.5f, 1.5f);

            // pads heat up while played, cool down slowly
            const float heatTarget = wheelSpeed * intensity * (gate ? 1.0f : 0.0f);
            padHeat += (heatTarget - padHeat) * (heatTarget > padHeat ? 1.0f / (3.0f * fs) : 1.0f / (6.0f * fs));
        }

        telemetry.crankAngle.store (crankAngle);
        telemetry.wheelSpeed.store (std::tanh (rpm / 90.0f));
        telemetry.familiarGlow.store (famEnv * s.exciter);
        telemetry.padHeat.store (padHeat);
        telemetry.lidOpen.store (lidSm);
        telemetry.bowDirection.store (dirSm);
        telemetry.heldNote.store (heldCount > 0 ? held[(size_t) heldCount - 1] : -1);
        slamFlashV *= 0.8f;
        telemetry.slamFlash.store (slamFlashV);
    }

    /** openness of the lid at the moment it was last slammed (drives reverb size) */
    float lastSlamOpenness() const { return slamOpen; }
    bool consumeSlamEvent() { const bool e = slamEvent; slamEvent = false; return e; }

    Telemetry telemetry;

private:
    struct FamiliarCfg { int steps; float pattern[12]; float drive, rattle, lpHz, hpHz, decayMs, chaos; };
    static const FamiliarCfg& familiarCfg (int type)
    {
        // steps per revolution, accent pattern, (unused) drive, (unused) rattle, voice lowpass, voice highpass, hit decay ms, randomness
        static const FamiliarCfg cfg[5] = {
            // shiba-soul: a steady friendly pulse, loosened a little like a fire
            { 8,  { 1, 0, .55f, 0, .9f, 0, .55f, .3f },              0, 0, 2600.0f, 60.0f, 90.0f, 0.15f },
            // gremlin: syncopated, nasty, unpredictable
            { 8,  { 1, 0, 0, .8f, 0, .9f, .4f, 0 },                 0, 0, 7500.0f, 400.0f, 35.0f, 0.35f },
            // troll: one big wave per revolution and a small backwash
            { 4,  { 1, 0, 0, .35f },                                0, 0, 900.0f, 30.0f, 450.0f, 0.0f },
            // fairy: sparse bells, never quite the same
            { 12, { 1, 0, 0, .5f, 0, 0, .8f, 0, 0, .4f, 0, .3f },   0, 0, 12000.0f, 2000.0f, 60.0f, 0.2f },
            // bees-nest: quick triplet swarm, light and high
            { 12, { 1, .4f, .6f, 1, .4f, .6f, 1, .4f, .6f, 1, .6f, .8f }, 5.0f, 0.2f, 11000.0f, 1200.0f, 22.0f, 0.1f },
        };
        return cfg[std::clamp (type, 0, 4)];
    }

    /**
        The familiar's voice. `x` = the familiar strings, `e` = rhythmic hit envelope,
        `awake` = how awake it is overall (exciter x crank speed), `hit` = > 0 on the sample a new hit lands.
        Returns the sound to add on top of the familiar strings.
    */
    float familiarVoice (int type, float x, float e, float awake, float hit)
    {
        const float famHz = strings[Fam1].targetF;
        switch (type)
        {
            case 0: // shiba-soul — fire crackling on a cold night: warm low buzz, pops and embers
            {
                const float warm = famLp.tick (std::tanh (x * 5.0f));                 // lp 2.6k: cosy, rounded
                // crackle: random pops, denser on each hit, with a few embers between hits
                const float popsPerSec = 70.0f * e + 6.0f * awake;
                float imp = 0.0f;
                if (rng.unipolar() < popsPerSec / fs)
                {
                    const float a = 0.3f + 0.7f * rng.unipolar() * rng.unipolar();   // mostly small, a few big
                    imp = (rng.bipolar() > 0 ? a : -a);
                    crackleRes.setBandpass (1200.0f + 4000.0f * rng.unipolar(), 2.0f, fs);
                    if (a > 0.75f) popThump = 0.6f * a;                              // the occasional log-shifting pop
                }
                popThump *= thumpLike;
                const float crackle = 2.2f * crackleRes.tick (imp) + popThump * popLp.tick (rng.bipolar());
                return 1.7f * e * warm + 5.0f * crackle * std::sqrt (awake);
            }
            case 1: // gremlin — the gremlin in the machine: crushed, electrical, glitching
            {
                if (hit > 0) { crushHold = 2 + (int) (rng.unipolar() * 9.0f); crushBits = 3.0f + 4.0f * rng.unipolar(); }
                if (--crushCount <= 0) { crushCount = crushHold; crushHeld = std::tanh (x * 22.0f); }  // sample-rate crush
                const float q = std::pow (2.0f, crushBits);
                const float crushed = std::round (crushHeld * q) / q;                    // bit crush
                if (rng.unipolar() < (3.0f * awake) / fs) glitch = 0.03f * fs;          // the odd electrical spit
                float zap = 0.0f;
                if (glitch > 0) { glitch -= 1.0f; zap = (std::fmod (glitch, 37.0f) < 18.0f ? 0.35f : -0.35f) * awake; }
                return 3.2f * famHp.tick (famLp.tick (1.3f * e * crushed + zap));
            }
            case 2: // troll — water under the bridge: each hit swells in and washes out like a tide
            {
                waveTarget = std::max (waveTarget, hit);
                waveTarget *= waveFall;
                waveEnv += (waveTarget - waveEnv) * waveRise;        // slow swell in, long wash out
                const float wv = waveEnv;  // hits already carry the exciter amount
                if (--waveCtl <= 0)                                   // re-voice the water every 32 samples
                {
                    waveCtl = 32;
                    waterLp.setLowpass (180.0f + 1500.0f * waveEnv, 1.8f, fs);   // the wave brightens as it crests
                }
                const float water = waterLp.tick (rng.bipolar());
                // the odd bubble glugging up from below
                if (rng.unipolar() < (14.0f * wv) / fs) { bubble.setBandpass (260.0f + 520.0f * rng.unipolar(), 18.0f, fs); bubbleKick = 1.0f; }
                const float glug = 18.0f * bubble.tick (bubbleKick * (0.5f + 0.5f * rng.unipolar()));
                bubbleKick = 0.0f;
                // dark undertow: the familiar strings an octave down, riding the tide
                const float lowX = subLp.tick (x);
                if (lowX > 0.0f && prevLowX <= 0.0f) subSign = -subSign;
                prevLowX = lowX;
                const float under = famLp.tick (std::tanh (3.0f * (0.5f * x + 1.6f * subSign * std::fabs (lowX))));
                return wv * (1.9f * under + 1.5f * water) + 0.5f * glug * awake;
            }
            case 3: // fairy — celestial glass bells, tuned to the drone, with a slow shimmer
            {
                if (hit > 0)
                {
                    static const float pitches[5] = { 4.0f, 6.0f, 8.0f, 10.0f, 12.0f };   // key, fifth, octave, 3rd, 5th above
                    auto& b = bells[(size_t) nextBell];
                    nextBell = (nextBell + 1) % (int) bells.size();
                    const float f0 = clampf (famHz * pitches[(int) (rng.unipolar() * 4.99f)], 400.0f, 4000.0f);
                    static const float partial[3] = { 1.0f, 2.32f, 4.25f };              // glass-bell partials
                    static const float amp[3]     = { 1.0f, 0.45f, 0.25f };
                    for (size_t k = 0; k < 3; ++k)
                    {
                        const float f = clampf (f0 * partial[k] * (k == 0 ? 1.0f : 1.0f + 0.004f * rng.bipolar()), 300.0f, 15000.0f);
                        const float q = clampf (1.4f * pi * f / (1.0f + (float) k), 30.0f, 3000.0f);  // ~1.4 s ring, upper partials shorter
                        b.res[k].reset();
                        b.res[k].setBandpass (f, q, fs);
                        b.gain[k] = amp[k] * 2.0f * q / std::sin (twoPi * f / fs);
                    }
                    // a twin very slightly detuned, for the shimmer
                    b.twin.reset();
                    b.twin.setBandpass (f0 * 1.0035f, clampf (1.4f * pi * f0, 30.0f, 3000.0f), fs);
                    b.twinGain = 0.6f * 2.0f * clampf (1.4f * pi * f0, 30.0f, 3000.0f) / std::sin (twoPi * f0 * 1.0035f / fs);
                    b.strike = hit;
                }
                float ring = 0.0f;
                for (auto& b : bells)
                {
                    const float in = b.strike; b.strike = 0.0f;
                    for (size_t k = 0; k < 3; ++k) ring += b.gain[k] * b.res[k].tick (in);
                    ring += b.twinGain * b.twin.tick (in);
                }
                const float air = famHp.tick (famLp.tick (rng.bipolar() * std::fabs (x))) * e;  // a breath of stardust
                return 0.45f * ring + 0.8f * air;
            }
            default: // bees-nest — a buzzing swarm: the strings rattling fast and light
            {
                const float driven = std::tanh (x * 5.0f);
                const float rattle = rng.bipolar() * std::fabs (x) * 0.8f;
                return 13.0f * e * famHp.tick (famLp.tick (0.4f * driven + rattle));
            }
        }
    }

    /** what the handler does to the wheel at a given point in the revolution */
    struct HandlerOut
    {
        float speed = 1.0f;      // bow-speed multiplier  -> loudness / "drive"
        float direction = 1.0f;  // -1..1                  -> stalls and reversals
        float pressure = 0.0f;   // added bow pressure     -> airy (-) vs. raspy/growly (+)
        float noise = 1.0f;      // wheel-noise multiplier -> breath / grit
        float growl = 0.0f;      // 0..1 string saturation -> snarl on hard strokes
        bool  kick = false;      // a hard shove that also kicks the familiar
    };

    /** handler = who turns the crank. Each one shapes speed, pressure, noise and direction differently. */
    HandlerOut handlerMod (int h, float p)
    {
        HandlerOut o;
        switch (h)
        {
            case 0: // ghost — airy, breathy, tidal swells; sometimes nearly fades out
            {
                ghostLfo += 0.11f / fs; if (ghostLfo >= 1.0f) ghostLfo -= 1.0f;
                const float tide = 0.62f + 0.38f * std::sin (twoPi * ghostLfo);       // ~9 s tide
                const float swell = 1.0f + 0.4f * std::sin (twoPi * p);               // once per revolution
                if (p < lastPhase) ghostFade = rng.unipolar() < 0.25f ? 0.25f : 1.0f; // occasional vanishing turn
                ghostFadeSm += (ghostFade - ghostFadeSm) * smooth30 * 0.05f;
                o.speed = tide * swell * ghostFadeSm;
                o.pressure = -0.28f;   // light bow: thin, flautando, whispery
                o.noise = 4.0f;        // breath
                break;
            }
            case 1: // devil — two violent shoves per revolution (the "coup de poignet"), growling on each
            {
                const float q = std::fmod (p * 2.0f, 1.0f);
                const float shove = std::exp (-q * 9.0f);
                o.speed = 0.45f + 2.0f * shove;
                o.pressure = 0.38f * shove - 0.05f;
                o.noise = 1.0f + 2.0f * shove;
                o.growl = shove;
                o.kick = q < lastDevilQ;
                lastDevilQ = q;
                break;
            }
            case 2: // seer — calm, even and clean: the reference drone
                o.speed = 1.0f + 0.02f * std::sin (twoPi * p);
                o.noise = 0.3f;
                break;
            default: // witch — rocks the crank back and forth
            {
                const float sv = std::sin (twoPi * p);
                o.direction = std::copysign (std::sqrt (std::fabs (sv)), sv);
                o.speed = 1.1f;
                o.pressure = 0.1f;
                break;
            }
        }
        lastPhase = p;
        return o;
    }

    void slam (float velocity)
    {
        slamOpen = lidSm;
        coverAtSlam = st.coverOpen;
        slammed = true;
        slamEvent = true;
        const float a = velocity * (0.4f + 0.6f * slamOpen);
        thumpEnv = 0.9f * a; thumpNoiseEnv = 0.5f * a;
        thumpFreq = 110.0f; thumpPhase = 0.0f;
        thumpDecay = std::exp (-1.0f / (0.12f * fs));
        thumpNoiseDecay = std::exp (-1.0f / (0.025f * fs));
        slamFlashV = 1.0f;
    }

    void removeHeld (int note)
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[(size_t) i] == note)
            {
                for (int j = i; j < heldCount - 1; ++j) held[(size_t) j] = held[(size_t) j + 1];
                --heldCount;
                return;
            }
    }

    float fs = 48000.0f;
    Settings st;
    std::array<BowedString, NumSlots> strings;
    SympatheticString sympathetic;
    BodyResonator body, fxBody;
    Biquad padFilter, creakRes;
    std::array<Biquad, 3> eq;
    OnePoleLP noiseLp, jitterLp, lidLp, famLp, thumpNoiseLp;
    OnePoleHP noiseHp, famHp;
    Rng rng { 0xC0FFEEu };

    std::array<int, 16> held {};
    int heldCount = 0;
    float lastVelocity = 0.8f, mod = 0.0f, bend = 0.0f;

    float rpm = 0, rpmAttack = 0.01f, rpmRelease = 0.01f, smooth30 = 0.01f, smooth5 = 0.01f;
    float dirSm = 1.0f, handlerPhase = 0, crankAngle = 0, ghostLfo = 0;
    float expr = 0.0f, startBoost = 0.0f, startBoostDecay = 0.999f;
    bool lastGate = false;
    static constexpr float slotBeta[NumSlots] = { 0.127f, 0.141f, 0.118f, 0.133f, 0.11f, 0.122f, 0.13f };
    std::array<Biquad, 2> matColor;
    OnePoleLP roughLp;
    std::array<OnePoleLP, NumSlots> jitter;
    float vibPhase = 0;
    // familiar voices
    float hitAcc = 0, famVoiceTail = 0;
    Biquad crackleRes, chimeA, chimeB;
    float chimeGainA = 0, chimeGainB = 0;
    OnePoleLP popLp, subLp;
    float popThump = 0, thumpLike = 0.999f;
    int crushHold = 4, crushCount = 0; float crushHeld = 0, crushBits = 5, glitch = 0;
    float prevLowX = 0, subSign = 1.0f;
    float waveTarget = 0, waveEnv = 0, waveRise = 0.001f, waveFall = 0.9999f, bubbleKick = 0; int waveCtl = 0;
    Biquad waterLp, bubble;
    struct Bell { std::array<Biquad, 3> res; std::array<float, 3> gain {}; Biquad twin; float twinGain = 0, strike = 0; };
    std::array<Bell, 4> bells; int nextBell = 0;
    float lastPhase = 0, lastDevilQ = 0, ghostFade = 1.0f, ghostFadeSm = 1.0f;
    float shValue = 0, shCounter = 0;
    float famEnv = 0, famDecay = 0.999f;
    int lastStep = -1;
    float padHeat = 0, lidSm = 1.0f;

    float prevCover = 1.0f, creakTarget = 0, creakAmt = 0, creakPhase = 0;
    bool slammed = false, slamEvent = false;
    float coverAtSlam = 1.0f, slamOpen = 1.0f, slamFlashV = 0;
    float thumpEnv = 0, thumpNoiseEnv = 0, thumpFreq = 110, thumpPhase = 0, thumpDecay = 0.999f, thumpNoiseDecay = 0.99f;

    int lastBody = -1, lastPads = -1, lastStrings = -1, lastFamiliar = -1;
    float lastEq[3] {};
    bool eqDirty = true;
};
} // namespace hg
