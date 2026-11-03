#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Params.h"
#include "dsp/Engine.h"

class HyrdiGyrdiProcessor : public juce::AudioProcessor
{
public:
    HyrdiGyrdiProcessor();
    ~HyrdiGyrdiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // lets the on-screen keys play notes
    const hg::Telemetry& telemetry() const { return engine.telemetry; }

private:
    hg::Settings readSettings() const;
    void handleMidi (const juce::MidiMessage&, const hg::Settings&);

    hg::Engine engine;
    juce::Reverb slamReverb;
    juce::AudioBuffer<float> fxBuffer;

    struct P { std::atomic<float>* v = nullptr; float get() const { return v->load(); } };
    P body, bodyAmt, build, handler, handlerAmt, speed, mult, direction, sync, crankMode, modSpeed, bendDir,
      wheel, wheelAmt, pads, padsAmt, strings, familiar, exciter, keyBase, transpose, tunerKey, droneOct, detune,
      mixMel, mixFam, mixBass, mixSym, eqLow, eqMid, eqHigh, coverOpen, coverVol, master;
    P stringOn[7];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HyrdiGyrdiProcessor)
};
