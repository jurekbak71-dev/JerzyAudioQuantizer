#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

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
        sixteenthTriplet,
        shuffleEighth,
        shuffleSixteenth
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
        history.fill(0.0);
        historyCount = 0;
        historyWrite = 0;
    }

    static double gridStepPpq(Grid grid) noexcept
    {
        switch (grid)
        {
            case Grid::quarter:          return 1.0;
            case Grid::eighth:
            case Grid::shuffleEighth:    return 0.5;
            case Grid::sixteenth:
            case Grid::shuffleSixteenth: return 0.25;
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

        const Grid grid = requestedGrid == Grid::automatic
                        ? chooseAutomaticGrid(transientPpq)
                        : requestedGrid;

        const double step = gridStepPpq(grid);
        const auto nearestIndex = static_cast<long long>(std::llround(transientPpq / step));
        double targetPpq = static_cast<double>(nearestIndex) * step;

        double swingFraction = 0.0;
        if (grid == Grid::shuffleEighth || grid == Grid::shuffleSixteenth)
            swingFraction = 1.0 / 3.0;
        else
            swingFraction = juce::jmap(static_cast<double>(swing01), 0.0, 1.0, 0.0, 1.0 / 3.0);

        if (swingFraction > 0.0001 && (nearestIndex & 1LL) != 0)
            targetPpq += step * swingFraction;

        const double deltaSamples = (targetPpq - transientPpq) * samplesPerQuarter;
        const double windowSamples = 0.001 * static_cast<double>(windowMs) * sampleRate;

        if (std::abs(deltaSamples) > windowSamples)
            return r;

        const float proximity = 1.0f - juce::jlimit(0.0f, 1.0f,
            static_cast<float>(std::abs(deltaSamples) / juce::jmax(1.0, windowSamples)));

        r.timingConfidence = juce::jlimit(0.0f, 1.0f,
            detectorConfidence * 0.68f + proximity * 0.32f);

        if (r.timingConfidence < 0.50f)
            return r;

        const double corrected = deltaSamples
            * juce::jlimit(0.0, 1.0, static_cast<double>(strength01));

        r.valid = true;
        r.transientPpq = transientPpq;
        r.targetPpq = targetPpq;
        r.correctionSamples = static_cast<int>(std::llround(corrected));
        r.selectedGrid = grid;

        if (lastAcceptedPpq >= 0.0)
        {
            const double ioi = transientPpq - lastAcceptedPpq;
            if (ioi > 0.02 && ioi < 8.0)
            {
                history[static_cast<size_t>(historyWrite)] = ioi;
                historyWrite = (historyWrite + 1) % static_cast<int>(history.size());
                historyCount = juce::jmin(historyCount + 1, static_cast<int>(history.size()));
            }
        }

        lastAcceptedPpq = transientPpq;
        preferredGrid = grid;
        return r;
    }

private:
    Grid chooseAutomaticGrid(double transientPpq) const noexcept
    {
        constexpr std::array<Grid, 7> candidates {
            Grid::eighth,
            Grid::sixteenth,
            Grid::thirtySecond,
            Grid::eighthTriplet,
            Grid::sixteenthTriplet,
            Grid::shuffleEighth,
            Grid::shuffleSixteenth
        };

        double bestScore = 1.0e9;
        Grid best = preferredGrid;

        for (const auto g : candidates)
        {
            const double step = gridStepPpq(g);
            const double nearest = std::round(transientPpq / step) * step;
            double score = std::abs(transientPpq - nearest) / step;

            if (historyCount > 0)
            {
                double intervalScore = 0.0;
                for (int i = 0; i < historyCount; ++i)
                {
                    const double ioi = history[static_cast<size_t>(i)];
                    const double cells = juce::jmax(1.0, std::round(ioi / step));
                    intervalScore += std::abs(ioi - cells * step) / step;
                }
                intervalScore /= static_cast<double>(historyCount);
                score = 0.42 * score + 0.58 * intervalScore;
            }

            if (g == preferredGrid)
                score *= 0.84;

            if (g == Grid::thirtySecond)
                score *= 1.16;

            if (g == Grid::shuffleEighth || g == Grid::shuffleSixteenth)
                score *= 1.05;

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

    std::array<double, 8> history {};
    int historyCount = 0;
    int historyWrite = 0;
};
