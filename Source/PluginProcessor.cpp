#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    const juce::StringArray kKeys { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
}

juce::AudioProcessorValueTreeState::ParameterLayout RayaTuneAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", kKeys, 0));

    StringArray scales;
    for (int i = 0; i < AutoTuneEngine::kNumScales; ++i) scales.add (AutoTuneEngine::scaleName (i));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scales, 0));

    auto fl = [] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit, int dec)
    {
        const String u (unit);
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
            AudioParameterFloatAttributes()
                .withLabel (u)
                .withStringFromValueFunction ([u, dec] (float v, int) { return String (v, dec) + " " + u; })
                .withValueFromStringFunction ([] (const String& t) { return t.getFloatValue(); }));
    };
    p.push_back (fl ("retune", "Retune Speed",  { 0.f, 400.f, 0.1f, 0.4f }, 20.f,  "ms", 1));
    p.push_back (fl ("amount", "Amount",        { 0.f, 100.f, 0.1f },       100.f, "%",  0));
    p.push_back (fl ("detune", "Detune",        { -100.f, 100.f, 0.1f },    0.f,   "c",  1));
    p.push_back (fl ("refA",   "Reference A",   { 415.f, 466.f, 0.1f },     440.f, "Hz", 1));
    p.push_back (fl ("mix",    "Mix",           { 0.f, 100.f, 0.1f },       100.f, "%",  0));
    p.push_back (fl ("output", "Output",        { -24.f, 12.f, 0.1f },      0.f,   "dB", 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));

    return { p.begin(), p.end() };
}

RayaTuneAudioProcessor::RayaTuneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RAYATUNE", createLayout())
{
}

bool RayaTuneAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return out == l.getMainInputChannelSet();
}

void RayaTuneAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, juce::jmax (1, getTotalNumInputChannels()));
    setLatencySamples (engine.latencySamples());
}

void RayaTuneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int c = numIn; c < numOut; ++c) buffer.clear (c, 0, n);

    TuneParams t;
    t.keyRoot     = (int) apvts.getRawParameterValue ("key")->load();
    t.scaleIndex  = (int) apvts.getRawParameterValue ("scale")->load();
    t.retuneMs    = apvts.getRawParameterValue ("retune")->load();
    t.amount      = apvts.getRawParameterValue ("amount")->load() * 0.01f;
    t.detuneCents = apvts.getRawParameterValue ("detune")->load();
    t.refA        = apvts.getRawParameterValue ("refA")->load();
    t.mix         = apvts.getRawParameterValue ("mix")->load() * 0.01f;
    t.outGainDb   = apvts.getRawParameterValue ("output")->load();
    t.bypass      = apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    engine.setParams (t);

    engine.process (buffer.getArrayOfWritePointers(), juce::jmin (numIn, buffer.getNumChannels()), n);
}

juce::AudioProcessorEditor* RayaTuneAudioProcessor::createEditor()
{
    return new RayaTuneAudioProcessorEditor (*this);
}

void RayaTuneAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void RayaTuneAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RayaTuneAudioProcessor();
}
