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
        rpmAttack  = msToCoef (70.0f, fs);
        rpmRelease = msToCoef (260.0f, fs);
        smooth30   = msToCoef (30.0f, fs);
        smooth5    = msToCoef (5.0f, fs);

        // bow positions differ slightly per slot for timbral variety
        const float betas[NumSlots] = { 0.127f, 0.141f, 0.118f, 0.133f, 0.11f, 0.122f, 0.13f };
        for (int i = 0; i < NumSlots; ++i) strings[(size_t) i].setBowPosition (betas[i]);

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
            // loop pole (brightness, at 44.1k) / loop gain (sustain) per string material
            static const float pole[4] = { 0.70f, 0.45f, 0.62f, 0.80f };   // gut, steel, nylon, fiber
            static const float gain[4] = { 0.95f, 0.985f, 0.965f, 0.93f };
            for (auto& str : strings) str.setMaterial (pole[s.strings], gain[s.strings]);
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
            float m = 1.0f, d = 1.0f;
            handlerMod (s.handler, handlerPhase, m, d);
            m = 1.0f + (m - 1.0f) * s.handlerAmt;
            d = 1.0f + (d - 1.0f) * s.handlerAmt;

            const float dir = dirSm * d;
            const float wheelSpeed = std::tanh (std::max (0.0f, rpm * m) / 90.0f);
            crankAngle += twoPi * (rpm * m / 60.0f) / fs * dir;
            if (crankAngle > twoPi) crankAngle -= twoPi; else if (crankAngle < 0) crankAngle += twoPi;

            // --- bow (wheel surface) ---------------------------------------
            const float noise = noiseLp.tick (rng.bipolar());
            float bowVel = 0.27f * wheelSpeed * intensity * dir * (0.65f + 0.35f * lastVelocity);
            if (jitAmt > 0) bowVel *= 1.0f + jitAmt * 6.0f * jitterLp.tick (rng.bipolar());
            if (s.wheel == 3) bowVel += noiseAmt * noiseHp.tick (rng.bipolar()) * wheelSpeed;
            else              bowVel += noiseAmt * noise * wheelSpeed;

            if (shAmt > 0 && (shCounter -= 1.0f) <= 0.0f) { shValue = rng.unipolar(); shCounter = fs / 45.0f; }
            const float pressure = clampf (basePressure + shAmt * shValue + 0.08f * lastVelocity, 0.05f, 0.98f);

            // --- strings -----------------------------------------------------
            float mel = 0, famS = 0, bass = 0;
            if (on[Mel1])  mel  += strings[Mel1].tick (bowVel, pressure);
            if (on[Mel2])  mel  += strings[Mel2].tick (bowVel * 0.97f, pressure);
            if (on[Fam1])  famS += strings[Fam1].tick (bowVel, pressure);
            if (on[Fam2])  famS += strings[Fam2].tick (bowVel * 0.95f, pressure);
            if (on[Bass1]) bass += strings[Bass1].tick (bowVel, pressure);
            if (on[Bass2]) bass += strings[Bass2].tick (bowVel * 0.95f, pressure);

            // --- the familiar (buzzing bridge / "chien") ------------------
            const int step = (int) (handlerPhase * (float) fam.steps) % fam.steps;
            if (step != lastStep)
            {
                float acc = fam.pattern[step];
                if (fam.chaos > 0 && rng.unipolar() < fam.chaos) acc = acc > 0 ? 0.0f : 0.6f * rng.unipolar();
                if (acc > 0) famEnv = std::max (famEnv, acc);
                lastStep = step;
            }
            famEnv *= famDecay;
            const float buzzGate = smoothstep (0.12f, 0.4f, wheelSpeed * intensity);
            const float e = famEnv * s.exciter * buzzGate;
            if (e > 1.0e-4f)
            {
                const float driven = std::tanh (famS * fam.drive);
                const float rattle = rng.bipolar() * std::fabs (famS) * fam.rattle * 4.0f;
                const float buzz = famHp.tick (famLp.tick (0.4f * driven + rattle));
                famS = famS * (1.0f - 0.3f * e) + 1.2f * e * buzz;
            }
            else { famHp.tick (famLp.tick (0.0f)); }

            // --- sympathetic (un-bowed, excited by the others) ------------
            float sym = 0;
            if (on[Sym]) sym = 0.06f * sympathetic.tick (0.05f * (mel + famS + 0.5f * bass));

            float x = s.mixMelody * mel + s.mixFamiliar * famS + s.mixBass * bass + s.mixSympathy * sym;

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
        static const FamiliarCfg cfg[4] = {
            // shiba-soul: friendly, steady, a little "bark" on the off-beat
            { 8,  { 1, 0, .55f, 0, .9f, 0, .55f, .3f },              7.0f, 0.25f, 3200.0f, 150.0f, 70.0f, 0.0f },
            // gremlin: syncopated, nasty, unpredictable
            { 8,  { 1, 0, 0, .8f, 0, .9f, .4f, 0 },                 22.0f, 0.65f, 7500.0f, 400.0f, 35.0f, 0.35f },
            // troll: one heavy stomp per revolution, low and long
            { 4,  { 1, 0, 0, .35f },                                12.0f, 0.5f, 1300.0f, 60.0f, 220.0f, 0.0f },
            // fairy: quick triplet sparkle, light and high
            { 12, { 1, .4f, .6f, 1, .4f, .6f, 1, .4f, .6f, 1, .6f, .8f }, 5.0f, 0.2f, 11000.0f, 1200.0f, 22.0f, 0.1f },
        };
        return cfg[std::clamp (type, 0, 3)];
    }

    /** handler = who turns the crank: modulates wheel speed (m) and direction (d) over each revolution */
    void handlerMod (int h, float p, float& m, float& d)
    {
        switch (h)
        {
            case 0: // ghost — drifting swells, never quite steady
                ghostLfo += 0.11f / fs; if (ghostLfo >= 1.0f) ghostLfo -= 1.0f;
                m = 1.0f + 0.28f * std::sin (twoPi * p) + 0.22f * std::sin (twoPi * ghostLfo);
                break;
            case 1: // devil — two hard pushes per revolution
            {
                const float q = std::fmod (p * 2.0f, 1.0f);
                m = 0.7f + 1.1f * std::exp (-q * 6.0f);
                break;
            }
            case 2: // seer — almost perfectly steady
                m = 1.0f + 0.02f * std::sin (twoPi * p);
                break;
            default: // witch — rocks the crank back and forth
            {
                const float sv = std::sin (twoPi * p);
                d = std::copysign (std::sqrt (std::fabs (sv)), sv);
                m = 1.1f;
                break;
            }
        }
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
