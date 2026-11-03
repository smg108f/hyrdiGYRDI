#include "Params.h"

namespace hg
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto choice = [&] (const char* id, const char* name, const StringArray& opts, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, opts, def)); };

    auto amount = [&] (const char* id, const char* name, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 1.0f), def,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; })));
    };
    auto toggle = [&] (const char* id, const char* name, bool def)
    { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto integer = [&] (const char* id, const char* name, int lo, int hi, int def)
    { layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto decibels = [&] (const char* id, const char* name, float lo, float hi, float def)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), def,
            AudioParameterFloatAttributes().withLabel ("dB")));
    };

    // body
    choice (ids::body,  "Body",        names::body,  0);
    amount (ids::bodyAmt, "Body Resonance", 0.6f);
    choice (ids::build, "Body Build",  names::build, 0);
    choice (ids::decor, "Decoration",  names::decor, 0);

    // handler + grind
    choice (ids::handler, "Handler", names::handler, 2);
    amount (ids::handlerAmt, "Handler Influence", 0.7f);
    amount (ids::speed, "Grind Speed", 0.6f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::mult, 1 }, "Grind RPM Multiplier",
        NormalisableRange<float> (0.25f, 4.0f, 0.0f, 0.5f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + "x"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::direction, 1 }, "Grind Direction",
        NormalisableRange<float> (-1.0f, 1.0f), 1.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            if (std::abs (v) < 0.02f) return String ("stall");
            return (v > 0 ? String ("push ") : String ("pull ")) + String (roundToInt (std::abs (v) * 100.0f)) + "%";
        })));
    toggle (ids::sync, "Sync Crank to Tempo", false);
    choice (ids::crankMode, "Crank Mode", names::crankMode, 0);
    toggle (ids::modSpeed, "Mod Wheel -> Speed", true);
    toggle (ids::bendDir, "Pitch Bend -> Direction", true);

    // wheel / pads / strings / familiar
    choice (ids::wheel, "Wheel", names::wheel, 0);
    amount (ids::wheelAmt, "Wheel Character", 0.7f);
    choice (ids::pads, "Pads", names::pads, 0);
    amount (ids::padsAmt, "Pads Character", 0.5f);
    choice (ids::strings, "Strings", names::strings, 0);
    for (int i = 0; i < 7; ++i)
        toggle (ids::stringOn[i], (names::stringSlot[i] + " On").toRawUTF8(), true);
    choice (ids::familiar, "Familiar", names::familiar, 0);
    amount (ids::exciter, "Familiar Exciter", 0.5f);

    // keys + tuners
    integer (ids::keyBase, "Lowest Key (MIDI)", 0, 103, 48);
    integer (ids::transpose, "Melody Transpose", -24, 24, 0);
    choice (ids::tunerKey, "Tuned Key", names::keys, 7);
    integer (ids::droneOct, "Drone Octave", -2, 2, 0);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ids::detune, 1 }, "String Detune",
        NormalisableRange<float> (0.0f, 50.0f, 0.1f), 6.0f, AudioParameterFloatAttributes().withLabel ("ct")));

    // mixer
    amount (ids::mixMel, "Melody Volume", 0.8f);
    amount (ids::mixFam, "Familiar Volume", 0.55f);
    amount (ids::mixBass, "Bass Volume", 0.6f);
    amount (ids::mixSym, "Sympathy Volume", 0.4f);

    // EQ
    decibels (ids::eqLow,  "EQ Low",  -12.0f, 12.0f, 0.0f);
    decibels (ids::eqMid,  "EQ Mid",  -12.0f, 12.0f, 0.0f);
    decibels (ids::eqHigh, "EQ High", -12.0f, 12.0f, 0.0f);

    // cover + output
    amount (ids::coverOpen, "Cover Open", 1.0f);
    amount (ids::coverVol, "Cover Sound Volume", 0.5f);
    decibels (ids::master, "Master", -36.0f, 6.0f, 0.0f);

    return layout;
}
} // namespace hg
