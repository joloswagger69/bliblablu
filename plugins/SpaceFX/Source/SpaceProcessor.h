#pragma once
#ifndef SPACE_WITH_GUI
#define SPACE_WITH_GUI 0
#endif
#include <nwa/PresetManager.h>
#include <nwa/Util.h>
#include "PluginInfo.h"
#include "FactoryPresets.h"
#include "dsp/Engine.h"
#include <atomic>

namespace spf
{
class SpaceProcessor : public juce::AudioProcessor
{
public:
    SpaceProcessor();
    ~SpaceProcessor() override = default;

    const juce::String getName() const override { return "SPACE//FX"; }
    void prepareToPlay(double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;
    bool hasEditor() const override { return SPACE_WITH_GUI != 0; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    float getRaw(const juce::String& id) const { return apvts.getRawParameterValue(id)->load(); }
    int   getInt(const juce::String& id) const { return (int) std::lround(getRaw(id)); }
    void  setParam(const juce::String& id, float plain);

    // Werkspresets / Randomizer (Message-Thread)
    int  numFactory() const { return (int) factoryPresets().size(); }
    void applyFactory(int index);                 // setzt zuerst alles auf Standard, dann die Werte des Presets
    void resetAllToDefault();
    void engineResetForTest() { engine.reset(); }      // loescht Echo/Hall-Reste (Tests, Preset-Wechsel ohne Altlasten)
    void randomize(unsigned seed);                // musikalische Grenzen, Freeze/Infinite bleiben aus, Ausgangspegel unveraendert
    int  currentFactory = -1;                     // Anzeige (nur GUI)

    // Aufgeloeste Delay-Zeit in ms (Sync -> Host-Tempo) und aktuelles Tempo, fuer die Anzeige
    float resolvedDelayMs() const;
    std::atomic<float> hostBpm { 120.f };
    std::atomic<float> inPeak { 0.f }, outPeak { 0.f };      // Spitzenpegel seit letzter GUI-Abfrage (GUI setzt zurueck)

    juce::AudioProcessorValueTreeState apvts;
    nwa::PresetManager presets;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    sfx::EngineParams readParams(double bpm) const;       // fuer Tests und processBlock

private:
    std::atomic<float>* pr(const char* id) const { return apvts.getRawParameterValue(id); }
    sfx::SpaceEngine engine;
    double sr = 44100.0;
    std::vector<float> bl, br;
};
} // namespace spf
