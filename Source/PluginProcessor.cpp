#include "PluginProcessor.h"
#include "PluginEditor.h"

const juce::StringArray TapeHaltProcessor::modeNames { "Classic", "Modern", "Loop" };
const juce::StringArray TapeHaltProcessor::loopNames { "1/32", "1/16", "1/8", "1/4", "1/2" };
const juce::StringArray TapeHaltProcessor::stopLenNames { "1/8", "1/4", "1/2", "1 BAR", "2 BARS", "4 BARS" };

TapeHaltProcessor::TapeHaltProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto& b : waveBins) b.store (0.f);
}

juce::AudioProcessorValueTreeState::ParameterLayout TapeHaltProcessor::createLayout()
{
    using juce::ParameterID;
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<juce::AudioParameterFloat> (ParameterID { "pitch", 1 }, "Pitch",
                                                        juce::NormalisableRange<float> (0.f, 1.f, 0.0001f), 1.f));
    l.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modeNames, 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "loop", 1 }, "Loop Length", loopNames, 1));
    l.add (std::make_unique<juce::AudioParameterBool>   (ParameterID { "stop", 1 }, "Stop", false));
    l.add (std::make_unique<juce::AudioParameterChoice> (ParameterID { "stopLen", 1 }, "Stop Length", stopLenNames, 3));
    l.add (std::make_unique<juce::AudioParameterFloat>  (ParameterID { "curve", 1 }, "Curve",
                                                         juce::NormalisableRange<float> (-100.f, 100.f, 1.f), 0.f));
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
    speed = 1.f; wet = 0.f; running = false; rampPos = 0.f;
    speedCoef = 1.f - std::exp (-1.f / (0.003f * (float) rate));
    wetCoef   = 1.f - std::exp (-1.f / (0.010f * (float) rate));

    waveBinSamples = juce::jmax (64, (int) (rate * 0.012));
    waveAcc = 0.f; waveCount = 0; waveHead.store (0);
    for (auto& b : waveBins) b.store (0.f);
    uiSpeed.store (1.f); uiLoopPos.store (-1.f);
}

void TapeHaltProcessor::engage()
{
    for (auto& c : ch)
    {
        c.delay = 0; c.phase = 0; c.loopPos = 0;
        for (int k = 0; k < loopLen; ++k)
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

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float pitch  = get ("pitch");
    const int   mode   = (int) get ("mode");
    const int   loopI  = (int) get ("loop");
    const bool  stopOn = get ("stop") > 0.5f;
    const int   stopI  = juce::jlimit (0, 5, (int) get ("stopLen"));
    const float k      = std::pow (4.f, get ("curve") / 100.f);   // 1 = linéaire

    double bpm = 120.0, beatsPerBar = 4.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto ts = pos->getTimeSignature())
                if (ts->denominator > 0) beatsPerBar = ts->numerator * 4.0 / ts->denominator;
        }
    bpm = juce::jmax (20.0, bpm);

    static const double loopBeats[] { 0.125, 0.25, 0.5, 1.0, 2.0 };
    const int wantedLoop = juce::jlimit (64, ringSize - 8,
                                         (int) (loopBeats[juce::jlimit (0, 4, loopI)] * 60.0 / bpm * sr));

    static const double stopBeats[] { 0.5, 1.0, 2.0, -1.0, -2.0, -4.0 };  // négatif = nombre de mesures
    const double beats = stopBeats[stopI] > 0 ? stopBeats[stopI] : -stopBeats[stopI] * beatsPerBar;
    const float rampInc = (float) (1.0 / juce::jmax (1.0, beats * 60.0 / bpm * sr));

    const float G = (float) grain;

    for (int i = 0; i < n; ++i)
    {
        rampPos = juce::jlimit (0.f, 1.f, rampPos + (stopOn ? rampInc : -rampInc));
        const float target  = pitch * (1.f - std::pow (rampPos, k));
        const bool  engaged = target < 0.995f;

        speed += speedCoef * ((engaged ? target : 1.f) - speed);
        wet   += wetCoef * ((engaged ? 1.f : 0.f) - wet);

        if (! running && engaged) { loopLen = wantedLoop; engage(); running = true; }
        else if (running && ! engaged && wet < 0.001f) running = false;

        const float amp = juce::jlimit (0.f, 1.f, speed * 10.f);
        float peak = 0.f;

        for (int c = 0; c < nch; ++c)
        {
            auto& s = ch[(size_t) c];
            float* d = buf.getWritePointer (c);
            const float dry = d[i];
            s.ring[(size_t) s.w] = dry;

            float w = dry;
            if (running)
            {
                if (mode == 0)
                {
                    s.delay = juce::jmin (s.delay + (1.f - speed), (float) (ringSize - 4));
                    w = readLinear (s.ring, (float) s.w - s.delay, ringSize);
                }
                else if (mode == 1)
                {
                    s.phase += (1.f - speed) / G;
                    s.phase -= std::floor (s.phase);
                    const float p2 = std::fmod (s.phase + 0.5f, 1.f);
                    const float w1 = std::sin (juce::MathConstants<float>::pi * s.phase);
                    const float w2 = std::sin (juce::MathConstants<float>::pi * p2);
                    w = readLinear (s.ring, (float) s.w - s.phase * G, ringSize) * w1 * w1
                      + readLinear (s.ring, (float) s.w - p2 * G,      ringSize) * w2 * w2;
                }
                else
                {
                    s.loopPos += speed;
                    if (s.loopPos >= (float) loopLen) s.loopPos -= (float) loopLen;
                    w = readLinear (s.loop, s.loopPos, loopLen);
                }
            }
            d[i] = dry * (1.f - wet) + w * amp * wet;
            peak = juce::jmax (peak, std::abs (d[i]));
            s.w = (s.w + 1) % ringSize;
        }

        waveAcc = juce::jmax (waveAcc, peak);
        if (++waveCount >= waveBinSamples)
        {
            const int h = waveHead.load();
            waveBins[(size_t) h].store (waveAcc);
            waveHead.store ((h + 1) % numWaveBins);
            waveAcc = 0.f; waveCount = 0;
        }
    }

    uiSpeed.store (speed);
    uiLoopPos.store ((running && mode == 2 && loopLen > 0) ? ch[0].loopPos / (float) loopLen : -1.f);
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
