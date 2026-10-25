#include "PluginProcessor.h"
#include "PluginEditor.h"

N73Processor::N73Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "N73", params::layout())
{
    apvts.addParameterListener (params::quality, this);
    apvts.addParameterListener (params::gain, this);
    rememberGain ((int) apvts.getRawParameterValue (params::gain)->load());
    apvts.state.setProperty ("preset", "INIT", nullptr);   // l'état de départ : les valeurs par défaut
    n73presets::importOldUserPresets (presets);
}

N73Processor::~N73Processor()
{
    cancelPendingUpdate();
    apvts.removeParameterListener (params::quality, this);
    apvts.removeParameterListener (params::gain, this);
}

void N73Processor::rememberGain (int index)
{
    index = juce::jlimit (0, n73::numGainPositions - 1, index);
    (n73::isLineIndex (index) ? lastLineIndex : lastMicIndex).store (index);
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

void N73Processor::parameterChanged (const juce::String& id, float v)
{
    if (id == params::gain) rememberGain ((int) std::lround (v));
    else triggerAsyncUpdate();
}

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

void N73Processor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    presets.writeState (state);   // le côté A/B écouté, et l'autre
    state.setProperty ("lastMic", lastMicIndex.load(), nullptr);
    state.setProperty ("lastLine", lastLineIndex.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void N73Processor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto state = juce::ValueTree::fromXml (*xml);
    // un projet de la 1.0 : ses mémoires A / B (Slot0, Slot1, activeSlot) passent dans la barre du haut, et le nom de
    // son preset suit
    if (state.hasProperty ("activeSlot") && ! state.hasProperty ("slot"))
    {
        const int active = juce::jlimit (0, 1, (int) state.getProperty ("activeSlot", 0));
        state.setProperty ("slot", active, nullptr);
        const auto other = state.getChildWithName ("Slot" + juce::String (1 - active));
        if (other.isValid() && other.getNumProperties() > 0)
        {
            juce::ValueTree t ("OtherSlot");
            t.copyPropertiesFrom (other, nullptr);
            t.setProperty ("preset", state.getProperty ("presetName", "INIT"), nullptr);
            state.appendChild (t, nullptr);
        }
    }
    for (auto* old : { "Slot0", "Slot1" }) state.removeChild (state.getChildWithName (old), nullptr);
    if (! state.hasProperty ("preset") && state.hasProperty ("presetName"))
        state.setProperty ("preset", state["presetName"], nullptr);
    presets.readState (state);
    apvts.replaceState (state);
    // après replaceState : la position restaurée vient de mettre à jour une des deux mémoires
    rememberGain ((int) state.getProperty ("lastMic", n73::defaultMicIndex));
    rememberGain ((int) state.getProperty ("lastLine", n73::defaultLineIndex));
    rememberGain ((int) apvts.getRawParameterValue (params::gain)->load());
    triggerAsyncUpdate();
}

juce::AudioProcessorEditor* N73Processor::createEditor() { return new N73Editor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new N73Processor(); }
