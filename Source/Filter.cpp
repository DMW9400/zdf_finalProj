/*
  ==============================================================================

    Filter.cpp
    Created: 10 Dec 2024 10:24:28pm
    Author:  David Matthew Welch

  ==============================================================================
*/

#include "Filter.h"
#include <cmath>

VAKorg35Filter::VAKorg35Filter() {}


        /** reset members to initialized state */
    bool VAKorg35Filter::reset(double _sampleRate)
    {
        sampleRate = _sampleRate;
        halfSamplePeriod = 1.0 / (2.0 * sampleRate);

        for (uint32_t i = 0; i < KORG_SUBFILTERS; i++)
        {
            lpfVAFilters[i].reset(_sampleRate);
            hpfVAFilters[i].reset(_sampleRate);
        }

        output.clearData();
        return true;
    }

    void VAKorg35Filter::setFilterParams(double _fc, double _Q)
    {
        // --- use mapping function for Q -> K
        mapDoubleValue(_Q, 1.0, 0.707, KORG35_Q_SLOPE);

        if (fc != _fc || coeffs.K != _Q)
        {
            fc = _fc;
            coeffs.K = _Q;
            update();
        }
    }

    bool VAKorg35Filter::update()
    {
        coeffs.g = tan(kTwoPi*fc*halfSamplePeriod); // (2.0 / T)*tan(wd*T / 2.0);
        coeffs.alpha = coeffs.g / (1.0 + coeffs.g);

        // --- alpha0 same for LPF, HPF
        coeffs.alpha0 = 1.0 / (1.0 - coeffs.K*coeffs.alpha + coeffs.K*coeffs.alpha*coeffs.alpha);

        // --- three sync-tuned filters
        for (uint32_t i = 0; i < KORG_SUBFILTERS; i++)
        {
            lpfVAFilters[i].setAlpha(coeffs.alpha);
            hpfVAFilters[i].setAlpha(coeffs.alpha);
        }

        // --- set filter beta values
        lpfVAFilters[FLT2].setBeta((coeffs.K * (1.0 - coeffs.alpha)) / (1.0 + coeffs.g));
        lpfVAFilters[FLT3].setBeta(-1.0 / (1.0 + coeffs.g));

        hpfVAFilters[FLT2].setBeta(-coeffs.alpha / (1.0 + coeffs.g));
        hpfVAFilters[FLT3].setBeta(1.0 / (1.0 + coeffs.g));

        return true;
    }

    FilterOutput* VAKorg35Filter::process(double xn)
    {
        // --- lowpass
        // --- process input through LPF1
        FilterOutput* tempOut;
        tempOut = lpfVAFilters[FLT1].process(xn);

        // --- form S35
        double S35 = lpfVAFilters[FLT2].getFBOutput() + lpfVAFilters[FLT3].getFBOutput();

        // --- calculate u
        double u = coeffs.alpha0*(tempOut->filter[ANM_LPF1] + S35);

        // --- feed it to LPF2
        tempOut = lpfVAFilters[FLT2].process(u);

        // --- output = LPF*K
        output.filter[LPF2] = tempOut->filter[LPF1] * coeffs.K;
        output.filter[ANM_LPF2] = tempOut->filter[ANM_LPF1] * coeffs.K;

        // --- feed output to HPF, no need to gather it's output
        lpfVAFilters[FLT3].process(output.filter[LPF2]);

        // --- HIGHPASS:
        // --- process input through HPF1
        tempOut = hpfVAFilters[FLT1].process(xn);

        // --- then: form feedback and feed forward values (read before write)
        S35 = hpfVAFilters[FLT2].getFBOutput() + hpfVAFilters[FLT3].getFBOutput();

        // --- calculate u
        u = coeffs.alpha0*(tempOut->filter[HPF1] + S35);

        //---  form HPF output
        output.filter[HPF2] = coeffs.K*u;

        // --- process through feedback path
        tempOut = hpfVAFilters[FLT2].process(output.filter[HPF2]);

        // --- continue to LPF, no need to gather it's output
        hpfVAFilters[FLT3].process(tempOut->filter[HPF1]);

        // auto-normalize
        if (coeffs.K > 0)
        {
            output.filter[ANM_LPF2] *= 1.0 / coeffs.K;
            output.filter[LPF2] *= 1.0 / coeffs.K;
            output.filter[HPF2] *= 1.0 / coeffs.K;
        }

        return &output;
    }
