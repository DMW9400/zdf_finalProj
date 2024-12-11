/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

//PluginProcessor.h

#pragma once

#include <JuceHeader.h>
#include <Interfaces.h>
#include <Filter.h>

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
    double wc = 2.0 * M_PI * 1000.0; // cutoff in rad/s
    double nonlinearParam = 0.5;     // diode parameter

    double vPrev = 0.0;       // previous state
    double xPrev = 0.0;       // previous input

    void solveState(double x);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ZDFAudioProcessor)
};

class VAKorg35Filter : public IFilterBase
    {
    public:
        // --- constructor/destructor
        VAKorg35Filter();
        virtual ~VAKorg35Filter() {}

        // --- these match SynthModule names
        virtual bool reset(double _sampleRate);
        virtual bool update();
        virtual FilterOutput* process(double xn);
        virtual void setFilterParams(double _fc, double _Q);
        
        struct VAKorg35Coeffs
        {
            // --- filter coefficients
            double K = 1.0;            ///< beta value, not used
            double alpha = 0.0;            ///< alpha is (wcT/2)
            double alpha0 = 1.0;            ///< beta value, not used
            double g = 1.0;            ///< beta value, not used
        };
        // --- set coeffs directly, bypassing coeff calculation
        void setCoeffs(VAKorg35Coeffs& _coeffs) {
            coeffs = _coeffs;

            // --- three sync-tuned filters
            for (uint32_t i = 0; i < KORG_SUBFILTERS; i++)
            {
                lpfVAFilters[i].setAlpha(coeffs.alpha);
                hpfVAFilters[i].setAlpha(coeffs.alpha);
            }

            // --- set filter beta values
            double deno = 1.0 + coeffs.g;

            lpfVAFilters[FLT2].setBeta((coeffs.K * (1.0 - coeffs.alpha)) / deno);
            lpfVAFilters[FLT3].setBeta(-1.0 / deno);

            hpfVAFilters[FLT2].setBeta(-coeffs.alpha / deno);
            hpfVAFilters[FLT3].setBeta(1.0 / deno);
        //    hpfVAFilters[FLT3].setBeta(lpfVAFilters[FLT3].getBeta);
        }

        void copyCoeffs(VAKorg35Filter& destination) {
            destination.setCoeffs(coeffs);
        }

    protected:
        FilterOutput output;
        VA1Filter lpfVAFilters[KORG_SUBFILTERS];
        VA1Filter hpfVAFilters[KORG_SUBFILTERS];
        double sampleRate = 44100.0;                ///< current sample rate
        double halfSamplePeriod = 1.0;
        double fc = 0.0;

        // --- filter coefficients
        VAKorg35Coeffs coeffs;

        //double K = 0.0;
        //double alpha = 0.0;            ///< alpha is (wcT/2)
        //double alpha0 = 0.0;        ///< input scalar, correct delay-free loop
    };
