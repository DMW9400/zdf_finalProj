/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

//PluginProcessor.cpp

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ZDFAudioProcessor::ZDFAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "PARAMETERS", {
         std::make_unique<juce::AudioParameterFloat>("cutoff", "Cutoff", 20.0f, 20000.0f, 1000.0f),
         std::make_unique<juce::AudioParameterFloat>("resonance", "Resonance", 0.0f, 1.0f, 0.5f)
                        })
#endif
{
}

ZDFAudioProcessor::~ZDFAudioProcessor()
{
}

//==============================================================================
const juce::String ZDFAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ZDFAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool ZDFAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool ZDFAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double ZDFAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int ZDFAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int ZDFAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ZDFAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String ZDFAudioProcessor::getProgramName (int index)
{
    return {};
}

void ZDFAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void ZDFAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sampleRate = sampleRate;
    wc = 2.0 * M_PI * (*apvts.getRawParameterValue("cutoff"));
    nonlinearParam = *apvts.getRawParameterValue("nonlinear");

    vPrev = 0.0;
    xPrev = 0.0;
}

void ZDFAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool ZDFAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
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
#endif

void ZDFAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
//    juce::ScopedNoDenormals noDenormals;
//    auto totalNumInputChannels  = getTotalNumInputChannels();
//    auto totalNumOutputChannels = getTotalNumOutputChannels();
//
//    // In case we have more outputs than inputs, this code clears any output
//    // channels that didn't contain input data, (because these aren't
//    // guaranteed to be empty - they may contain garbage).
//    // This is here to avoid people getting screaming feedback
//    // when they first compile a plugin, but obviously you don't need to keep
//    // this code if your algorithm always overwrites all the output channels.
//    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
//        buffer.clear (i, 0, buffer.getNumSamples());
//
//    // This is the place where you'd normally do the guts of your plugin's
//    // audio processing...
//    // Make sure to reset the state if your inner loop is processing
//    // the samples and the outer loop is handling the channels.
//    // Alternatively, you can process the samples with the channels
//    // interleaved by keeping the same state.
//    for (int channel = 0; channel < totalNumInputChannels; ++channel)
//    {
//        auto* channelData = buffer.getWritePointer (channel);
//
//        // ..do something to the data...
//    }
    wc = 2.0 * M_PI * (*apvts.getRawParameterValue("cutoff"));
        nonlinearParam = *apvts.getRawParameterValue("nonlinear");

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            float* data = buffer.getWritePointer(channel);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                double x = (double) data[i];
                solveState(x); // solve for v(n)
                
                // The output is our state v(n), which we now have stored in vPrev
                data[i] = (float)vPrev;
                
                xPrev = x;
            }
        }
}

void ZDFAudioProcessor::solveState(double x)
{
    double T = 1.0 / sampleRate;
    double vGuess = vPrev; // initial guess from previous state

    auto diode = [this](double v) {
        return nonlinearParam * (std::exp(v / 0.02) - 1.0);
    };

    auto F = [&](double v) {
        // F(v) = v - vPrev - (T*wc/2)*[(x - v - diode(v)) + (xPrev - vPrev - diode(vPrev))]
        double termNew = (x - v - diode(v));
        double termOld = (xPrev - vPrev - diode(vPrev));
        return v - vPrev - (T * wc / 2.0) * (termNew + termOld);
    };

    auto dFdv = [&](double v) {
        // dF/dv = 1 - (T*wc/2)*[-1 - diode'(v)]
        // diode'(v) = nonlinearParam * exp(v/0.02)*(1/0.02)
        double diodeDeriv = nonlinearParam * std::exp(v / 0.02) / 0.02;
        return 1.0 - (T * wc / 2.0)*(-1.0 - diodeDeriv);
    };

    // Newton-Raphson
    const int maxIterations = 5;
    const double tol = 1e-9;

    for (int iter = 0; iter < maxIterations; ++iter)
    {
        double val = F(vGuess);
        double deriv = dFdv(vGuess);
        if (std::abs(deriv) < 1e-14)
            break; // avoid division by zero

        double delta = val / deriv;
        vGuess -= delta;
        if (std::abs(delta) < tol)
            break;
    }

    vPrev = vGuess;
}

juce::AudioProcessorEditor* ZDFAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
bool ZDFAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

//juce::AudioProcessorEditor* ZDFAudioProcessor::createEditor()
//{
//    return new ZDFAudioProcessorEditor (*this);
//}

//==============================================================================
void ZDFAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void ZDFAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZDFAudioProcessor();
}
