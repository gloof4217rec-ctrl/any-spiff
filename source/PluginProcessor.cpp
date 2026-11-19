#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
       apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    cacheParameterPointers();
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    using namespace juce;

    constexpr int versionHint = 1;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", versionHint },
                                                       "Mode",
                                                       StringArray { "Cut", "Boost" },
                                                       0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "depth", versionHint },
                                                       "Depth",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                                       0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sensitivity", versionHint },
                                                       "Sensitivity",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                                       50.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sharpness", versionHint },
                                                       "Sharpness",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                                       50.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "decay", versionHint },
                                                       "Decay",
                                                       NormalisableRange<float> (0.0f, 10.0f, 0.01f),
                                                       0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "decay_lfhf", versionHint },
                                                       "Decay LF/HF",
                                                       NormalisableRange<float> (0.0f, 10.0f, 0.01f),
                                                       5.0f));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "stereo_mode", versionHint },
                                                       "Stereo Mode",
                                                       StringArray { "Left/Right", "Mid/Side" },
                                                       0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "stereo_link", versionHint },
                                                       "Stereo Link",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                                       100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "balance", versionHint },
                                                       "Balance",
                                                       NormalisableRange<float> (0.0f, 200.0f, 0.1f),
                                                       100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", versionHint },
                                                       "Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                                       100.0f,
                                                       AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "trim", versionHint },
                                                       "Trim",
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.1f),
                                                       0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "delta", versionHint },
                                                      "Delta",
                                                      false));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "bypass", versionHint },
                                                      "Bypass",
                                                      false));

    return layout;
}

void PluginProcessor::cacheParameterPointers()
{
    modeParam = apvts.getRawParameterValue ("mode");
    depthParam = apvts.getRawParameterValue ("depth");
    sensitivityParam = apvts.getRawParameterValue ("sensitivity");
    sharpnessParam = apvts.getRawParameterValue ("sharpness");
    decayParam = apvts.getRawParameterValue ("decay");
    decayLfHfParam = apvts.getRawParameterValue ("decay_lfhf");
    stereoModeParam = apvts.getRawParameterValue ("stereo_mode");
    stereoLinkParam = apvts.getRawParameterValue ("stereo_link");
    balanceParam = apvts.getRawParameterValue ("balance");
    mixParam = apvts.getRawParameterValue ("mix");
    trimParam = apvts.getRawParameterValue ("trim");
    deltaParam = apvts.getRawParameterValue ("delta");
    bypassParam = apvts.getRawParameterValue ("bypass");

    jassert (modeParam != nullptr && depthParam != nullptr && sensitivityParam != nullptr
             && sharpnessParam != nullptr && decayParam != nullptr && decayLfHfParam != nullptr
             && stereoModeParam != nullptr && stereoLinkParam != nullptr && balanceParam != nullptr
             && mixParam != nullptr && trimParam != nullptr && deltaParam != nullptr
             && bypassParam != nullptr);
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PluginProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PluginProcessor::getTailLengthSeconds() const
{
    // The STFT only delays the signal, it does not ring beyond that.
    return getSampleRate() > 0.0 ? (double) transientShaper.getLatencySamples() / getSampleRate() : 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    // Steinberg's VST3 validator fails plugins whose single default program has no name
    return "Default";
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    transientShaper.prepare (sampleRate);
    setLatencySamples (transientShaper.getLatencySamples());
}

void PluginProcessor::releaseResources()
{
    // Playback has stopped, so dropping the analysis history here is safe (message thread).
    transientShaper.reset();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    TransientShaperParameters parameters;

    parameters.boost = modeParam->load() > 0.5f;
    parameters.depth = depthParam->load() * 0.01f;
    parameters.sensitivity = sensitivityParam->load() * 0.01f;
    parameters.sharpness = sharpnessParam->load() * 0.01f;
    parameters.decay = decayParam->load();
    parameters.decayLfHf = decayLfHfParam->load() * 0.1f;
    parameters.midSide = stereoModeParam->load() > 0.5f;
    parameters.stereoLink = stereoLinkParam->load() * 0.01f;
    parameters.balance = (balanceParam->load() - 100.0f) * 0.01f;
    parameters.mix = mixParam->load() * 0.01f;
    parameters.trimDb = trimParam->load();
    parameters.delta = deltaParam->load() > 0.5f;
    parameters.bypass = bypassParam->load() > 0.5f;

    transientShaper.process (buffer, parameters);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    const std::unique_ptr<juce::XmlElement> xml (state.createXml());

    copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
