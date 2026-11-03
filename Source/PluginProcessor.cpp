#include "PluginProcessor.h"
#include "PluginEditor.h"

HyrdiGyrdiProcessor::HyrdiGyrdiProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "hyrdiGYRDI", hg::createParameterLayout())
{
    auto bind = [this] (P& p, const char* id)
    {
        p.v = apvts.getRawParameterValue (id);
        jassert (p.v != nullptr);
    };
    using namespace hg::ids;
    bind (body, hg::ids::body); bind (bodyAmt, hg::ids::bodyAmt); bind (build, hg::ids::build);
    bind (handler, hg::ids::handler); bind (handlerAmt, hg::ids::handlerAmt);
    bind (speed, hg::ids::speed); bind (mult, hg::ids::mult); bind (direction, hg::ids::direction);
    bind (sync, hg::ids::sync); bind (crankMode, hg::ids::crankMode);
    bind (modSpeed, hg::ids::modSpeed); bind (bendDir, hg::ids::bendDir);
    bind (wheel, hg::ids::wheel); bind (wheelAmt, hg::ids::wheelAmt);
    bind (pads, hg::ids::pads); bind (padsAmt, hg::ids::padsAmt);
    bind (strings, hg::ids::strings); bind (familiar, hg::ids::familiar); bind (exciter, hg::ids::exciter);
    bind (keyBase, hg::ids::keyBase); bind (transpose, hg::ids::transpose); bind (tunerKey, hg::ids::tunerKey);
    bind (droneOct, hg::ids::droneOct); bind (detune, hg::ids::detune);
    bind (mixMel, hg::ids::mixMel); bind (mixFam, hg::ids::mixFam); bind (mixBass, hg::ids::mixBass); bind (mixSym, hg::ids::mixSym);
    bind (eqLow, hg::ids::eqLow); bind (eqMid, hg::ids::eqMid); bind (eqHigh, hg::ids::eqHigh);
    bind (coverOpen, hg::ids::coverOpen); bind (coverVol, hg::ids::coverVol); bind (master, hg::ids::master);
    for (int i = 0; i < 7; ++i) bind (stringOn[i], hg::ids::stringOn[i]);
}

bool HyrdiGyrdiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void HyrdiGyrdiProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    slamReverb.setSampleRate (sampleRate);
    slamReverb.reset();
    fxBuffer.setSize (2, samplesPerBlock);
    keyboardState.reset();
}

hg::Settings HyrdiGyrdiProcessor::readSettings() const
{
    hg::Settings s;
    s.body = (int) body.get();           s.bodyAmt = bodyAmt.get();     s.build = (int) build.get();
    s.handler = (int) handler.get();     s.handlerAmt = handlerAmt.get();
    s.speed = speed.get();               s.mult = mult.get();           s.direction = direction.get();
    s.sync = sync.get() > 0.5f;          s.crankMode = (int) crankMode.get();
    s.modToSpeed = modSpeed.get() > 0.5f; s.bendToDirection = bendDir.get() > 0.5f;
    s.wheel = (int) wheel.get();         s.wheelAmt = wheelAmt.get();
    s.pads = (int) pads.get();           s.padsAmt = padsAmt.get();
    s.strings = (int) strings.get();     s.familiar = (int) familiar.get(); s.exciter = exciter.get();
    for (int i = 0; i < 7; ++i) s.stringOn[i] = stringOn[i].get() > 0.5f;
    s.keyBase = (int) keyBase.get();     s.transpose = (int) transpose.get();
    s.tunerKey = (int) tunerKey.get();   s.droneOctave = (int) droneOct.get(); s.detuneCents = detune.get();
    s.mixMelody = mixMel.get(); s.mixFamiliar = mixFam.get(); s.mixBass = mixBass.get(); s.mixSympathy = mixSym.get();
    s.eqLow = eqLow.get(); s.eqMid = eqMid.get(); s.eqHigh = eqHigh.get();
    s.coverOpen = coverOpen.get(); s.coverVolume = coverVol.get(); s.masterDb = master.get();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm()) { s.hostBpm = *bpm; s.hostBpmValid = *bpm > 1.0; }
    return s;
}

void HyrdiGyrdiProcessor::handleMidi (const juce::MidiMessage& m, const hg::Settings& s)
{
    if (m.isNoteOn())                    engine.noteOn (m.getNoteNumber(), m.getFloatVelocity(), s);
    else if (m.isNoteOff())              engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
    else if (m.isController() && m.getControllerNumber() == 1) engine.modWheel ((float) m.getControllerValue() / 127.0f);
    else if (m.isPitchWheel())           engine.pitchBend (((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f);
}

void HyrdiGyrdiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    if (fxBuffer.getNumSamples() < numSamples) fxBuffer.setSize (2, numSamples, false, false, true);
    fxBuffer.clear();

    const auto s = readSettings();
    engine.setSettings (s, numSamples);

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : fxBuffer.getWritePointer (1); // mono: R is scratch
    float* fx = fxBuffer.getWritePointer (0);

    // sample-accurate MIDI: render up to each event, then apply it
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        if (at > pos) { engine.render (L, R, fx, pos, at - pos); pos = at; }
        handleMidi (meta.getMessage(), s);
    }
    if (pos < numSamples) engine.render (L, R, fx, pos, numSamples - pos);

    // the slam reverb is shaped by how open the cover was before it closed
    if (engine.consumeSlamEvent())
    {
        const float open = engine.lastSlamOpenness();
        juce::Reverb::Parameters p;
        p.roomSize = 0.3f + 0.65f * open;
        p.damping  = 0.5f - 0.3f * open;
        p.wetLevel = 0.15f + 0.6f * open;
        p.dryLevel = 0.8f;
        p.width    = 1.0f;
        slamReverb.setParameters (p);
    }
    float* fxR = fxBuffer.getWritePointer (1);
    juce::FloatVectorOperations::copy (fxR, fx, numSamples); // (in mono, this also discards the scratch R)
    slamReverb.processStereo (fx, fxR, numSamples);

    buffer.addFrom (0, 0, fx, numSamples);
    if (buffer.getNumChannels() > 1) buffer.addFrom (1, 0, fxR, numSamples);
}

juce::AudioProcessorEditor* HyrdiGyrdiProcessor::createEditor() { return new HyrdiGyrdiEditor (*this); }

void HyrdiGyrdiProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void HyrdiGyrdiProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new HyrdiGyrdiProcessor(); }
