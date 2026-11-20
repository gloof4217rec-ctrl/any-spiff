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


    // --- Sensitivity graph: three bands plus lo/hi cuts. ---
    const auto freqRange = [] { return NormalisableRange<float> (20.0f, 20000.0f, 1.0f, 0.3f); };
    const auto gainRange = [] { return NormalisableRange<float> (-12.0f, 12.0f, 0.1f); };
    const auto qRange = [] { return NormalisableRange<float> (0.1f, 10.0f, 0.01f); };

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "band_listen", versionHint },
                                                       "Band Listen",
                                                       false));

    const StringArray bandTypes { "Parametric", "Low shelf", "High shelf", "Tilt shelf" };

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "band1_freq", versionHint }, "Band 1 Freq", freqRange(), 100.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "band1_gain", versionHint }, "Band 1 Gain", gainRange(), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "band1_q", versionHint }, "Band 1 Q", qRange(), 0.7f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "band1_type", versionHint }, "Band 1 Type", bandTypes, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "band1_on", versionHint }, "Band 1 On", true));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "band2_freq", versionHint }, "Band 2 Freq", freqRange(), 1000.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "band2_gain", versionHint }, "Band 2 Gain", gainRange(), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "band2_q", versionHint }, "Band 2 Q", qRange(), 0.7f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "band2_type", versionHint }, "Band 2 Type", bandTypes, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "band2_on", versionHint }, "Band 2 On", true));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "band3_freq", versionHint }, "Band 3 Freq", freqRange(), 6000.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "band3_gain", versionHint }, "Band 3 Gain", gainRange(), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "band3_q", versionHint }, "Band 3 Q", qRange(), 0.7f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "band3_type", versionHint }, "Band 3 Type", bandTypes, 0));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "band3_on", versionHint }, "Band 3 On", true));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "locut_freq", versionHint },
                                                       "Lo Cut Freq",
                                                       NormalisableRange<float> (20.0f, 2000.0f, 1.0f, 0.3f),
                                                       20.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "locut_slope", versionHint },
                                                        "Lo Cut Slope",
                                                        StringArray { "Off", "12 dB/oct", "24 dB/oct", "48 dB/oct" },
                                                        0));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hicut_freq", versionHint },
                                                       "Hi Cut Freq",
                                                       NormalisableRange<float> (200.0f, 20000.0f, 1.0f, 0.3f),
                                                       20000.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "hicut_slope", versionHint },
                                                        "Hi Cut Slope",
                                                        StringArray { "Off", "12 dB/oct", "24 dB/oct", "48 dB/oct" },
                                                        0));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "resolution", versionHint },
                                                        "Resolution",
                                                        StringArray { "Low", "Medium", "High" },
                                                        2));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "oversample", versionHint },
                                                        "Oversample",
                                                        StringArray { "Off", "2x", "4x", "8x" },
                                                        0));

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
    resolutionParam = apvts.getRawParameterValue ("resolution");
    oversampleParam = apvts.getRawParameterValue ("oversample");

    bandListenParam = apvts.getRawParameterValue ("band_listen");
    band1FreqParam = apvts.getRawParameterValue ("band1_freq");
    band1GainParam = apvts.getRawParameterValue ("band1_gain");
    band1QParam = apvts.getRawParameterValue ("band1_q");
    band1TypeParam = apvts.getRawParameterValue ("band1_type");
    band1OnParam = apvts.getRawParameterValue ("band1_on");
    band2FreqParam = apvts.getRawParameterValue ("band2_freq");
    band2GainParam = apvts.getRawParameterValue ("band2_gain");
    band2QParam = apvts.getRawParameterValue ("band2_q");
    band2TypeParam = apvts.getRawParameterValue ("band2_type");
    band2OnParam = apvts.getRawParameterValue ("band2_on");
    band3FreqParam = apvts.getRawParameterValue ("band3_freq");
    band3GainParam = apvts.getRawParameterValue ("band3_gain");
    band3QParam = apvts.getRawParameterValue ("band3_q");
    band3TypeParam = apvts.getRawParameterValue ("band3_type");
    band3OnParam = apvts.getRawParameterValue ("band3_on");
    loCutFreqParam = apvts.getRawParameterValue ("locut_freq");
    loCutSlopeParam = apvts.getRawParameterValue ("locut_slope");
    hiCutFreqParam = apvts.getRawParameterValue ("hicut_freq");
    hiCutSlopeParam = apvts.getRawParameterValue ("hicut_slope");

    jassert (modeParam != nullptr && depthParam != nullptr && sensitivityParam != nullptr
             && sharpnessParam != nullptr && decayParam != nullptr && decayLfHfParam != nullptr
             && stereoModeParam != nullptr && stereoLinkParam != nullptr && balanceParam != nullptr
             && mixParam != nullptr && trimParam != nullptr && deltaParam != nullptr
             && bypassParam != nullptr
             && resolutionParam != nullptr
             && oversampleParam != nullptr
             && bandListenParam != nullptr && band1FreqParam != nullptr && band1GainParam != nullptr
             && band1QParam != nullptr && band1TypeParam != nullptr && band1OnParam != nullptr
             && band2FreqParam != nullptr && band2GainParam != nullptr && band2QParam != nullptr
             && band2TypeParam != nullptr && band2OnParam != nullptr
             && band3FreqParam != nullptr && band3GainParam != nullptr && band3QParam != nullptr
             && band3TypeParam != nullptr && band3OnParam != nullptr
             && loCutFreqParam != nullptr && loCutSlopeParam != nullptr
             && hiCutFreqParam != nullptr && hiCutSlopeParam != nullptr);
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
    lastReportedLatency = transientShaper.getLatencySamples();
    setLatencySamples (lastReportedLatency);
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

    const auto resolutionIndex = juce::jlimit (0, 2, (int) resolutionParam->load());
    parameters.resolutionHops = 1 << (2 - resolutionIndex);   // low=4, medium=2, high=1

    const auto oversampleIndex = juce::jlimit (0, 3, (int) oversampleParam->load());
    parameters.oversamplingFactor = 1 << oversampleIndex;     // off=1, 2x=2, 4x=4, 8x=8

    parameters.bandListen = bandListenParam->load() > 0.5f;
    parameters.listenFrequencyHz = bandListenFrequency.load();
    parameters.listenQ = bandListenQ.load();

    auto& band1 = parameters.curve.bands[0];
    band1.frequencyHz = band1FreqParam->load();
    band1.gainDb = band1GainParam->load();
    band1.q = band1QParam->load();
    band1.type = (int) band1TypeParam->load();
    band1.enabled = band1OnParam->load() > 0.5f;

    auto& band2 = parameters.curve.bands[1];
    band2.frequencyHz = band2FreqParam->load();
    band2.gainDb = band2GainParam->load();
    band2.q = band2QParam->load();
    band2.type = (int) band2TypeParam->load();
    band2.enabled = band2OnParam->load() > 0.5f;

    auto& band3 = parameters.curve.bands[2];
    band3.frequencyHz = band3FreqParam->load();
    band3.gainDb = band3GainParam->load();
    band3.q = band3QParam->load();
    band3.type = (int) band3TypeParam->load();
    band3.enabled = band3OnParam->load() > 0.5f;

    parameters.curve.loCutFrequencyHz = loCutFreqParam->load();
    parameters.curve.loCutSlopeDbPerOctave = (int) loCutSlopeParam->load() * 12;
    parameters.curve.hiCutFrequencyHz = hiCutFreqParam->load();
    parameters.curve.hiCutSlopeDbPerOctave = (int) hiCutSlopeParam->load() * 12;

    transientShaper.process (buffer, parameters);

    // Oversampling changes the latency at runtime, so keep the host in sync.
    const auto latency = transientShaper.getLatencySamples();

    if (latency != lastReportedLatency)
    {
        lastReportedLatency = latency;
        setLatencySamples (latency);
    }
}

void PluginProcessor::beginBandListen (float frequencyHz, float q)
{
    bandListenFrequency.store (frequencyHz);
    bandListenQ.store (q);

    if (auto* param = apvts.getParameter ("band_listen"))
        param->setValueNotifyingHost (1.0f);
}

void PluginProcessor::endBandListen()
{
    if (auto* param = apvts.getParameter ("band_listen"))
        param->setValueNotifyingHost (0.0f);
}

void PluginProcessor::getReductionSnapshot (std::array<float, AdaptiveTransientShaper::numBins>& destination) const
{
    transientShaper.copyReduction (destination);
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
