#include "PluginProcessor.h"
#include "PluginEditor.h"

N73Processor::N73Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "N73", params::layout())
{
    apvts.addParameterListener (params::quality, this);
    slots[0] = slots[1] = presets.capture();
}

N73Processor::~N73Processor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener (params::quality, this);
}

bool N73Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void N73Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
#if N73_LICENCE
    license.refresh();
#endif
    engine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, getTotalNumOutputChannels()));
    setLatencySamples (engine.latencySamples (apvts.getRawParameterValue (params::quality)->load() > 0.5f));
}

void N73Processor::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }

void N73Processor::handleAsyncUpdate()
{
    // la qualité change la latence : l'hôte est prévenu depuis le thread de l'interface
    const int lat = engine.latencySamples (apvts.getRawParameterValue (params::quality)->load() > 0.5f);
    if (lat != getLatencySamples()) setLatencySamples (lat);
}

void N73Processor::process (juce::AudioBuffer<float>& buffer, bool bypassed)
{
    juce::ScopedNoDenormals noDenormals;
    // un hôte envoie parfois des NaN / inf : ils ne doivent jamais entrer dans les mémoires des filtres
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        auto* s = buffer.getWritePointer (c);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (s[i])) s[i] = 0.0f;
    }
    auto p = reader.read();
    if (bypassed) p.power = false;
    engine.process (buffer, p);
#if N73_LICENCE
    license.process (buffer, getSampleRate(), isNonRealtime());
#endif
}

void N73Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process (buffer, false); }
void N73Processor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) { process (buffer, true); }

void N73Processor::selectSlot (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    if (slot == activeSlot) return;
    slots[activeSlot] = presets.capture();
    activeSlot = slot;
    presets.apply (slots[activeSlot]);
}

void N73Processor::copyActiveToOther()
{
    slots[activeSlot] = presets.capture();
    slots[1 - activeSlot] = slots[activeSlot];
}

void N73Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    slots[activeSlot] = presets.capture();
    state.setProperty ("activeSlot", activeSlot, nullptr);
    for (int s = 0; s < 2; ++s)
    {
        juce::ValueTree t ("Slot" + juce::String (s));
        for (const auto& [id, v] : slots[s]) t.setProperty (id, v, nullptr);
        state.removeChild (state.getChildWithName (t.getType()), nullptr);
        state.appendChild (t, nullptr);
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void N73Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    for (int s = 0; s < 2; ++s)
    {
        const auto t = state.getChildWithName ("Slot" + juce::String (s));
        slots[s].clear();
        for (int i = 0; i < t.getNumProperties(); ++i)
            slots[s][t.getPropertyName (i).toString()] = (float) t.getProperty (t.getPropertyName (i));
    }
    activeSlot = juce::jlimit (0, 1, (int) state.getProperty ("activeSlot", 0));
    apvts.replaceState (state);
    if (slots[0].empty()) slots[0] = presets.capture();
    if (slots[1].empty()) slots[1] = presets.capture();
    triggerAsyncUpdate();
}

juce::AudioProcessorEditor* N73Processor::createEditor() { return new N73Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new N73Processor(); }
