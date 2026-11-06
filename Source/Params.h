#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace hg::ids
{
// Parameter IDs are part of saved sessions/presets — don't rename them once released.
inline constexpr const char* body       = "body";
inline constexpr const char* bodyAmt    = "bodyAmount";
inline constexpr const char* build      = "build";
inline constexpr const char* decor      = "decoration";
inline constexpr const char* handler    = "handler";
inline constexpr const char* handlerAmt = "handlerAmount";
inline constexpr const char* speed      = "grindSpeed";
inline constexpr const char* mult       = "grindMultiplier";
inline constexpr const char* direction  = "grindDirection";
inline constexpr const char* sync       = "hostSync";
inline constexpr const char* crankMode  = "crankMode";
inline constexpr const char* modSpeed   = "modWheelToSpeed";
inline constexpr const char* bendDir    = "bendToDirection";
inline constexpr const char* wheel      = "wheel";
inline constexpr const char* wheelAmt   = "wheelAmount";
inline constexpr const char* pads       = "pads";
inline constexpr const char* padsAmt    = "padsAmount";
inline constexpr const char* strings    = "strings";
inline constexpr const char* familiar   = "familiar";
inline constexpr const char* exciter    = "exciter";
inline constexpr const char* keyBase    = "keyBase";
inline constexpr const char* transpose  = "melodyTranspose";
inline constexpr const char* tunerKey   = "tunerKey";
inline constexpr const char* droneOct   = "droneOctave";
inline constexpr const char* detune     = "detune";
inline constexpr const char* mixMel     = "mixMelody";
inline constexpr const char* mixFam     = "mixFamiliar";
inline constexpr const char* mixBass    = "mixBass";
inline constexpr const char* mixSym     = "mixSympathy";
inline constexpr const char* eqLow      = "eqLow";
inline constexpr const char* eqMid      = "eqMid";
inline constexpr const char* eqHigh     = "eqHigh";
inline constexpr const char* coverOpen  = "coverOpen";
inline constexpr const char* coverVol   = "coverVolume";
inline constexpr const char* master     = "master";
inline constexpr const char* stringOn[7] = { "stringMelody1", "stringMelody2", "stringFamiliar1", "stringFamiliar2",
                                             "stringBass1", "stringBass2", "stringSympathy" };
} // namespace hg::ids

namespace hg::names
{
inline const juce::StringArray body      { "Woods", "Metals", "Organics", "Scrap-Plastics" };
inline const juce::StringArray build     { "7 strings", "6 strings", "4 strings", "3 strings" };
inline const juce::StringArray decor     { "None", "Sigils", "Runes", "Stickers" };
inline const juce::StringArray handler   { "Ghost", "Devil", "Seer", "Witch" };
inline const juce::StringArray wheel     { "Rosin on Wood", "Blood on Stone", "Polished Metal", "Chalkboard" };
inline const juce::StringArray pads      { "Cotton", "Bone", "Glass", "Titanium" };
inline const juce::StringArray strings   { "Gut-String", "Steel-Wire", "Nylon-Cord", "Fiber-Light", "Hemp-Rope" }; // add new ones at the end
inline const juce::StringArray familiar  { "Shiba-Soul", "Gremlin", "Troll", "Fairy", "Bees-Nest" }; // add new ones at the end: saved sets store the index
inline const juce::StringArray crankMode { "While keys held", "Always turning" };
inline const juce::StringArray keys      { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
inline const juce::StringArray stringSlot { "Melody 1", "Melody 2", "Familiar 1", "Familiar 2", "Bass 1", "Bass 2", "Sympathy" };
} // namespace hg::names

namespace hg
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
