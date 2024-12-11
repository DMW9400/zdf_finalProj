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
//    // Smooth parameters
//    smoothedCutoff.setTargetValue(*apvts.getRawParameterValue("cutoff"));
//    smoothedResonance.setTargetValue(*apvts.getRawParameterValue("resonance"));
//
//    const int numSamples = buffer.getNumSamples();
//    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
//    {
//        float* data = buffer.getWritePointer(channel);
//        for (int i = 0; i < numSamples; ++i)
//        {
//            float currentCutoff = smoothedCutoff.getNextValue();
//            float currentRes = smoothedResonance.getNextValue();
//
//            wc = 2.0 * juce::MathConstants<double>::pi * (double)currentCutoff;
//            nonlinearParam = (double)currentRes;
//
//            double x = (double)data[i];
//
//            // First stage
//            solveState(x, vPrev, xPrev);
//            double firstStage = vPrev;
//
//            // Second stage
//            solveState(firstStage, vPrev2, xPrev2);
//            double secondStage = vPrev2;
//
//            data[i] = (float)secondStage;
//
//            // Update previous inputs
//            xPrev = x;
//            xPrev2 = firstStage;
//        }
//    }
    float currentCutoff = *apvts.getRawParameterValue("cutoff");
    wc = 2.0 * juce::MathConstants<double>::pi * (double)currentCutoff;

    double T = 1.0 / sr;
    double a = (T * wc) / 2.0;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* data = buffer.getWritePointer(channel);
        double& vP = vPrev[channel];
        double& xP = xPrev[channel];
        double& vP2 = vPrev2[channel];
        double& xP2 = xPrev2[channel];

        for (int i = 0; i < numSamples; ++i)
        {
            double x = (double)data[i];

            double vNext = (vP*(1.0 - a) + a*(x + xP)) / (1.0 + a);
            double firstStage = vNext;
            vP = vNext;

            double vNext2 = (vP2*(1.0 - a) + a*(firstStage + xP2)) / (1.0 + a);
            double secondStage = vNext2;
            vP2 = vNext2;

            data[i] = (float)secondStage;

            xP = x;
            xP2 = firstStage;
        }
    }
}

//void ZDFAudioProcessor::solveState(double x, double& vCurrent, double xPrevState)
//{
//    double T = 1.0 / sr;
//    double vGuess = vCurrent;
//
//    // Gentler nonlinear function than diode: soft clipping via tanh
//    auto nonlinear = [this](double v)
//    {
//        // Scale the input to tanh for a mild saturation
//        double drive = nonlinearParam * 0;
//        double scaledV = v * .5;
//        return std::tanh(drive * scaledV);
//    };
//
//    // Equation based on trapezoidal integration:
//    // v is the new state, vCurrent is old state
//    // Using a nonlinear function in place of diode
//    auto F = [&](double v)
//    {
//        double termNew = (x - v - nonlinear(v));
//        double termOld = (xPrevState - vCurrent - nonlinear(vCurrent));
//        return (v - vCurrent) - (T * wc / 2.0) * (termNew + termOld);
//    };
//
//    auto dFdv = [&](double v)
//    {
//        // derivative of tanh(drive*v) w.r.t. v is drive * sech(drive*v)^2
//        double drive = nonlinearParam * 0;
//        double sech2 = 1.0 / std::cosh(drive * v);
//        sech2 = sech2 * sech2; // sech^2(x)
//        double nonlinearDeriv = drive * sech2;
//
//        return 1.0 - (T * wc / 2.0)*(-1.0 - nonlinearDeriv);
//    };
//
//    const int maxIterations = 10;
//    const double tol = 1e-9;
//
//    for (int iter = 0; iter < maxIterations; ++iter)
//    {
//        double val = F(vGuess);
//        double deriv = dFdv(vGuess);
//        if (std::abs(deriv) < 1e-14)
//            break; // Avoid division by zero
//
//        double delta = val / deriv;
//        vGuess -= delta;
//        if (std::abs(delta) < tol)
//            break;
//    }
//
//    vCurrent = vGuess;
//}

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
