/*
  ==============================================================================

    Filter.h
    Created: 10 Dec 2024 9:43:30pm
    Author:  David Matthew Welch

  ==============================================================================
*/

#pragma once
#include "Interfaces.h"
#include <cstdint>

const double GUI_Q_MIN = 1.0;
const double GUI_Q_MAX = 10.0;
const double KORG35_Q_SLOPE = (2.0 - 0.707) / (GUI_Q_MAX - GUI_Q_MIN);

struct VA1Coeffs
{
    // --- filter coefficients
    double alpha = 0.0;            ///< alpha is (wcT/2)
    double beta = 1.0;            ///< beta value, not used
};

const int KORG_SUBFILTERS = 3;
enum { FLT1, FLT2, FLT3, FLT4 };

class VA1Filter : public IFilterBase
    {
    public:
        // --- constructor/destructor
        VA1Filter();
        virtual ~VA1Filter() {}

        // --- these match SynthModule names
        virtual bool reset(double _sampleRate);
        virtual bool update();
        virtual FilterOutput* process(double xn);
        virtual void setFilterParams(double _fc, double _Q);

        // --- set coeffs directly, bypassing coeff calculation
        void setAlpha(double _alpha) { coeffs.alpha = _alpha; }
        void setBeta(double _beta) { coeffs.beta = _beta; }
        void setCoeffs(VA1Coeffs& _coeffs) {
            coeffs = _coeffs;
        }

        void copyCoeffs(VA1Filter& destination) {
            destination.setCoeffs(coeffs);
        }
        
        // --- added for MOOG & K35, need access to this output value, scaled by beta
        double getFBOutput() { return coeffs.beta * sn; }

    protected:
        FilterOutput output;
        double sampleRate = 44100.0;                ///< current sample rate
        double halfSamplePeriod = 1.0;
        double fc = 0.0;

        // --- state storage
        double sn = 0.0;                        ///< state variables

        // --- filter coefficients
        VA1Coeffs coeffs;
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
