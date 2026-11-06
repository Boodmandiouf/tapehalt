#pragma once
#include "PluginProcessor.h"

class TapeHaltEditor : public juce::AudioProcessorEditor
{
public:
    explicit TapeHaltEditor (TapeHaltProcessor& p) : AudioProcessorEditor (&p),
        pitchAtt (p.apvts, "pitch", pitch), modeAtt (p.apvts, "mode", mode), loopAtt (p.apvts, "loop", loop)
    {
        pitch.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        pitch.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 22);
        pitch.setDoubleClickReturnValue (true, 1.0);
        addAndMakeVisible (pitch);
        mode.addItemList (TapeHaltProcessor::modeNames, 1);
        loop.addItemList (TapeHaltProcessor::loopNames, 1);
        addAndMakeVisible (mode);
        addAndMakeVisible (loop);
        for (auto* l : { &modeL, &loopL })
        {
            l->setJustificationType (juce::Justification::centred);
            addAndMakeVisible (l);
        }
        modeL.setText ("Mode", juce::dontSendNotification);
        loopL.setText ("Loop length", juce::dontSendNotification);
        setSize (360, 360);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1b1b22));
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (22.f, juce::Font::bold));
        g.drawText ("TapeHalt", 0, 8, getWidth(), 30, juce::Justification::centred);
    }

    void resized() override
    {
        pitch.setBounds (getLocalBounds().withTrimmedTop (45).removeFromTop (230).reduced (40, 0));
        mode.setBounds (30, 300, 140, 28);  modeL.setBounds (30, 278, 140, 20);
        loop.setBounds (190, 300, 140, 28); loopL.setBounds (190, 278, 140, 20);
    }

private:
    juce::Slider pitch;
    juce::ComboBox mode, loop;
    juce::Label modeL, loopL;
    juce::AudioProcessorValueTreeState::SliderAttachment pitchAtt;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment modeAtt, loopAtt;
};
