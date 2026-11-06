#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray TapeHaltProcessor::modeNames { "Classic", "Modern", "Loop" };
const juce::StringArray TapeHaltProcessor::loopNames { "1/32", "1/16", "1/8", "1/4", "1/2" };

TapeHaltProcessor::TapeHaltProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout TapeHaltProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    // 1 = vitesse normale, 0 = bande à l'arrêt. Automatisable dans la DAW.
    l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { "pitch", 1 }, "Pitch",
                                                        juce::NormalisableRange<float> (0.f, 1.f, 0.0001f), 1.f));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode", 1 }, "Mode", modeNames, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "loop", 1 }, "Loop Length", loopNames, 1));
    return l;
}

bool TapeHaltProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
           && out == l.getMainInputChannelSet();
}

void TapeHaltProcessor::prepareToPlay (double rate, int)
{
    sr = rate;
    ringSize = (int) (rate * 12.0);
    grain    = juce::jmax (64, (int) (rate * 0.04));
    for (auto& c : ch)
    {
        c.ring.assign ((size_t) ringSize, 0.f);
        c.loop.assign ((size_t) ringSize, 0.f);
        c.w = 0; c.delay = 0; c.phase = 0; c.loopPos = 0;
    }
    speed = 1.f; wet = 0.f; running = false;
    speedCoef = 1.f - std::exp (-1.f / (0.003f * (float) rate));
    wetCoef   = 1.f - std::exp (-1.f / (0.010f * (float) rate));
}

void TapeHaltProcessor::engage()
{
    for (auto& c : ch)
    {
        c.delay = 0; c.phase = 0; c.loopPos = 0;
        for (int k = 0; k < loopLen; ++k)           // instantané des derniers loopLen samples
            c.loop[(size_t) k] = c.ring[(size_t) (((c.w - loopLen + k) % ringSize + ringSize) % ringSize)];
    }
}

static inline float readLinear (const std::vector<float>& b, float pos, int size)
{
    int i0 = (int) std::floor (pos);
    float f = pos - (float) i0;
    i0 = ((i0 % size) + size) % size;
    int i1 = (i0 + 1) % size;
    return b[(size_t) i0] + f * (b[(size_t) i1] - b[(size_t) i0]);
}

void TapeHaltProcessor::processBlock (juce::AudioBuffer<float>& buf, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    const int nch = juce::jmin (buf.getNumChannels(), 2);

    const float pitch = apvts.getRawParameterValue ("pitch")->load();
    const int   mode  = (int) apvts.getRawParameterValue ("mode")->load();
    const int   loopI = (int) apvts.getRawParameterValue ("loop")->load();

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    static const double beats[] { 0.125, 0.25, 0.5, 1.0, 2.0 };
    const int wantedLoop = juce::jlimit (64, ringSize - 8, (int) (beats[juce::jlimit (0, 4, loopI)] * 60.0 / bpm * sr));

    const bool engaged = pitch < 0.995f;
    const float target = engaged ? pitch : 1.f;
    const float G = (float) grain;

    for (int i = 0; i < n; ++i)
    {
        speed += speedCoef * (target - speed);
        wet   += wetCoef * ((engaged ? 1.f : 0.f) - wet);

        if (! running && engaged) { loopLen = wantedLoop; engage(); running = true; }
        else if (running && ! engaged && wet < 0.001f) running = false;

        // fondu vers le silence quand la bande est (presque) arrêtée, évite un DC
        const float amp = juce::jlimit (0.f, 1.f, speed * 10.f);

        for (int c = 0; c < nch; ++c)
        {
            auto& s = ch[(size_t) c];
            float* d = buf.getWritePointer (c);
            const float dry = d[i];
            s.ring[(size_t) s.w] = dry;

            float w = dry;
            if (running)
            {
                if (mode == 0) // Classic : pitch ET temps ralentissent ensemble
                {
                    s.delay = juce::jmin (s.delay + (1.f - speed), (float) (ringSize - 4));
                    w = readLinear (s.ring, (float) s.w - s.delay, ringSize);
                }
                else if (mode == 1) // Modern : pitch baisse, le temps continue (2 grains croisés)
                {
                    s.phase += (1.f - speed) / G;
                    s.phase -= std::floor (s.phase);
                    const float p2 = std::fmod (s.phase + 0.5f, 1.f);
                    const float w1 = std::sin (juce::MathConstants<float>::pi * s.phase);
                    const float w2 = std::sin (juce::MathConstants<float>::pi * p2);
                    w = readLinear (s.ring, (float) s.w - s.phase * G, ringSize) * w1 * w1
                      + readLinear (s.ring, (float) s.w - p2 * G,      ringSize) * w2 * w2;
                }
                else // Loop : boucle sur les derniers instants, lue à vitesse variable
                {
                    s.loopPos += speed;
                    if (s.loopPos >= (float) loopLen) s.loopPos -= (float) loopLen;
                    w = readLinear (s.loop, s.loopPos, loopLen);
                }
            }
            d[i] = dry * (1.f - wet) + w * amp * wet;
            s.w = (s.w + 1) % ringSize;
        }
    }
}

juce::AudioProcessorEditor* TapeHaltProcessor::createEditor() { return new TapeHaltEditor (*this); }

void TapeHaltProcessor::getStateInformation (juce::MemoryBlock& m)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, m);
}
void TapeHaltProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TapeHaltProcessor(); }
