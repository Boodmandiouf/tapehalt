#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

class TapeHaltProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numWaveBins = 200;

    TapeHaltProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "TapeHalt"; }
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

    juce::AudioProcessorValueTreeState apvts;

    static const juce::StringArray modeNames;
    static const juce::StringArray loopNames;
    static const juce::StringArray stopLenNames;

    // Écrits par le thread audio, lus par l'interface
    std::array<std::atomic<float>, numWaveBins> waveBins;
    std::atomic<int>   waveHead { 0 };
    std::atomic<float> uiSpeed { 1.f };     // vitesse effective (0..1)
    std::atomic<float> uiLoopPos { -1.f };  // position dans la boucle (0..1), -1 si inactif

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Channel
    {
        std::vector<float> ring, loop;
        int   w = 0;
        float delay = 0.f;
        float phase = 0.f;
        float loopPos = 0.f;
    };

    void engage();

    std::array<Channel, 2> ch;
    double sr = 44100.0;
    int ringSize = 0, grain = 0, loopLen = 0;
    float speed = 1.f, wet = 0.f, speedCoef = 0.f, wetCoef = 0.f;
    float rampPos = 0.f;                 // progression du STOP (0 = normal, 1 = arrêté)
    bool running = false;
    float waveAcc = 0.f;
    int waveCount = 0, waveBinSamples = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeHaltProcessor)
};
