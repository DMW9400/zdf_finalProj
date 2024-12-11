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
    // Initialize states to 0
    for (int c = 0; c < 2; ++c)
    {
        vPrev[c]  = 0.0;
        xPrev[c]  = 0.0;
        vPrev2[c] = 0.0;
        xPrev2[c] = 0.0;
    }
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
    sr = sampleRate;
//    wc = 2.0 * M_PI * (*apvts.getRawParameterValue("cutoff"));
//    nonlinearParam = *apvts.getRawParameterValue("resonance");
//    
//    smoothedCutoff.reset(sr, 0.05);
//    smoothedCutoff.setCurrentAndTargetValue(*apvts.getRawParameterValue("cutoff"));
//
//    smoothedResonance.reset(sr, 0.05);
//    smoothedResonance.setCurrentAndTargetValue(*apvts.getRawParameterValue("resonance"));
    for (int i = 0; i < 2; ++i)
    {
        vPrev[i] = 0.0;
        xPrev[i] = 0.0;
        vPrev2[i] = 0.0;
        xPrev2[i] = 0.0;
    }
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

    float currentCutoff = *apvts.getRawParameterValue("cutoff");
//     float currentRes    = *apvts.getRawParameterValue("resonance");
//     double R = (double)currentRes;
//     double resonanceBoost = 1.5;  // Try a value > 1.0
//     R *= resonanceBoost;

    float currentRes = *apvts.getRawParameterValue("resonance");
    // Map resonance [0,1] -> Q [0.5,10]
    double Q = 0.5 + currentRes * 9.5;
    double R = 1.0 - (1.0 / (2.0 * Q));
    R = std::min(R, 0.99); // prevent hitting extreme instability

    wc = 2.0 * juce::MathConstants<double>::pi * (double)currentCutoff;
    double T = 1.0 / sr;
    double a = (T * wc) / 2.0;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    jassert(numChannels <= 2); // Ensure we don't exceed array bounds

    for (int channel = 0; channel < numChannels; ++channel)
    {
     float* data = buffer.getWritePointer(channel);
     double& vP  = vPrev[channel];
     double& xP  = xPrev[channel];
     double& vP2 = vPrev2[channel];
     double& xP2 = xPrev2[channel];

     for (int i = 0; i < numSamples; ++i)
     {
         double x = (double)data[i];

         // Setup E, F based on previous states
         // E = vP*(1 - a) + a*(x + xP)
         // F = vP2*(1 - a) + a*(vP)
         // Here careful: vP is the previous v1, so for the second stage input: v1_prev = vP.
         double E = vP*(1.0 - a) + a*(x + xP);
         double F = vP2*(1.0 - a) + a*(vP);

         // Matrix coefficients:
         // [ (1+a)  (-aR) ] [ v1 ] = [ E ]
         // [  -a     (1+a) ] [ v2 ]   [ F ]
         double A = 1.0 + a;
         double B = -a*R;
         double C = -a;
         double D = 1.0 + a;

         double Det = A*D - B*C;
         // = (1+a)*(1+a) - (-aR)*(-a)
         // = (1+a)^2 - a^2*R

         double v1 = (E*D - B*F) / Det;
         double v2 = (A*F - C*E) / Det;

         double firstStage = v1;
         double secondStage = v2;

         data[i] = (float)secondStage;

         // Update states:
         vP = v1;
         vP2 = v2;
         xP = x;
         xP2 = firstStage;
     }
    }
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
