// hÿrdiGYRDI — small DSP building blocks (no JUCE dependency)
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <algorithm>

namespace hg
{
constexpr float pi    = 3.14159265358979f;
constexpr float twoPi = 6.28318530717959f;

inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }
inline float lerpf (float a, float b, float t)     { return a + (b - a) * t; }
inline float mtof (float note)                      { return 440.0f * std::pow (2.0f, (note - 69.0f) / 12.0f); }
inline float dbToGain (float db)                    { return std::pow (10.0f, db / 20.0f); }
inline float smoothstep (float e0, float e1, float x)
{
    const float t = clampf ((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
/** coefficient for a one-pole smoother reaching ~63% in `ms` milliseconds */
inline float msToCoef (float ms, float sr) { return 1.0f - std::exp (-1.0f / (0.001f * ms * sr)); }

//==============================================================================
struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 0x9E3779B9u) : s (seed ? seed : 1u) {}
    /** uniform in [-1, 1) */
    float bipolar()  { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) s * (2.0f / 4294967296.0f) - 1.0f; }
    /** uniform in [0, 1) */
    float unipolar() { return 0.5f * (bipolar() + 1.0f); }
};

//==============================================================================
struct OnePoleLP
{
    float a = 0.0f, y = 0.0f;
    void setCutoff (float hz, float sr) { a = std::exp (-twoPi * clampf (hz, 1.0f, 0.49f * sr) / sr); }
    float tick (float x) { y = (1.0f - a) * x + a * y; return y; }
    void reset() { y = 0.0f; }
};

struct OnePoleHP
{
    OnePoleLP lp;
    void setCutoff (float hz, float sr) { lp.setCutoff (hz, sr); }
    float tick (float x) { return x - lp.tick (x); }
    void reset() { lp.reset(); }
};

//==============================================================================
/** RBJ-cookbook biquad, transposed direct form II */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    float tick (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() { z1 = z2 = 0.0f; }

    void setBandpass (float f, float q, float sr)
    {
        const float w = twoPi * clampf (f, 10.0f, 0.45f * sr) / sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        set (al, 0.0f, -al, 1.0f + al, -2.0f * c, 1.0f - al);
    }
    void setLowpass (float f, float q, float sr)
    {
        const float w = twoPi * clampf (f, 10.0f, 0.45f * sr) / sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        set ((1.0f - c) * 0.5f, 1.0f - c, (1.0f - c) * 0.5f, 1.0f + al, -2.0f * c, 1.0f - al);
    }
    void setPeak (float f, float q, float gainDb, float sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = twoPi * clampf (f, 10.0f, 0.45f * sr) / sr, c = std::cos (w), al = std::sin (w) / (2.0f * q);
        set (1.0f + al * A, -2.0f * c, 1.0f - al * A, 1.0f + al / A, -2.0f * c, 1.0f - al / A);
    }
    void setLowShelf (float f, float gainDb, float sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = twoPi * clampf (f, 10.0f, 0.45f * sr) / sr, c = std::cos (w);
        const float al = std::sin (w) * 0.5f * std::sqrt (2.0f), sA = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) - (A - 1) * c + sA), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - sA),
             (A + 1) + (A - 1) * c + sA, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - sA);
    }
    void setHighShelf (float f, float gainDb, float sr)
    {
        const float A = std::pow (10.0f, gainDb / 40.0f);
        const float w = twoPi * clampf (f, 10.0f, 0.45f * sr) / sr, c = std::cos (w);
        const float al = std::sin (w) * 0.5f * std::sqrt (2.0f), sA = 2.0f * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sA), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sA),
             (A + 1) - (A - 1) * c + sA, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sA);
    }

private:
    void set (float nb0, float nb1, float nb2, float na0, float na1, float na2)
    {
        b0 = nb0 / na0; b1 = nb1 / na0; b2 = nb2 / na0; a1 = na1 / na0; a2 = na2 / na0;
    }
};

//==============================================================================
/** ring-buffer delay with linear-interpolated fractional read */
struct FracDelay
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    float delay = 1.0f, last = 0.0f;

    void prepare (int maxSamples)
    {
        int size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0; last = 0.0f;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); last = 0.0f; }
    void setDelay (float d) { delay = clampf (d, 1.0f, (float) mask - 2.0f); }

    float tick (float in)
    {
        buf[(size_t) w] = in;
        const float rp = (float) w - delay;
        const int   i0 = (int) std::floor (rp);
        const float fr = rp - (float) i0;
        const float s0 = buf[(size_t) (i0 & mask)], s1 = buf[(size_t) ((i0 + 1) & mask)];
        last = s0 + fr * (s1 - s0);
        w = (w + 1) & mask;
        return last;
    }
};

//==============================================================================
/**
    Bowed string — a digital waveguide in the style of Perry Cook / STK's Bowed:
    two delay lines (nut side + bridge side) meeting at the bow contact point,
    with a stick-slip friction table. On a hurdy-gurdy the "bow" is the rosined
    wheel, so bow velocity == wheel surface speed.
*/
struct BowedString
{
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        neck.prepare ((int) (fs / 20.0f) + 8);
        bridge.prepare ((int) (fs / 20.0f) + 8);
        glide = msToCoef (6.0f, fs);
        setMaterial (0.70f, 0.95f);
        reset();
    }
    void reset() { neck.clear(); bridge.clear(); loopY = 0.0f; curF = targetF; }

    void setFrequency (float hz, bool immediate = false)
    {
        targetF = clampf (hz, 20.0f, 0.2f * fs);
        if (immediate) curF = targetF;
    }
    /** pole is specified at 44.1 kHz and converted so the loop brightness is rate-independent */
    void setMaterial (float pole44k, float loopGain)
    {
        pole = std::pow (clampf (pole44k, 0.0f, 0.99f), 44100.0f / fs);
        gain = loopGain;
    }
    void setBowPosition (float b) { beta = clampf (b, 0.05f, 0.4f); }

    float tick (float bowVelocity, float pressure)
    {
        curF += (targetF - curF) * glide;
        const float base = std::max (4.0f, fs / curF - 4.0f);
        bridge.setDelay (base * beta);
        neck.setDelay (base * (1.0f - beta));

        loopY = gain * (1.0f - pole) * bridge.last + pole * loopY;
        const float bridgeRefl = -loopY;
        const float nutRefl    = -neck.last;
        const float stringVel  = bridgeRefl + nutRefl;
        const float dv         = bowVelocity - stringVel;

        const float slope = 5.0f - 4.0f * clampf (pressure, 0.0f, 1.0f);
        float t = std::fabs (dv * slope) + 0.75f;
        t = 1.0f / (t * t * t * t);
        t = clampf (t, 0.01f, 1.0f);
        const float nv = dv * t;

        neck.tick (bridgeRefl + nv);
        bridge.tick (nutRefl + nv);
        return bridge.last;
    }

    float fs = 48000.0f, targetF = 220.0f, curF = 220.0f, glide = 0.01f;
    float pole = 0.65f, gain = 0.95f, beta = 0.127f, loopY = 0.0f;
    FracDelay neck, bridge;
};

//==============================================================================
/** feedback comb used for the un-bowed sympathetic string */
struct SympatheticString
{
    void prepare (double sampleRate) { fs = (float) sampleRate; d.prepare ((int) (fs / 30.0f) + 8); lp.setCutoff (5000.0f, fs); }
    void setFrequency (float hz) { d.setDelay (fs / clampf (hz, 30.0f, 4000.0f) - 0.5f); }
    void reset() { d.clear(); lp.reset(); }
    float tick (float x)
    {
        const float y = x + 0.993f * lp.tick (d.last);
        d.tick (y);
        return y;
    }
    float fs = 48000.0f;
    FracDelay d;
    OnePoleLP lp;
};

//==============================================================================
/** modal "body" resonator: a few band-pass modes summed over the dry signal */
struct BodyResonator
{
    struct Mode { float f, q, g; };

    void prepare (double sampleRate) { fs = (float) sampleRate; configure (type); }

    void configure (int bodyType)
    {
        type = bodyType;
        static const Mode tables[4][5] = {
            // woods — warm, moderately damped plate + air modes
            { { 275, 8, 1.0f }, { 460, 10, 0.8f }, { 980, 7, 0.6f }, { 2300, 5, 0.4f }, { 4100, 4, 0.25f } },
            // metals — high-Q, slightly inharmonic ringing
            { { 520, 40, 0.7f }, { 1310, 55, 0.6f }, { 2870, 60, 0.5f }, { 4650, 70, 0.4f }, { 7300, 50, 0.25f } },
            // organics — gourd/shell: low, hollow, heavily damped
            { { 170, 4, 1.0f }, { 380, 5, 0.8f }, { 820, 4, 0.5f }, { 1650, 3, 0.35f }, { 3300, 3, 0.2f } },
            // scrap-plastics — boxy mids, a bit of rattle (handled in tick)
            { { 340, 6, 0.7f }, { 900, 9, 0.7f }, { 2050, 12, 0.6f }, { 3500, 6, 0.4f }, { 5600, 5, 0.3f } },
        };
        type = std::clamp (bodyType, 0, 3);
        for (size_t i = 0; i < 5; ++i)
        {
            const auto& m = tables[type][i];
            modes[i].setBandpass (m.f, m.q, fs);
            gains[i] = m.g;
        }
    }
    void reset() { for (auto& m : modes) m.reset(); }

    float tick (float x, float amount)
    {
        float wet = 0.0f;
        for (size_t i = 0; i < 5; ++i)
            wet += gains[i] * modes[i].tick (x);
        if (type == 3) wet = std::tanh (wet * 2.0f) * 0.6f; // plastic rattle
        return x * (1.0f - 0.55f * amount) + amount * 1.6f * wet;
    }

    float fs = 48000.0f;
    int type = 0;
    std::array<Biquad, 5> modes;
    std::array<float, 5> gains {};
};
} // namespace hg
