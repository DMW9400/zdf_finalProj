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
         std::make_unique<juce::AudioParameterFloat>("resonance", "Resonance", 0.0f, 1.0f, 0.5f),
         std::make_unique<juce::AudioParameterFloat>("hpCutoff", "HP Cutoff", 20.0f, 20000.0f, 200.0f),
         std::make_unique<juce::AudioParameterFloat>("drive", "Drive", 0.0f, 2.0f, 0.5f)
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
    for (int i = 0; i < 2; ++i)
    {
        vPrev[i] = 0.0;
        xPrev[i] = 0.0;
        vPrev2[i] = 0.0;
        xPrev2[i] = 0.0;
        vHP[i] = 0.0;
        xHP[i] = 0.0;
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
//    --- Low-Pass Parameters ----
    float currentCutoff = *apvts.getRawParameterValue("cutoff");
    float param = *apvts.getRawParameterValue("resonance");
//    Log scale the Q value for smoother resonance responce
    double Q = std::exp(std::log(100.0)*param); // Q=1 at param=0, Q=100 at param=1
//   Calculate resonance based on the (currently not well-functioning) Q value to better emphasize cutoff freq
    double R = 1.0 - (1.0/(Q));
    R *= 1.8; // scale as needed
//        Convert cutoff frequency to angular frequency - radians per second - the preferred nomenclature of the following filter formulae
    wc = 2.0 * juce::MathConstants<double>::pi * (double)currentCutoff;
//    Determine the sampling period
    double T = 1.0 / sr;
//   Coefficient for trapezoidal integration - essential for the linear equations below
    double a = (T * wc) / 2.0;
    
    // driveParam: how much to push the signal into the tanh saturation
    float driveParam = *apvts.getRawParameterValue("drive");
    
//   ----- High-Pass Parameters -----
    float hpCutoff = *apvts.getRawParameterValue("hpCutoff");
    double wcHP = 2.0 * juce::MathConstants<double>::pi * (double)hpCutoff;
    double aHP = (T * wcHP) / 2.0;

    
//   Get samples from the buffer
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
//    Ensure that we are operating in stereo
    jassert(numChannels <= 2);
//    Loop through the channels to generate current sample per-channel
    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* data = buffer.getWritePointer(channel);
//    Set the LP filter values based on prior states from the channel's most recent sample
        double& vP  = vPrev[channel];
        double& xP  = xPrev[channel];
        double& vP2 = vPrev2[channel];
        double& xP2 = xPrev2[channel];
        
        // HP states
        double& vHpP = vHP[channel];
        double& xHpP = xHP[channel];

        for (int i = 0; i < numSamples; ++i)
        {
            double x = (double)data[i];
        
            // Compute vHP as if it's a low-pass at hpCutoff
            double vHP_next = (vHpP*(1.0 - aHP) + aHP*(x + xHpP)) / (1.0 + aHP);
            // Update HP states
            vHpP = vHP_next;
            xHpP = x;

            // Now produce high-pass output
            double hpOutput = x - vHP_next;
            
            double driveGain = std::pow(10.0, driveParam * 0.5);
            double drivenHP = std::tanh(driveGain * hpOutput);
            

            // Compute E and F - the "right hand sides" of the discretized filter equations
//                This is where the trapezoidal integration comes into play
//                Trapezoidal rule depends on both current and previous inputs
            double E = vP * (1.0 - a) + a * (drivenHP + xP);
            double F = vP2*(1.0 - a) + a*(vP);

            // Solve linear system:
//                These four variables form the coefficient matrix
            double A = 1.0 + a;
            double B = -a*R;
            double C = -a;
            double D = 1.0 + a;
//
            double Det = A*D - B*C;
//              Set current values for the first and second stage integrators
//              Equations solved directly for the current sample -> no one-sample delay feedback loop
            double v1 = (E*D - B*F) / Det;
            double v2 = (A*F - C*E) / Det;
//              The output of the second stage is used for the final output of the filter in this sample
            double secondStage = v2;
            data[i] = (float)secondStage;

            // Update LP states:
            vP = v1;
            vP2 = v2;
            xP = hpOutput;
            xP2 = v1; // firstStage = v1
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
    if (auto xmlState = apvts.copyState().createXml())
    {
        // 2) Convert it to binary and store in destData
        copyXmlToBinary(*xmlState, destData);
    }
}

void ZDFAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // 1) Attempt to parse binary data back into an XML object
    if (auto xmlState = getXmlFromBinary(data, sizeInBytes))
    {
        // 2) Replace our current state tree with the one we just loaded
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ZDFAudioProcessor();
}
