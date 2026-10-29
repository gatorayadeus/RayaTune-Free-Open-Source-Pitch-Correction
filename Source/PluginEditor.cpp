#include "PluginEditor.h"
#include "BinaryData.h"

//==============================================================================
RayaLookAndFeel::RayaLookAndFeel()
{
    using namespace juce;
    setColour (ComboBox::backgroundColourId, kNavyDark);
    setColour (ComboBox::textColourId, Colours::white);
    setColour (ComboBox::outlineColourId, kGold.withAlpha (0.65f));
    setColour (ComboBox::arrowColourId, kGold);
    setColour (PopupMenu::backgroundColourId, kNavyDark);
    setColour (PopupMenu::textColourId, Colours::white);
    setColour (PopupMenu::highlightedBackgroundColourId, kGold);
    setColour (PopupMenu::highlightedTextColourId, kNavy);
    setColour (Slider::textBoxTextColourId, Colours::white);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (Label::textColourId, Colours::white);
    setColour (TextButton::buttonColourId, kNavyDark);
    setColour (TextButton::buttonOnColourId, kGold);
    setColour (TextButton::textColourOffId, Colours::white);
    setColour (TextButton::textColourOnId, kNavy);
}

void RayaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    using namespace juce;
    auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // arco de fondo y arco de valor
    const float ar = r * 0.94f;
    Path track;  track.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, endAngle, true);
    g.setColour (Colours::white.withAlpha (0.14f));
    g.strokePath (track, PathStrokeType (4.f, PathStrokeType::curved, PathStrokeType::rounded));
    Path val;    val.addCentredArc (c.x, c.y, ar, ar, 0.f, startAngle, angle, true);
    g.setColour (kGold);
    g.strokePath (val, PathStrokeType (4.f, PathStrokeType::curved, PathStrokeType::rounded));

    // cuerpo
    const float br = r * 0.74f;
    g.setGradientFill (ColourGradient (kNavyLight, c.x, c.y - br, kNavyDark, c.x, c.y + br, false));
    g.fillEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f);
    g.setColour (kGold.withAlpha (0.85f));
    g.drawEllipse (c.x - br, c.y - br, br * 2.f, br * 2.f, 1.5f);

    // aguja
    Path p;
    p.addRoundedRectangle (-2.5f, -br * 0.92f, 5.f, br * 0.46f, 2.f);
    p.applyTransform (AffineTransform::rotation (angle).translated (c.x, c.y));
    g.setColour (Colours::white);
    g.fillPath (p);
}

//==============================================================================
RayaTuneAudioProcessorEditor::RayaTuneAudioProcessorEditor (RayaTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);
    logo = juce::Drawable::createFromImageData (BinaryData::logo_svg, (size_t) BinaryData::logo_svgSize);

    keyBox.addItemList (juce::StringArray { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 1);
    juce::StringArray scales;
    for (int i = 0; i < AutoTuneEngine::kNumScales; ++i) scales.add (AutoTuneEngine::scaleName (i));
    scaleBox.addItemList (scales, 1);
    addAndMakeVisible (keyBox);
    addAndMakeVisible (scaleBox);
    keyAtt   = std::make_unique<ComboAtt> (proc.apvts, "key", keyBox);
    scaleAtt = std::make_unique<ComboAtt> (proc.apvts, "scale", scaleBox);

    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAtt = std::make_unique<ButtonAtt> (proc.apvts, "bypass", bypassButton);

    addKnob (retune, "RETUNE SPEED", "retune");
    addKnob (amount, "AMOUNT",       "amount");
    addKnob (detune, "DETUNE",       "detune");
    addKnob (refA,   "REFERENCE A",  "refA");
    addKnob (mix,    "MIX",          "mix");
    addKnob (output, "OUTPUT",       "output");

    setSize (780, 470);
    startTimerHz (30);
}

RayaTuneAudioProcessorEditor::~RayaTuneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void RayaTuneAudioProcessorEditor::addKnob (Knob& k, const juce::String& text, const juce::String& id)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    k.label.setText (text, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (12.5f, juce::Font::bold).withKerningFactor (0.1f)));
    k.label.setColour (juce::Label::textColourId, kGold);
    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
    k.att = std::make_unique<SliderAtt> (proc.apvts, id, k.slider);
}

void RayaTuneAudioProcessorEditor::resized()
{
    keyBox.setBounds (470, 38, 90, 28);
    scaleBox.setBounds (570, 38, 186, 28);
    bypassButton.setBounds (470, 76, 286, 28);

    Knob* ks[] = { &retune, &amount, &detune, &refA, &mix, &output };
    const int w = 732 / 6, y0 = 298;
    for (int i = 0; i < 6; ++i)
    {
        const int x = 24 + i * w;
        ks[i]->label.setBounds (x, y0, w, 20);
        ks[i]->slider.setBounds (x + 4, y0 + 20, w - 8, 140);
    }
}

void RayaTuneAudioProcessorEditor::timerCallback()
{
    shownDetected   = proc.engine.meterDetectedMidi.load (std::memory_order_relaxed);
    shownTarget     = proc.engine.meterTargetMidi.load (std::memory_order_relaxed);
    shownCorrection = proc.engine.meterCorrection.load (std::memory_order_relaxed);
    repaint (displayArea);
}

//==============================================================================
void RayaTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    g.setGradientFill (ColourGradient (kNavyLight.darker (0.2f), 0.f, 0.f, kNavyDark, 0.f, (float) getHeight(), false));
    g.fillAll();

    // Cabecera
    g.setColour (kGold);
    g.fillRect (0, 114, getWidth(), 2);
    if (logo != nullptr)
        logo->drawWithin (g, Rectangle<float> (24.f, 10.f, 72.f, 98.f), RectanglePlacement::centred, 1.f);

    g.setColour (kGold);
    g.setFont (Font (FontOptions (46.f, Font::bold).withKerningFactor (0.1f)));
    g.drawText ("RAYATUNE", 112, 16, 340, 54, Justification::centredLeft);
    g.setColour (Colours::white.withAlpha (0.8f));
    g.setFont (Font (FontOptions (12.5f).withKerningFactor (0.28f)));
    g.drawText ("AUTO PITCH CORRECTION", 114, 70, 320, 18, Justification::centredLeft);

    // cinco estrellas, como en el escudo
    for (int i = 0; i < 5; ++i)
    {
        Path star;
        star.addStar ({ 120.f + (float) i * 22.f, 98.f }, 5, 3.6f, 8.f, 0.f);
        g.setColour (kGold);
        g.fillPath (star);
    }

    g.setColour (kGold);
    g.setFont (Font (FontOptions (11.f, Font::bold).withKerningFactor (0.15f)));
    g.drawText ("KEY", 470, 22, 90, 14, Justification::centredLeft);
    g.drawText ("SCALE", 570, 22, 186, 14, Justification::centredLeft);

    // Panel de display y de controles
    paintDisplay (g, displayArea);
    auto ctl = Rectangle<float> (24.f, 290.f, 732.f, 172.f);
    g.setColour (kNavyDark.withAlpha (0.55f));
    g.fillRoundedRectangle (ctl, 10.f);
    g.setColour (kGold.withAlpha (0.35f));
    g.drawRoundedRectangle (ctl, 10.f, 1.2f);
}

void RayaTuneAudioProcessorEditor::paintDisplay (juce::Graphics& g, juce::Rectangle<int> area)
{
    using namespace juce;
    static const char* names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    auto a = area.toFloat();
    g.setColour (kNavyDark);
    g.fillRoundedRectangle (a, 10.f);
    g.setColour (kGold.withAlpha (0.7f));
    g.drawRoundedRectangle (a, 10.f, 1.5f);

    const bool voiced = shownTarget > -0.5f && shownDetected > -0.5f;

    // Nota objetivo grande
    String noteText = "--", centsText = "no signal";
    if (voiced)
    {
        const int nt = (int) std::lround (shownTarget);
        noteText = String (names[((nt % 12) + 12) % 12]) + String (juce::roundToInt (std::floor ((float) nt / 12.f)) - 1);
        const float dev = (shownDetected - shownTarget) * 100.f;
        centsText = String (dev, 0) + " c  input";
        if (dev > 0.5f) centsText = "+" + centsText;
    }
    g.setColour (Colours::white);
    g.setFont (Font (FontOptions (64.f, Font::bold)));
    g.drawText (noteText, area.getX() + 14, area.getY() + 14, 170, 74, Justification::centredLeft);
    g.setColour (kGold);
    g.setFont (Font (FontOptions (15.f, Font::bold)));
    g.drawText (centsText, area.getX() + 18, area.getY() + 92, 170, 20, Justification::centredLeft);
    g.setColour (Colours::white.withAlpha (0.6f));
    g.setFont (Font (FontOptions (11.f).withKerningFactor (0.15f)));
    g.drawText ("TARGET NOTE", area.getX() + 18, area.getY() + 112, 170, 16, Justification::centredLeft);

    // Teclado de 12 notas
    const int keyIdx   = (int) proc.apvts.getRawParameterValue ("key")->load();
    const int scaleIdx = (int) proc.apvts.getRawParameterValue ("scale")->load();
    const uint16_t mask = AutoTuneEngine::scaleMask (scaleIdx);

    const float kx = (float) area.getX() + 200.f, ky = (float) area.getY() + 20.f;
    const float cw = 520.f / 12.f, kh = 64.f;
    const int detPc = voiced ? ((int) std::lround (shownDetected) % 12 + 12) % 12 : -1;
    const int tgtPc = voiced ? ((int) std::lround (shownTarget) % 12 + 12) % 12 : -1;

    for (int pc = 0; pc < 12; ++pc)
    {
        const bool allowed = (mask >> (((pc - keyIdx) % 12 + 12) % 12)) & 1;
        auto cell = Rectangle<float> (kx + pc * cw, ky, cw, kh).reduced (2.f);
        g.setColour (allowed ? Colours::white.withAlpha (0.16f) : kNavy.withAlpha (0.8f));
        g.fillRoundedRectangle (cell, 4.f);
        if (pc == tgtPc) { g.setColour (kGold); g.fillRoundedRectangle (cell, 4.f); }
        g.setColour (allowed ? Colours::white.withAlpha (0.55f) : Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (cell, 4.f, 1.f);
        g.setColour (pc == tgtPc ? kNavy : (allowed ? Colours::white : Colours::white.withAlpha (0.3f)));
        g.setFont (Font (FontOptions (13.f, Font::bold)));
        g.drawText (names[pc], cell, Justification::centred);
        if (pc == keyIdx)
        {
            g.setColour (kGold);
            g.fillRect (cell.getX() + 8.f, cell.getBottom() - 6.f, cell.getWidth() - 16.f, 3.f);
        }
    }

    // Marcador de tono detectado (continuo)
    if (voiced)
    {
        float pcF = std::fmod (shownDetected, 12.f);
        if (pcF < 0.f) pcF += 12.f;
        const float mx = kx + (pcF + 0.5f) * cw;
        g.setColour (kGoldLight);
        g.fillEllipse (mx - 5.f, ky + kh + 5.f, 10.f, 10.f);
        (void) detPc;
    }

    // Medidor de cents (-50 .. +50) entre tono de entrada y objetivo
    const float mxL = kx, mw = 520.f, my = (float) area.getY() + 112.f;
    g.setColour (Colours::white.withAlpha (0.15f));
    g.fillRoundedRectangle (mxL, my, mw, 8.f, 4.f);
    g.setColour (Colours::white.withAlpha (0.6f));
    g.fillRect (mxL + mw * 0.5f - 1.f, my - 4.f, 2.f, 16.f);
    if (voiced)
    {
        const float dev = jlimit (-50.f, 50.f, (shownDetected - shownTarget) * 100.f);
        const float nx = mxL + mw * (0.5f + dev / 100.f);
        g.setColour (kGold);
        g.fillRoundedRectangle (jmin (nx, mxL + mw * 0.5f), my, std::abs (nx - (mxL + mw * 0.5f)), 8.f, 3.f);
        g.setColour (Colours::white);
        g.fillRoundedRectangle (nx - 2.f, my - 5.f, 4.f, 18.f, 2.f);
    }
    g.setColour (Colours::white.withAlpha (0.55f));
    g.setFont (Font (FontOptions (11.f)));
    g.drawText ("-50", (int) mxL - 4, (int) my + 14, 40, 14, Justification::centredLeft);
    g.drawText ("0", (int) (mxL + mw * 0.5f) - 20, (int) my + 14, 40, 14, Justification::centred);
    g.drawText ("+50", (int) (mxL + mw) - 36, (int) my + 14, 40, 14, Justification::centredRight);
}
