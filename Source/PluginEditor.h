#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Paleta tomada del escudo de referencia
inline const juce::Colour kNavy      { 0xff0a2240 };
inline const juce::Colour kNavyLight { 0xff143a6b };
inline const juce::Colour kNavyDark  { 0xff050f1f };
inline const juce::Colour kGold      { 0xffd59f0f };
inline const juce::Colour kGoldLight { 0xfff2c94c };

class RayaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RayaLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

class RayaTuneAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RayaTuneAudioProcessorEditor (RayaTuneAudioProcessor&);
    ~RayaTuneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<SliderAtt> att;
    };

    void timerCallback() override;
    void paintDisplay (juce::Graphics&, juce::Rectangle<int> area);
    void addKnob (Knob& k, const juce::String& text, const juce::String& paramId);

    RayaTuneAudioProcessor& proc;
    RayaLookAndFeel laf;
    std::unique_ptr<juce::Drawable> logo;

    Knob retune, amount, detune, refA, mix, output;
    juce::ComboBox keyBox, scaleBox;
    juce::TextButton bypassButton { "BYPASS" };
    std::unique_ptr<ComboAtt> keyAtt, scaleAtt;
    std::unique_ptr<ButtonAtt> bypassAtt;

    juce::Rectangle<int> displayArea { 24, 128, 732, 152 };
    float shownDetected = -1.f, shownTarget = -1.f, shownCorrection = 0.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RayaTuneAudioProcessorEditor)
};
