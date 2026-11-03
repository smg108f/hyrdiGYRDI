// Offline render test: plays a short tune through the engine for every handler /
// wheel / familiar / body combination, checks for NaNs + level, and writes WAVs.
//   usage: hyrdiRender <output-dir>
#include <juce_audio_formats/juce_audio_formats.h>
#include "dsp/Engine.h"
#include <cstdio>

namespace
{
constexpr double sr = 48000.0;
constexpr int block = 256;

struct Stats { float peak = 0, rms = 0; bool finite = true; };

Stats render (hg::Settings s, double seconds, const juce::File& wav, bool slamAtEnd = false)
{
    hg::Engine e;
    e.prepare (sr);
    const int total = (int) (seconds * sr);
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    std::vector<float> fx ((size_t) total, 0.0f);

    // a little modal tune over the drones (relative to keyBase), 0.5 s per note
    const int tune[] = { 8, 10, 12, 10, 8, 7, 5, 7, 8, 8, 12, 15, 13, 12, 10, 8 };
    int current = -1;
    double sumSq = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        const double t = pos / sr;
        const int idx = (int) (t / 0.5);
        const bool playing = t < seconds - 1.2;
        const int want = playing ? s.keyBase + tune[idx % 16] : -1;
        if (want != current)
        {
            if (current >= 0) e.noteOff (current);
            if (want >= 0) e.noteOn (want, 0.8f, s);
            current = want;
        }
        if (slamAtEnd && std::abs (t - (seconds - 0.9)) < block / sr) e.noteOn (s.keyBase + 24, 1.0f, s);
        e.setSettings (s, n);
        e.render (out.getWritePointer (0), out.getWritePointer (1), fx.data(), pos, n);
    }
    Stats st;
    for (int i = 0; i < total; ++i)
    {
        const float v = out.getSample (0, i) + fx[(size_t) i];
        out.setSample (0, i, v); out.setSample (1, i, v);
        if (! std::isfinite (v)) st.finite = false;
        st.peak = std::max (st.peak, std::abs (v));
        sumSq += (double) v * v;
    }
    st.rms = (float) std::sqrt (sumSq / total);

    wav.deleteFile();
    juce::WavAudioFormat fmt;
    if (auto os = wav.createOutputStream())
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (fmt.createWriterFor (os.get(), sr, 2, 24, {}, 0)))
        {
            os.release();
            w->writeFromAudioSampleBuffer (out, 0, total);
        }
    return st;
}
} // namespace

int main (int argc, char** argv)
{
    const juce::File dir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    dir.createDirectory();
    int failures = 0;
    auto check = [&] (const juce::String& name, const Stats& st)
    {
        const bool ok = st.finite && st.peak < 1.6f && st.rms > 0.005f;
        std::printf ("%-44s peak %.3f  rms %.4f  %s\n", name.toRawUTF8(), st.peak, st.rms, ok ? "ok" : "FAIL");
        if (! ok) ++failures;
    };

    hg::Settings s;
    check ("default", render (s, 8.5, dir.getChildFile ("default.wav"), true));

    const char* handlers[] = { "ghost", "devil", "seer", "witch" };
    const char* wheels[]   = { "rosin", "blood", "metal", "chalk" };
    const char* fams[]     = { "shiba", "gremlin", "troll", "fairy" };
    const char* bodies[]   = { "woods", "metals", "organics", "plastics" };
    for (int h = 0; h < 4; ++h)
    {
        auto v = s; v.handler = h; v.familiar = h; v.exciter = 0.9f;
        check (juce::String ("handler-") + handlers[h] + "+" + fams[h], render (v, 4.0, dir.getChildFile (juce::String ("handler_") + handlers[h] + ".wav")));
    }
    for (int w = 0; w < 4; ++w)
    {
        auto v = s; v.wheel = w; v.pads = w; v.strings = w; v.body = w;
        check (juce::String ("wheel-") + wheels[w] + "/" + bodies[w], render (v, 4.0, dir.getChildFile (juce::String ("material_") + wheels[w] + ".wav")));
    }
    for (int b = 0; b < 4; ++b)
    {
        auto v = s; v.build = b; v.speed = 1.0f; v.mult = 3.0f; v.eqHigh = 12; v.eqLow = 12; v.masterDb = 6;
        check (juce::String ("build ") + juce::String (b) + " (loud extreme)", render (v, 3.0, dir.getChildFile ("build_" + juce::String (b) + ".wav")));
    }
    { auto v = s; v.masterDb = -12.0f; check ("master -12 dB (headroom check)", render (v, 3.0, dir.getChildFile ("master_m12.wav"))); }
    { auto v = s; v.coverOpen = 0.0f; check ("cover closed", render (v, 3.0, dir.getChildFile ("cover_closed.wav"))); }
    { auto v = s; v.direction = -1.0f; check ("reverse crank", render (v, 3.0, dir.getChildFile ("reverse.wav"))); }
    { auto v = s; v.sync = true; v.hostBpmValid = true; v.hostBpm = 96; check ("tempo sync 96bpm", render (v, 4.0, dir.getChildFile ("sync96.wav"))); }

    { // tuning check: only Melody 1, first tune note = keyBase + 8 = A4 (440 Hz)
        auto v = s; for (auto& o : v.stringOn) o = false; v.stringOn[hg::Mel1] = true; v.keyBase = 61; v.bodyAmt = 0; v.padsAmt = 0;
        check ("tuning: melody only, A4 first", render (v, 2.0, dir.getChildFile ("tuning_a4.wav")));
    }

    std::printf ("%s\n", failures == 0 ? "ALL OK" : "FAILURES");
    return failures == 0 ? 0 : 1;
}
