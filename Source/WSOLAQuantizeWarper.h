#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <limits>
#include <cstdint>

/*
    WSOLA-inspired real-time timing warper.

    This module does NOT continuously vary playback speed. A timing correction is
    realised as a waveform-similarity-aligned splice between two constant-speed
    read positions inside a lookahead ring buffer.

    Consequences:
      - pitch remains unchanged on both sides of the splice,
      - the splice is moved to a waveform-similar position,
      - equal-power overlap-add suppresses clicks,
      - the detected transient remains inside the lookahead buffer while the
        timing correction is prepared.

    It is intentionally optimised for short guitar timing corrections rather
    than large offline stretching ratios.
*/
class WSOLAQuantizeWarper
{
public:
    enum class Quality { live = 0, studio = 1 };

    void prepare (double newSampleRate, int maxBlockSize, int numChannels)
    {
        sampleRate = juce::jmax (1.0, newSampleRate);
        maxBlock = juce::jmax (1, maxBlockSize);

        const int capacity = static_cast<int> (std::ceil (sampleRate * 1.25)) + maxBlock + 64;
        ring.setSize (juce::jmax (1, numChannels), capacity);
        ring.clear();

        writePos = 0;
        totalSamplesWritten = 0;
        initialisedBase = false;
        baseDelaySamples = msToSamples (90.0f);
        activeDelay = targetDelay = oldDelay = static_cast<double> (baseDelaySamples);
        crossfadePos = crossfadeLength = 0;
    }

    void reset()
    {
        ring.clear();
        writePos = 0;
        totalSamplesWritten = 0;
        crossfadePos = crossfadeLength = 0;
        activeDelay = targetDelay = oldDelay = static_cast<double> (baseDelaySamples);
    }

    void setBaseDelayMs (float ms)
    {
        baseDelaySamples = msToSamples (juce::jlimit (10.0f, 350.0f, ms));

        if (! initialisedBase)
        {
            activeDelay = targetDelay = oldDelay = static_cast<double> (baseDelaySamples);
            initialisedBase = true;
        }
    }

    void setTransientPreserveMs (float ms)
    {
        preserveSamples = msToSamples (juce::jlimit (0.0f, 40.0f, ms));
    }

    void setQuality (Quality q) noexcept { quality = q; }

    int getBaseDelaySamples() const noexcept { return baseDelaySamples; }

    void scheduleCorrectionSamples (int correctionSamples, float smoothMs)
    {
        const int minimumDelay = juce::jmax (2, preserveSamples + 2);
        const int requested = baseDelaySamples + correctionSamples;
        const int clamped = juce::jlimit (minimumDelay, ring.getNumSamples() - 8, requested);

        oldDelay = getCurrentEffectiveDelay();

        const int searchRadius = quality == Quality::studio ? msToSamples (8.0f)
                                                            : msToSamples (3.0f);
        const int compareLength = quality == Quality::studio ? msToSamples (6.0f)
                                                             : msToSamples (3.0f);

        targetDelay = findBestDelay (oldDelay,
                                     static_cast<double> (clamped),
                                     searchRadius,
                                     compareLength);

        const float minFadeMs = quality == Quality::studio ? 6.0f : 3.0f;
        const float maxFadeMs = quality == Quality::studio ? 18.0f : 10.0f;
        const float fadeMs = juce::jlimit (minFadeMs, maxFadeMs, smoothMs);

        const int availableBeforeAttack = juce::jmax (16, baseDelaySamples - preserveSamples);
        crossfadeLength = juce::jmin (availableBeforeAttack,
                                      juce::jmax (16, msToSamples (fadeMs)));
        crossfadePos = 0;
    }

    void returnToBase (float smoothMs)
    {
        oldDelay = getCurrentEffectiveDelay();
        targetDelay = static_cast<double> (baseDelaySamples);
        crossfadeLength = juce::jmax (16, msToSamples (juce::jlimit (2.0f, 20.0f, smoothMs)));
        crossfadePos = 0;
    }

    void process (juce::AudioBuffer<float>& buffer)
    {
        const int channels = juce::jmin (buffer.getNumChannels(), ring.getNumChannels());
        const int samples = buffer.getNumSamples();

        for (int i = 0; i < samples; ++i)
        {
            for (int ch = 0; ch < channels; ++ch)
                ring.setSample (ch, writePos, buffer.getSample (ch, i));

            float gainOld = 0.0f;
            float gainNew = 1.0f;

            if (crossfadeLength > 0 && crossfadePos < crossfadeLength)
            {
                const double t = static_cast<double> (crossfadePos)
                               / static_cast<double> (crossfadeLength);
                gainOld = static_cast<float> (std::cos (t * juce::MathConstants<double>::halfPi));
                gainNew = static_cast<float> (std::sin (t * juce::MathConstants<double>::halfPi));
            }

            for (int ch = 0; ch < channels; ++ch)
            {
                const float newer = readInterpolated (ch, targetDelay);

                if (crossfadeLength > 0 && crossfadePos < crossfadeLength)
                {
                    const float older = readInterpolated (ch, oldDelay);
                    buffer.setSample (ch, i, older * gainOld + newer * gainNew);
                }
                else
                {
                    buffer.setSample (ch, i, newer);
                }
            }

            writePos = (writePos + 1) % ring.getNumSamples();
            ++totalSamplesWritten;

            if (crossfadeLength > 0)
            {
                ++crossfadePos;
                if (crossfadePos >= crossfadeLength)
                {
                    activeDelay = targetDelay;
                    oldDelay = activeDelay;
                    crossfadeLength = 0;
                    crossfadePos = 0;
                }
            }
            else
            {
                activeDelay = targetDelay;
                oldDelay = activeDelay;
            }
        }
    }

private:
    int msToSamples (float ms) const
    {
        return static_cast<int> (std::round (0.001 * static_cast<double> (ms) * sampleRate));
    }

    double getCurrentEffectiveDelay() const
    {
        if (crossfadeLength <= 0)
            return targetDelay;

        const double t = juce::jlimit (0.0, 1.0,
                                      static_cast<double> (crossfadePos)
                                      / static_cast<double> (crossfadeLength));
        return oldDelay + (targetDelay - oldDelay) * t;
    }

    int wrapIndex (int index) const noexcept
    {
        const int n = ring.getNumSamples();
        while (index < 0) index += n;
        while (index >= n) index -= n;
        return index;
    }

    float readAtIntegerDelayMono (int delaySamples, int historyOffset) const
    {
        if (ring.getNumSamples() == 0)
            return 0.0f;

        const int pos = wrapIndex (writePos - delaySamples - historyOffset);
        float sum = 0.0f;
        const int channels = ring.getNumChannels();
        for (int ch = 0; ch < channels; ++ch)
            sum += ring.getSample (ch, pos);
        return sum / static_cast<float> (juce::jmax (1, channels));
    }

    double normalizedCorrelation (int delayA, int delayB, int length) const
    {
        if (length <= 4 || totalSamplesWritten < length + juce::jmax (delayA, delayB))
            return -1.0;

        double ab = 0.0, aa = 0.0, bb = 0.0;

        for (int i = 0; i < length; ++i)
        {
            const double a = readAtIntegerDelayMono (delayA, i);
            const double b = readAtIntegerDelayMono (delayB, i);
            ab += a * b;
            aa += a * a;
            bb += b * b;
        }

        const double denom = std::sqrt (aa * bb) + 1.0e-12;
        return ab / denom;
    }

    double findBestDelay (double fromDelay,
                          double requestedDelay,
                          int searchRadius,
                          int compareLength) const
    {
        const int from = juce::jlimit (2, ring.getNumSamples() - 4,
                                      static_cast<int> (std::round (fromDelay)));
        const int requested = juce::jlimit (2, ring.getNumSamples() - 4,
                                           static_cast<int> (std::round (requestedDelay)));

        if (searchRadius <= 0 || compareLength <= 0)
            return static_cast<double> (requested);

        double bestScore = -std::numeric_limits<double>::infinity();
        int bestDelay = requested;

        const int lo = juce::jmax (2, requested - searchRadius);
        const int hi = juce::jmin (ring.getNumSamples() - 4, requested + searchRadius);

        const int coarseStep = quality == Quality::studio ? 4 : 8;

        for (int d = lo; d <= hi; d += coarseStep)
        {
            const double score = normalizedCorrelation (from, d, compareLength);
            if (score > bestScore)
            {
                bestScore = score;
                bestDelay = d;
            }
        }

        const int fineLo = juce::jmax (lo, bestDelay - coarseStep);
        const int fineHi = juce::jmin (hi, bestDelay + coarseStep);
        for (int d = fineLo; d <= fineHi; ++d)
        {
            const double score = normalizedCorrelation (from, d, compareLength);
            if (score > bestScore)
            {
                bestScore = score;
                bestDelay = d;
            }
        }

        if (bestScore < 0.15)
            return static_cast<double> (requested);

        return static_cast<double> (bestDelay);
    }

    float readInterpolated (int channel, double delaySamples) const
    {
        double read = static_cast<double> (writePos) - delaySamples;
        const double n = static_cast<double> (ring.getNumSamples());

        while (read < 0.0) read += n;
        while (read >= n) read -= n;

        const int i0 = static_cast<int> (std::floor (read));
        const int i1 = (i0 + 1) % ring.getNumSamples();
        const float frac = static_cast<float> (read - static_cast<double> (i0));
        const float s0 = ring.getSample (channel, i0);
        const float s1 = ring.getSample (channel, i1);
        return s0 + frac * (s1 - s0);
    }

    double sampleRate = 44100.0;
    int maxBlock = 512;
    juce::AudioBuffer<float> ring;
    int writePos = 0;
    std::int64_t totalSamplesWritten = 0;

    int baseDelaySamples = 0;
    int preserveSamples = 0;
    bool initialisedBase = false;
    Quality quality = Quality::studio;

    double activeDelay = 0.0;
    double oldDelay = 0.0;
    double targetDelay = 0.0;

    int crossfadePos = 0;
    int crossfadeLength = 0;
};
