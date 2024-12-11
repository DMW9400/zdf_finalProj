/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

//PluginProcessor.h

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class ZDFAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    ZDFAudioProcessor();
    ~ZDFAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    juce::AudioProcessorValueTreeState apvts;

private:
    double sr = 44100.0;
    double wc = 2.0 * juce::MathConstants<double>::pi * 1000.0;
    double nonlinearParam = 0.5;     // diode parameter

    double vPrev[2] = {0.0, 0.0};
    double xPrev[2] = {0.0, 0.0};

    double vPrev2[2] = {0.0, 0.0};
    double xPrev2[2] = {0.0, 0.0};
//    Smoothed parameters
    juce::SmoothedValue<float> smoothedCutoff;
    juce::SmoothedValue<float> smoothedResonance;

    void solveState(double x, double& vCurrent, double xPrevState);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZDFAudioProcessor)
};
