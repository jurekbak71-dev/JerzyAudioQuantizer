#pragma once
#include <JuceHeader.h>
#include <cmath>

class TransientDetector
{
public:
    void prepare (double newSampleRate)
    {
        sampleRate = juce::jmax (1.0, newSampleRate);
        reset();
        updateCoefficients();
    }

    void reset()
    {
        hpX1 = hpY1 = 0.0f;
        fastEnv = slowEnv = 0.0f;
        refractorySamplesRemaining = 0;
    }

    void setSensitivity (float value01)
    {
        sensitivity = juce::jlimit (0.0f, 1.0f, value01);
    }

    void setThresholdDb (float db)
    {
        thresholdLinear = juce::Decibels::decibelsToGain (db);
    }

    bool processSample (float monoSample)
    {
        constexpr float hpHz = 90.0f;
        const float rc = 1.0f / (juce::MathConstants<float>::twoPi * hpHz);
        const float dt = 1.0f / static_cast<float> (sampleRate);
        const float alpha = rc / (rc + dt);

        const float hp = alpha * (hpY1 + monoSample - hpX1);
        hpX1 = monoSample;
        hpY1 = hp;

        const float x = std::abs (hp);

        fastEnv += fastCoeff * (x - fastEnv);
        slowEnv += slowCoeff * (x - slowEnv);

        if (refractorySamplesRemaining > 0)
        {
            --refractorySamplesRemaining;
            return false;
        }

        const float onsetRatio = juce::jmap (sensitivity, 0.0f, 1.0f, 3.8f, 1.35f);
        const float novelty = fastEnv / juce::jmax (slowEnv, 1.0e-6f);

        if (fastEnv >= thresholdLinear && novelty >= onsetRatio)
        {
            refractorySamplesRemaining = static_cast<int> (0.035 * sampleRate);
            return true;
        }

        return false;
    }

private:
    void updateCoefficients()
    {
        const auto coeffForMs = [this] (double ms)
        {
            return static_cast<float> (1.0 - std::exp (-1.0 / (0.001 * ms * sampleRate)));
        };

        fastCoeff = coeffForMs (2.0);
        slowCoeff = coeffForMs (45.0);
    }

    double sampleRate = 44100.0;

    float sensitivity = 0.65f;
    float thresholdLinear = juce::Decibels::decibelsToGain (-42.0f);

    float hpX1 = 0.0f, hpY1 = 0.0f;
    float fastEnv = 0.0f, slowEnv = 0.0f;
    float fastCoeff = 0.0f, slowCoeff = 0.0f;

    int refractorySamplesRemaining = 0;
};
