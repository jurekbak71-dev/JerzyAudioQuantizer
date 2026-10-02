#pragma once
#include <JuceHeader.h>
#include <cmath>

class QuantizerEngine
{
public:
    enum class Grid
    {
        quarter = 0,
        eighth,
        sixteenth,
        thirtySecond,
        eighthTriplet,
        sixteenthTriplet
    };

    struct Result
    {
        bool valid = false;
        double transientPpq = 0.0;
        double targetPpq = 0.0;
        int correctionSamples = 0;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = juce::jmax (1.0, newSampleRate);
    }

    static double gridStepPpq (Grid grid)
    {
        switch (grid)
        {
            case Grid::quarter:            return 1.0;
            case Grid::eighth:             return 0.5;
            case Grid::sixteenth:          return 0.25;
            case Grid::thirtySecond:       return 0.125;
            case Grid::eighthTriplet:      return 1.0 / 3.0;
            case Grid::sixteenthTriplet:   return 1.0 / 6.0;
        }

        return 0.25;
    }

    Result quantize (double blockStartPpq,
                     int transientSampleOffset,
                     double bpm,
                     Grid grid,
                     float strength01,
                     float windowMs,
                     float swing01) const
    {
        Result r;

        if (bpm <= 1.0 || sampleRate <= 1.0)
            return r;

        const double samplesPerQuarter = sampleRate * 60.0 / bpm;
        const double transientPpq = blockStartPpq
                                  + static_cast<double> (transientSampleOffset) / samplesPerQuarter;

        const double step = gridStepPpq (grid);
        const double rawIndex = transientPpq / step;
        const auto nearestIndex = static_cast<long long> (std::llround (rawIndex));

        double targetPpq = static_cast<double> (nearestIndex) * step;

        if (swing01 > 0.001f && (nearestIndex & 1LL) != 0)
        {
            const double swingFraction = juce::jmap (static_cast<double> (swing01),
                                                     0.0, 1.0,
                                                     0.0, 0.333333333333);
            targetPpq += step * swingFraction;
        }

        const double deltaPpq = targetPpq - transientPpq;
        const double deltaSamples = deltaPpq * samplesPerQuarter;
        const double windowSamples = 0.001 * static_cast<double> (windowMs) * sampleRate;

        if (std::abs (deltaSamples) > windowSamples)
            return r;

        const double corrected = deltaSamples
                               * juce::jlimit (0.0, 1.0, static_cast<double> (strength01));

        r.valid = true;
        r.transientPpq = transientPpq;
        r.targetPpq = targetPpq;
        r.correctionSamples = static_cast<int> (std::llround (corrected));
        return r;
    }

private:
    double sampleRate = 44100.0;
};
