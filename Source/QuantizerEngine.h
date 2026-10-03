#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <array>

class QuantizerEngine
{
public:
    enum class Grid
    {
        automatic = 0,
        quarter,
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
        Grid selectedGrid = Grid::sixteenth;
        float timingConfidence = 0.0f;
    };

    void prepare(double sr)
    {
        sampleRate = juce::jmax(1.0, sr);
        lastAcceptedPpq = -1.0;
        preferredGrid = Grid::sixteenth;
    }

    static double gridStepPpq(Grid grid)
    {
        switch (grid)
        {
            case Grid::quarter:          return 1.0;
            case Grid::eighth:           return 0.5;
            case Grid::sixteenth:        return 0.25;
            case Grid::thirtySecond:     return 0.125;
            case Grid::eighthTriplet:    return 1.0 / 3.0;
            case Grid::sixteenthTriplet: return 1.0 / 6.0;
            default:                     return 0.25;
        }
    }

    Result quantize(double blockStartPpq,
                    int transientSampleOffset,
                    double bpm,
                    Grid requestedGrid,
                    float strength01,
                    float windowMs,
                    float swing01,
                    float detectorConfidence)
    {
        Result r;
        if (bpm <= 1.0 || sampleRate <= 1.0)
            return r;

        const double samplesPerQuarter = sampleRate * 60.0 / bpm;
        const double transientPpq = blockStartPpq
                                  + static_cast<double>(transientSampleOffset) / samplesPerQuarter;

        Grid grid = requestedGrid == Grid::automatic
                  ? chooseAutomaticGrid(transientPpq)
                  : requestedGrid;

        const double step = gridStepPpq(grid);
        const auto nearestIndex = static_cast<long long>(std::llround(transientPpq / step));
        double targetPpq = static_cast<double>(nearestIndex) * step;

        if (swing01 > 0.001f && (nearestIndex & 1LL) != 0)
        {
            const double swingFraction = juce::jmap(static_cast<double>(swing01),
                                                    0.0, 1.0,
                                                    0.0, 0.333333333333);
            targetPpq += step * swingFraction;
        }

        const double deltaSamples = (targetPpq - transientPpq) * samplesPerQuarter;
        const double windowSamples = 0.001 * static_cast<double>(windowMs) * sampleRate;

        if (std::abs(deltaSamples) > windowSamples)
            return r;

        const float proximity = 1.0f - juce::jlimit(0.0f, 1.0f,
            static_cast<float>(std::abs(deltaSamples) / juce::jmax(1.0, windowSamples)));

        r.timingConfidence = juce::jlimit(0.0f, 1.0f,
                                         detectorConfidence * 0.70f + proximity * 0.30f);

        // Low-confidence attacks are deliberately left alone.
        if (r.timingConfidence < 0.48f)
            return r;

        const double corrected = deltaSamples
                               * juce::jlimit(0.0, 1.0, static_cast<double>(strength01));

        r.valid = true;
        r.transientPpq = transientPpq;
        r.targetPpq = targetPpq;
        r.correctionSamples = static_cast<int>(std::llround(corrected));
        r.selectedGrid = grid;

        lastAcceptedPpq = transientPpq;
        preferredGrid = grid;
        return r;
    }

private:
    Grid chooseAutomaticGrid(double transientPpq)
    {
        constexpr std::array<Grid, 5> candidates {
            Grid::eighth,
            Grid::sixteenth,
            Grid::thirtySecond,
            Grid::eighthTriplet,
            Grid::sixteenthTriplet
        };

        double bestScore = 1.0e9;
        Grid best = preferredGrid;

        for (auto g : candidates)
        {
            const double step = gridStepPpq(g);
            const double nearest = std::round(transientPpq / step) * step;
            double score = std::abs(transientPpq - nearest) / step;

            if (lastAcceptedPpq >= 0.0)
            {
                const double ioi = transientPpq - lastAcceptedPpq;
                const double cells = juce::jmax(1.0, std::round(ioi / step));
                const double intervalError = std::abs(ioi - cells * step) / step;
                score = 0.60 * score + 0.40 * intervalError;
            }

            // Hysteresis so AUTO does not jump grids for tiny score differences.
            if (g == preferredGrid)
                score *= 0.88;

            // Slight bias against unnecessarily fine 1/32 snapping.
            if (g == Grid::thirtySecond)
                score *= 1.12;

            if (score < bestScore)
            {
                bestScore = score;
                best = g;
            }
        }

        return best;
    }

    double sampleRate = 44100.0;
    double lastAcceptedPpq = -1.0;
    Grid preferredGrid = Grid::sixteenth;
};
