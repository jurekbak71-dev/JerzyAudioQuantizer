#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>

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

        fastCoeff = coeffForMs(1.2);
        midCoeff = coeffForMs(10.0);
        slowCoeff = coeffForMs(85.0);
        noiseCoeff = coeffForMs(650.0);

        // These used to be recomputed with exp() for every audio sample.
        lowCoeffValue = onePoleCoeff(170.0f);
        midBandCoeffValue = onePoleCoeff(2400.0f);
        const float rc = 1.0f / (juce::MathConstants<float>::twoPi * 70.0f);
        const float dt = 1.0f / static_cast<float>(sampleRate);
        hpCoeffValue = rc / (rc + dt);

        minIntervalSamples = static_cast<int>(0.032 * sampleRate);
        reset();
    }

    void reset()
    {
        hpX1 = hpY1 = 0.0f;
        lpLow = lpMid = 0.0f;
        fastEnv = midEnv = slowEnv = noiseFloor = 0.0f;
        previousFast = 0.0f;
        peakNovelty = 0.0f;
        refractory = 0;
    }

    void setSensitivity(float v) noexcept { sensitivity = juce::jlimit(0.0f, 1.0f, v); }
    void setThresholdDb(float db) noexcept { thresholdLinear = juce::Decibels::decibelsToGain(db); }

    Result processSample(float x) noexcept
    {
        Result r;

        // 70 Hz high-pass: removes handling rumble/DC before onset analysis.
        const float hp = hpCoeffValue * (hpY1 + x - hpX1);
        hpX1 = x;
        hpY1 = hp;

        // Cheap three-band decomposition tuned for pick/string attacks.
        lpLow += lowCoeffValue * (hp - lpLow);
        lpMid += midBandCoeffValue * (hp - lpMid);

        const float low = lpLow;
        const float mid = lpMid - lpLow;
        const float high = hp - lpMid;

        const float absLow = std::abs(low);
        const float absMid = std::abs(mid);
        const float absHigh = std::abs(high);
        const float weighted = 0.12f * absLow + 0.58f * absMid + 1.0f * absHigh;

        fastEnv += fastCoeff * (weighted - fastEnv);
        midEnv += midCoeff * (weighted - midEnv);
        slowEnv += slowCoeff * (weighted - slowEnv);

        // The floor follows background noise slowly and is prevented from chasing attacks.
        const float floorTarget = juce::jmin(weighted, slowEnv * 1.35f + 1.0e-7f);
        noiseFloor += noiseCoeff * (floorTarget - noiseFloor);

        const float base = juce::jmax(juce::jmax(slowEnv, noiseFloor * 2.2f), 1.0e-7f);
        const float flux = juce::jmax(0.0f, fastEnv - previousFast) / base;
        const float envelopeNovelty = juce::jmax(0.0f, (fastEnv - midEnv) / base);
        previousFast = fastEnv;

        const float novelty = 0.70f * envelopeNovelty + 0.30f * flux;
        peakNovelty = juce::jmax(novelty, peakNovelty * 0.985f);

        const float levelGate = juce::jmax(thresholdLinear, noiseFloor * 3.2f);
        const float requiredNovelty = juce::jmap(sensitivity, 0.0f, 1.0f, 1.75f, 0.26f);

        const float noveltyScore = juce::jlimit(0.0f, 1.0f,
            novelty / juce::jmax(0.05f, requiredNovelty * 1.8f));
        const float levelScore = juce::jlimit(0.0f, 1.0f,
            fastEnv / juce::jmax(levelGate * 2.6f, 1.0e-7f));
        const float pickBias = juce::jlimit(0.0f, 1.0f,
            (0.6f * absMid + absHigh) / (absLow + absMid + absHigh + 1.0e-7f));
        const float riseScore = juce::jlimit(0.0f, 1.0f, flux / 1.5f);

        r.confidence = juce::jlimit(0.0f, 1.0f,
            0.42f * noveltyScore + 0.24f * levelScore + 0.22f * pickBias + 0.12f * riseScore);
        r.level = fastEnv;
        r.novelty = novelty;

        if (refractory > 0)
        {
            --refractory;
            return r;
        }

        // Require both a fast spectral/envelope change and sufficient absolute level.
        // A second novelty check prevents finger slides and sustain fluctuations from
        // becoming warp markers.
        const bool clearRise = novelty >= requiredNovelty;
        const bool localPeakStrong = novelty >= peakNovelty * 0.72f;

        if (fastEnv >= levelGate && clearRise && localPeakStrong && r.confidence >= 0.42f)
        {
            r.hit = true;
            refractory = minIntervalSamples;
            peakNovelty = novelty;
        }

        return r;
    }

private:
    float coeffForMs(double ms) const noexcept
    {
        return static_cast<float>(1.0 - std::exp(-1.0 / (0.001 * ms * sampleRate)));
    }

    float onePoleCoeff(float hz) const noexcept
    {
        return 1.0f - std::exp(-juce::MathConstants<float>::twoPi * hz
                              / static_cast<float>(sampleRate));
    }

    double sampleRate = 44100.0;
    float sensitivity = 0.65f;
    float thresholdLinear = juce::Decibels::decibelsToGain(-48.0f);

    float hpX1 = 0.0f, hpY1 = 0.0f;
    float lpLow = 0.0f, lpMid = 0.0f;
    float fastEnv = 0.0f, midEnv = 0.0f, slowEnv = 0.0f, noiseFloor = 0.0f;
    float previousFast = 0.0f, peakNovelty = 0.0f;

    float fastCoeff = 0.0f, midCoeff = 0.0f, slowCoeff = 0.0f, noiseCoeff = 0.0f;
    float lowCoeffValue = 0.0f, midBandCoeffValue = 0.0f, hpCoeffValue = 0.0f;

    int refractory = 0;
    int minIntervalSamples = 1400;
};
