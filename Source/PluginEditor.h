#pragma once
#include "PluginProcessor.h"

namespace th
{
    inline const juce::Colour bgTop   (0xff3a0f16);
    inline const juce::Colour bgBot   (0xff1c070b);
    inline const juce::Colour panel   (0xff4a1a23);
    inline const juce::Colour track   (0xff7a2a36);
    inline const juce::Colour disc    (0xff230a0f);
    inline const juce::Colour red     (0xffff3344);
    inline const juce::Colour redHi   (0xffff7a86);
    inline const juce::Colour amber   (0xffffb02e);
    inline const juce::Colour text    (0xfffff1f2);
    inline const juce::Colour dim     (0xffc99aa1);
}

//==============================================================================
class PillButton : public juce::Button
{
public:
    explicit PillButton (const juce::String& t) : juce::Button (t) {}
    bool active = false;

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        const float r = b.getHeight() / 2.f;
        g.setColour (active ? th::red : (over ? th::panel.brighter (0.2f) : th::panel));
        g.fillRoundedRectangle (b, r);
        g.setColour (active ? th::redHi : th::track);
        g.drawRoundedRectangle (b, r, 2.f);
        g.setColour (active ? juce::Colours::white : th::text);
        g.setFont (juce::FontOptions (14.f, juce::Font::bold));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred);
    }
};

//==============================================================================
// Petite boîte cliquable qui fait défiler les valeurs d'un paramètre "choix"
class CycleBox : public juce::Component
{
public:
    CycleBox (juce::RangedAudioParameter& p, juce::StringArray n)
        : names (std::move (n)),
          att (p, [this] (float v) { index = juce::jlimit (0, juce::jmax (0, names.size() - 1), (int) std::round (v)); repaint(); })
    {
        att.sendInitialUpdate();
    }

    bool dimmed = false;

    void paint (juce::Graphics& g) override
    {
        const float a = dimmed ? 0.45f : 1.f;
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (th::panel.withAlpha (a));
        g.fillRoundedRectangle (b, 9.f);
        g.setColour (th::track.withAlpha (a));
        g.drawRoundedRectangle (b, 9.f, 2.f);
        g.setColour (th::text.withAlpha (a));
        g.setFont (juce::FontOptions (16.f, juce::Font::bold));
        g.drawText (names[index], getLocalBounds(), juce::Justification::centred);

        const float h = (float) getHeight() * 0.5f, w = (float) getWidth();
        juce::Path l, r;
        l.addTriangle (11.f, h, 18.f, h - 5.f, 18.f, h + 5.f);
        r.addTriangle (w - 11.f, h, w - 18.f, h - 5.f, w - 18.f, h + 5.f);
        g.setColour (th::red.withAlpha (a));
        g.fillPath (l);
        g.fillPath (r);
    }

    void mouseDown (const juce::MouseEvent& e) override { step (e.mods.isRightButtonDown() ? -1 : 1); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY != 0.f) step (w.deltaY > 0.f ? 1 : -1);
    }

private:
    void step (int d)
    {
        const int n = names.size();
        if (n > 0) att.setValueAsCompleteGesture ((float) ((index + d + n) % n));
    }

    juce::StringArray names;
    int index = 0;
    juce::ParameterAttachment att;
};

//==============================================================================
class CurveKnob : public juce::Component
{
public:
    explicit CurveKnob (juce::RangedAudioParameter& p)
        : att (p, [this] (float v) { value = v; repaint(); })
    {
        att.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto circ = juce::Rectangle<float> (60.f, 60.f).withCentre ({ getWidth() * 0.5f, 32.f });
        g.setColour (th::panel);
        g.fillEllipse (circ);
        g.setColour (th::track);
        g.drawEllipse (circ.reduced (1.f), 2.f);

        const float k = std::pow (4.f, value / 100.f);
        auto box = circ.reduced (15.f);
        juce::Path pth;
        for (int i = 0; i <= 24; ++i)
        {
            const float p = (float) i / 24.f;
            const float s = 1.f - std::pow (p, k);
            const float x = box.getX() + p * box.getWidth();
            const float y = box.getBottom() - s * box.getHeight();
            if (i == 0) pth.startNewSubPath (x, y); else pth.lineTo (x, y);
        }
        g.setColour (value == 0.f ? th::text : th::red);
        g.strokePath (pth, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (9.f, 9.f).withCentre ({ box.getX(), box.getY() }));

        g.setColour (th::text);
        g.setFont (juce::FontOptions (16.f, juce::Font::bold));
        g.drawText (value == 0.f ? juce::String ("Off") : juce::String ((int) std::round (value)),
                    0, 66, getWidth(), 20, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { start = value; att.beginGesture(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float f = e.mods.isShiftDown() ? 0.25f : 1.f;
        att.setValueAsPartOfGesture (juce::jlimit (-100.f, 100.f, start - (float) e.getDistanceFromDragStartY() * f));
    }
    void mouseUp (const juce::MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { att.setValueAsCompleteGesture (0.f); }

private:
    float value = 0.f, start = 0.f;
    juce::ParameterAttachment att;
};

//==============================================================================
class StopButton : public juce::Component
{
public:
    explicit StopButton (juce::RangedAudioParameter& p)
        : att (p, [this] (float v) { on = v > 0.5f; repaint(); })
    {
        att.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto circ = juce::Rectangle<float> (60.f, 60.f).withCentre ({ getWidth() * 0.5f, 32.f });
        g.setColour (on ? th::red : th::panel);
        g.fillEllipse (circ);
        g.setColour (on ? th::redHi : th::track);
        g.drawEllipse (circ.reduced (1.f), 2.f);
        g.setColour (on ? juce::Colours::white : th::text);
        g.fillRoundedRectangle (juce::Rectangle<float> (22.f, 22.f).withCentre (circ.getCentre()), 4.f);
    }

    void mouseDown (const juce::MouseEvent&) override { att.setValueAsCompleteGesture (on ? 0.f : 1.f); }

private:
    bool on = false;
    juce::ParameterAttachment att;
};

//==============================================================================
class Wheel : public juce::Component, private juce::Timer
{
public:
    explicit Wheel (TapeHaltProcessor& p)
        : proc (p), att (*p.apvts.getParameter ("pitch"), [this] (float v) { pitch = v; repaint(); })
    {
        att.sendInitialUpdate();
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        using juce::MathConstants;
        const auto b = getLocalBounds().toFloat();
        const auto c = b.getCentre();
        const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 6.f;
        const float ringW = 16.f;
        const float a0 = -0.75f * MathConstants<float>::pi, a1 = 0.75f * MathConstants<float>::pi;

        auto arc = [&] (float rad, float from, float to, juce::Colour col, float w)
        {
            juce::Path p;
            p.addCentredArc (c.x, c.y, rad, rad, 0.f, from, to, true);
            g.setColour (col);
            g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        };

        // anneau principal : vitesse effective (rouge) et position du réglage Pitch (point blanc)
        const float rr = R - ringW * 0.5f;
        arc (rr, a0, a1, th::track, ringW);
        const float sp = juce::jlimit (0.f, 1.f, proc.uiSpeed.load());
        if (sp > 0.004f) arc (rr, a0, a0 + sp * (a1 - a0), th::red, ringW);
        const float ap = a0 + pitch * (a1 - a0);
        const juce::Point<float> thumb (c.x + rr * std::sin (ap), c.y - rr * std::cos (ap));
        g.setColour (th::red.darker (0.3f));
        g.fillEllipse (juce::Rectangle<float> (26.f, 26.f).withCentre (thumb));
        g.setColour (juce::Colours::white);
        g.fillEllipse (juce::Rectangle<float> (16.f, 16.f).withCentre (thumb));

        // anneau Loop (orange) : visible seulement en mode Loop
        const bool loopMode = (int) proc.apvts.getRawParameterValue ("mode")->load() == 2;
        const float Rl = R - ringW - 12.f;
        if (loopMode)
        {
            arc (Rl, 0.f, 2.f * MathConstants<float>::pi - 0.001f, th::amber.withAlpha (0.28f), 7.f);
            const float lp = proc.uiLoopPos.load();
            if (lp > 0.f)
            {
                arc (Rl, 0.f, lp * 2.f * MathConstants<float>::pi, th::amber, 7.f);
                const float ang = lp * 2.f * MathConstants<float>::pi;
                g.setColour (juce::Colours::white);
                g.fillEllipse (juce::Rectangle<float> (11.f, 11.f).withCentre ({ c.x + Rl * std::sin (ang), c.y - Rl * std::cos (ang) }));
            }
        }

        // disque intérieur + forme d'onde
        const float Ri = Rl - 12.f;
        g.setColour (th::disc);
        g.fillEllipse (c.x - Ri, c.y - Ri, 2.f * Ri, 2.f * Ri);

        g.saveState();
        juce::Path clip;
        clip.addEllipse (c.x - Ri, c.y - Ri, 2.f * Ri, 2.f * Ri);
        g.reduceClipRegion (clip);
        const int N = TapeHaltProcessor::numWaveBins;
        const int head = proc.waveHead.load();
        const float bw = (2.f * Ri) / (float) N;
        for (int i = 0; i < N; ++i)
        {
            const float v = juce::jlimit (0.f, 1.f, proc.waveBins[(size_t) ((head + i) % N)].load());
            const float h = juce::jmax (2.f, std::pow (v, 0.6f) * Ri * 1.7f);
            g.setColour (th::red.withAlpha (0.35f + 0.65f * (float) i / (float) N));
            g.fillRect (c.x - Ri + (float) i * bw, c.y - h * 0.5f, juce::jmax (1.f, bw - 0.5f), h);
        }
        g.restoreState();

        // pourcentage de vitesse
        auto pill = juce::Rectangle<float> (78.f, 28.f).withCentre ({ c.x, c.y + Ri - 26.f });
        g.setColour (th::panel);
        g.fillRoundedRectangle (pill, 14.f);
        g.setColour (th::text);
        g.setFont (juce::FontOptions (16.f, juce::Font::bold));
        g.drawText (juce::String ((int) std::round (sp * 100.f)) + "%", pill, juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent&) override { start = pitch; att.beginGesture(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float f = e.mods.isShiftDown() ? 1000.f : 250.f;
        const float d = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
        att.setValueAsPartOfGesture (juce::jlimit (0.f, 1.f, start + d / f));
    }
    void mouseUp (const juce::MouseEvent&) override { att.endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { att.setValueAsCompleteGesture (1.f); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        att.setValueAsCompleteGesture (juce::jlimit (0.f, 1.f, pitch + w.deltaY * 0.1f));
    }

private:
    void timerCallback() override { repaint(); }

    TapeHaltProcessor& proc;
    float pitch = 1.f, start = 1.f;
    juce::ParameterAttachment att;
};

//==============================================================================
class TapeHaltEditor : public juce::AudioProcessorEditor
{
public:
    explicit TapeHaltEditor (TapeHaltProcessor& p)
        : AudioProcessorEditor (&p),
          wheel (p),
          curve (*p.apvts.getParameter ("curve")),
          stop (*p.apvts.getParameter ("stop")),
          stopLen (*p.apvts.getParameter ("stopLen"), TapeHaltProcessor::stopLenNames),
          loopLen (*p.apvts.getParameter ("loop"), TapeHaltProcessor::loopNames),
          modeAtt (*p.apvts.getParameter ("mode"), [this] (float v) { updateModeButtons ((int) std::round (v)); })
    {
        for (int i = 0; i < 3; ++i)
        {
            btns[i]->onClick = [this, i] { modeAtt.setValueAsCompleteGesture ((float) i); };
            addAndMakeVisible (*btns[i]);
        }
        addAndMakeVisible (wheel);
        addAndMakeVisible (curve);
        addAndMakeVisible (stop);
        addAndMakeVisible (stopLen);
        addAndMakeVisible (loopLen);
        modeAtt.sendInitialUpdate();
        setSize (460, 600);
    }

    void paint (juce::Graphics& g) override
    {
        g.setGradientFill (juce::ColourGradient (th::bgTop, 0.f, 0.f, th::bgBot, 0.f, (float) getHeight(), false));
        g.fillAll();
        g.setColour (th::text);
        g.setFont (juce::FontOptions (26.f, juce::Font::bold));
        g.drawText ("TapeHalt", 0, 10, getWidth(), 32, juce::Justification::centred);
        g.setColour (th::dim);
        g.setFont (juce::FontOptions (12.f, juce::Font::plain));
        g.drawText ("BoodmanFX", 0, getHeight() - 24, getWidth(), 16, juce::Justification::centred);
    }

    void resized() override
    {
        wheel.setBounds (60, 48, 340, 340);
        curve.setBounds (15, 378, 90, 88);
        stop.setBounds (355, 378, 90, 64);
        stopLen.setBounds (355, 446, 90, 28);
        for (int i = 0; i < 3; ++i) btns[i]->setBounds (55 + i * 120, 490, 110, 36);
        loopLen.setBounds (295, 534, 110, 28);
    }

private:
    void updateModeButtons (int m)
    {
        for (int i = 0; i < 3; ++i) { btns[i]->active = (i == m); btns[i]->repaint(); }
        loopLen.dimmed = (m != 2);
        loopLen.repaint();
    }

    Wheel wheel;
    CurveKnob curve;
    StopButton stop;
    CycleBox stopLen, loopLen;
    PillButton btnClassic { "CLASSIC" }, btnModern { "MODERN" }, btnLoop { "LOOP" };
    PillButton* btns[3] { &btnClassic, &btnModern, &btnLoop };
    juce::ParameterAttachment modeAtt;
};
