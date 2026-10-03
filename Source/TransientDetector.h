#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <algorithm>

class TransientDetector
{
public:
    struct Result
    {
        bool hit = false;
        float confidence = 0.0f;
        float level = 0.0f;
        float novelty = 0.0f;
    };

    void prepare(double newSampleRate)
    {
        sampleRate = juce::jmax(1.0, newSampleRate);
        reset();
        fastCoeff = coeffForMs(1.5);
        midCoeff = coeffForMs(12.0);
        slowCoeff = coeffForMs(90.0);
        noiseCoeff = coeffForMs(500.0);
    }

    void reset()
    {
        hpX1 = hpY1 = 0.0f;
        lpLow = lpMid = 0.0f;
        fastEnv = midEnv = slowEnv = noiseFloor = 0.0f;
        refractory = 0;
    }

    void setSensitivity(float v) { sensitivity = juce::jlimit(0.0f, 1.0f, v); }
    void setThresholdDb(float db) { thresholdLinear = juce::Decibels::decibelsToGain(db); }

    Result processSample(float x)
    {
        Result r;

        // Reject DC/rumble.
        constexpr float hpHz = 70.0f;
        const float rc = 1.0f / (juce::MathConstants<float>::twoPi * hpHz);
        const float dt = 1.0f / static_cast<float>(sampleRate);
        const float a = rc / (rc + dt);
        const float hp = a * (hpY1 + x - hpX1);
        hpX1 = x;
        hpY1 = hp;

        // Crude three-band decomposition: useful for distinguishing a pick attack
        // from low-frequency sustain and handling noise without an FFT per sample.
        lpLow += lowCoeff() * (hp - lpLow);
        lpMid += midBandCoeff() * (hp - lpMid);

        const float low = lpLow;
        const float mid = lpMid - lpLow;
        const float high = hp - lpMid;

        const float weighted = 0.15f * std::abs(low)
                             + 0.55f * std::abs(mid)
                             + 1.00f * std::abs(high);

        fastEnv += fastCoeff * (weighted - fastEnv);
        midEnv += midCoeff * (weighted - midEnv);
        slowEnv += slowCoeff * (weighted - slowEnv);

        // Adaptive floor: follows room noise/hiss slowly, but not pick attacks.
        const float floorTarget = juce::jmin(weighted, slowEnv * 1.5f + 1.0e-6f);
        noiseFloor += noiseCoeff * (floorTarget - noiseFloor);

        const float base = std::max(std::max(slowEnv, noiseFloor * 2.0f), 1.0e-6f);
        const float novelty = juce::jmax(0.0f, (fastEnv - midEnv) / base);
        const float levelGate = juce::jmax(thresholdLinear, noiseFloor * 3.0f);

        const float requiredNovelty = juce::jmap(sensitivity, 0.0f, 1.0f, 1.6f, 0.30f);
        const float noveltyScore = juce::jlimit(0.0f, 1.0f, novelty / juce::jmax(0.05f, requiredNovelty * 2.0f));
        const float levelScore = juce::jlimit(0.0f, 1.0f, fastEnv / juce::jmax(levelGate * 3.0f, 1.0e-6f));
        const float attackBias = juce::jlimit(0.0f, 1.0f, std::abs(high) / (std::abs(low) + std::abs(mid) + std::abs(high) + 1.0e-6f));

        r.confidence = juce::jlimit(0.0f, 1.0f,
                                   0.52f * noveltyScore
                                 + 0.28f * levelScore
                                 + 0.20f * attackBias);
        r.level = fastEnv;
        r.novelty = novelty;

        if (refractory > 0)
        {
            --refractory;
            return r;
        }

        if (fastEnv >= levelGate && novelty >= requiredNovelty && r.confidence >= 0.35f)
        {
            r.hit = true;
            refractory = static_cast<int>(0.028 * sampleRate);
        }

        return r;
    }

private:
    float coeffForMs(double ms) const
    {
        return static_cast<float>(1.0 - std::exp(-1.0 / (0.001 * ms * sampleRate)));
    }

    float lowCoeff() const
    {
        const float hz = 180.0f;
        return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * hz / static_cast<float>(sampleRate));
    }

    float midBandCoeff() const
    {
        const float hz = 2200.0f;
        return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * hz / static_cast<float>(sampleRate));
    }

    double sampleRate = 44100.0;
    float sensitivity = 0.65f;
    float thresholdLinear = juce::Decibels::decibelsToGain(-48.0f);

    float hpX1 = 0.0f, hpY1 = 0.0f;
    float lpLow = 0.0f, lpMid = 0.0f;
    float fastEnv = 0.0f, midEnv = 0.0f, slowEnv = 0.0f, noiseFloor = 0.0f;
    float fastCoeff = 0.0f, midCoeff = 0.0f, slowCoeff = 0.0f, noiseCoeff = 0.0f;
    int refractory = 0;
};
