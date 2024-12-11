/*
  ==============================================================================

    Interfaces.h
    Created: 10 Dec 2024 9:43:12pm
    Author:  David Matthew Welch

  ==============================================================================
*/

#pragma once

enum {
    LPF_OUTPUT,
    HPF_OUTPUT,
    NUM_FILTER_OUTPUTS // This will automatically become 2
};

struct FilterOutput
{
    FilterOutput() { clearData(); }
    double filter[NUM_FILTER_OUTPUTS];
    void clearData()
    {
        memset(&filter[0], 0, sizeof(double) * NUM_FILTER_OUTPUTS);
    }
};

class IFilterBase
    {
    public:
        virtual ~IFilterBase() {}
        /**
        \brief
        reset the filter to intialized state; called once at init time and
        again any time the sample rate changes

        \return true if sucessful, false otherwise
        */
        virtual bool reset(double _sampleRate) = 0;

        /**
        \brief
        update the filter due to changes in the GUI controls and/or modulations

        \return true if sucessful, false otherwise
        */
        virtual bool update() = 0;

        /**
        \brief
        Process audio through the filter. Different filters produce different outputs in the FilterOutput's
        array.

        \return FilterOutput structure containing outputs in pre-defined slots
        */
        virtual FilterOutput* process(double xn) = 0;

        /**
        \brief
        Sets the two parameters of all synth filters. You can add more here if you need to.

        \param _fc the center or cutoff frequency of the filter
        \param _Q the quality factor (damping) of the filter
        */
        virtual void setFilterParams(double _fc, double _Q) = 0;
    };
