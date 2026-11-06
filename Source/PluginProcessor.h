#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class TapeHaltProcessor : public juce::AudioProcessor
{
public:
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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Channel
    {
        std::vector<float> ring, loop;
        int   w = 0;
        float delay = 0.f;   // Classic
        float phase = 0.f;   // Modern
        float loopPos = 0.f; // Loop
    };

    void engage();

    std::array<Channel, 2> ch;
    double sr = 44100.0;
    int ringSize = 0, grain = 0, loopLen = 0;
    float speed = 1.f, wet = 0.f, speedCoef = 0.f, wetCoef = 0.f;
    bool running = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TapeHaltProcessor)
};
