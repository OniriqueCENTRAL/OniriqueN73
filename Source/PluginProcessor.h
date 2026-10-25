#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "N73Engine.h"
#include "Params.h"
#include "Presets.h"
#if N73_LICENCE
 #include "License/LicenseGuard.h"
#endif

class N73Processor : public juce::AudioProcessor,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::AsyncUpdater
{
public:
    N73Processor();
    ~N73Processor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    // le bypass du DAW pilote le paramètre Bypass (IN de la barre du haut) : fondu propre, latence alignée
    juce::AudioProcessorParameter* getBypassParameter() const override { return apvts.getParameter (params::bypass); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ONIRIQUE N73"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    /** dernière position du gain dans la plage MIC ou LINE : clic sur MIC ou LINE, comme sur le plugin UAD. */
    int lastGainIndex (bool mic) const { return mic ? lastMicIndex.load() : lastLineIndex.load(); }
    int getLatency() const { return getLatencySamples(); }
    double getWorkingRate() const { return engine.workingRate (apvts.getRawParameterValue (params::quality)->load() > 0.5f); }

    juce::AudioProcessorValueTreeState apvts;
    /** Les presets d'usine et ceux de l'utilisateur, la comparaison A/B : ce que montre la barre du haut. */
    onirique::PresetBank presets { apvts, "N73", n73presets::stockPresets(), n73presets::tagNames(), params::notStored() };
#if N73_LICENCE
    onirique::license::Guard license { "onirique-n73" };
#endif

private:
    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void process (juce::AudioBuffer<float>&, bool bypassed);

    params::Reader reader { apvts };
    n73::Engine engine;
    std::atomic<int> lastMicIndex { n73::defaultMicIndex }, lastLineIndex { n73::defaultLineIndex };
    void rememberGain (int index);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (N73Processor)
};
