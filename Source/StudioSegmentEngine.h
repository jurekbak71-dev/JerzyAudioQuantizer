#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

class StudioSegmentEngine
{
public:
    void prepare(double sr, int maxBlockSize, int channels)
    {
        sampleRate = juce::jmax(1.0, sr);
        numChannels = juce::jlimit(1, 2, channels);
        maxBlock = juce::jmax(1, maxBlockSize);

        baseLatencySamples = msToSamples(1200.0f);
        maxCorrectionSamples = msToSamples(200.0f);
        transitionSamples = juce::jmax(64, msToSamples(10.0f));
        attackGuardSamples = msToSamples(22.0f);

        const int capacity = baseLatencySamples + maxCorrectionSamples
                           + msToSamples(300.0f) + maxBlock * 4 + 256;

        ring.setSize(numChannels, capacity, false, true, true);
        ring.clear();

        reset();
    }

    void reset()
    {
        ring.clear();
        writePos = 0;
        absoluteInput = 0;
        absoluteOutput = 0;

        currentDelay = baseLatencySamples;
        oldDelay = currentDelay;
        targetDelay = currentDelay;
        scheduledDelay = currentDelay;

        fadePos = 0;
        fadeLength = 0;

        eventRead = 0;
        eventWrite = 0;
        eventCount = 0;

        lastScheduledTargetOutput = -1;
        lastCorrectionSamples = 0;
        artifactRisk = 0.0f;
    }

    void setAnalysisMs(float) noexcept {}

    void setTransientPreserveMs(float ms) noexcept
    {
        attackGuardSamples = msToSamples(juce::jlimit(8.0f, 45.0f, ms));
    }

    int getLatencySamples() const noexcept
    {
        return baseLatencySamples;
    }

    void pushInput(const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n = buffer.getNumSamples();
        const int inChannels = buffer.getNumChannels();

        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                ring.setSample(ch, writePos,
                    buffer.getSample(juce::jmin(ch, inChannels - 1), i));

            writePos = (writePos + 1) % ring.getNumSamples();
            ++absoluteInput;
        }
    }

    bool addAnchor(std::int64_t inputSample, int correctionSamples, float confidence) noexcept
    {
        if (confidence < 0.50f)
            return false;

        const int correction = juce::jlimit(-maxCorrectionSamples,
                                            maxCorrectionSamples,
                                            correctionSamples);

        const int delay = baseLatencySamples + correction;
        const std::int64_t targetOutput = inputSample + delay;

        if (lastScheduledTargetOutput >= 0
            && targetOutput <= lastScheduledTargetOutput + msToSamples(18.0f))
            return false;

        Event e;
        e.targetDelay = delay;
        e.targetOutput = targetOutput;

        // Move the read head in the quietest region before the next pick attack.
        // Both heads still run at 1x, so pitch does not bend during correction.
        const auto quietSource = findQuietSplice(inputSample);
        e.startOutput = quietSource + scheduledDelay;

        if (e.startOutput <= absoluteOutput + transitionSamples)
            return false;

        if (eventCount >= static_cast<int>(events.size()))
            return false;

        events[static_cast<size_t>(eventWrite)] = e;
        eventWrite = (eventWrite + 1) % static_cast<int>(events.size());
        ++eventCount;

        lastScheduledTargetOutput = targetOutput;
        scheduledDelay = delay;
        lastCorrectionSamples = correction;

        const float correctionNorm = static_cast<float>(std::abs(correction))
                                   / static_cast<float>(juce::jmax(1, maxCorrectionSamples));
        artifactRisk = juce::jlimit(0.0f, 1.0f, correctionNorm * 0.75f);

        return true;
    }

    void commitSafeAudio() noexcept {}

    void pullOutput(juce::AudioBuffer<float>& buffer) noexcept
    {
        const int samples = buffer.getNumSamples();
        const int channels = juce::jmin(numChannels, buffer.getNumChannels());

        for (int i = 0; i < samples; ++i)
        {
            maybeStartScheduledTransition();

            float gOld = 0.0f;
            float gNew = 1.0f;

            if (fadeLength > 0)
            {
                const float t = juce::jlimit(0.0f, 1.0f,
                    static_cast<float>(fadePos) / static_cast<float>(fadeLength));

                gOld = std::cos(t * juce::MathConstants<float>::halfPi);
                gNew = std::sin(t * juce::MathConstants<float>::halfPi);
            }

            for (int ch = 0; ch < channels; ++ch)
            {
                const float newer = readAbsolute(ch, absoluteOutput - targetDelay);

                float y = newer;
                if (fadeLength > 0)
                {
                    const float older = readAbsolute(ch, absoluteOutput - oldDelay);
                    y = older * gOld + newer * gNew;
                }

                buffer.setSample(ch, i, y);
            }

            if (fadeLength > 0)
            {
                ++fadePos;
                if (fadePos >= fadeLength)
                {
                    currentDelay = targetDelay;
                    oldDelay = currentDelay;
                    fadePos = 0;
                    fadeLength = 0;
                }
            }

            ++absoluteOutput;
        }
    }

    float getLastStretchRatio() const noexcept
    {
        // GUI compatibility: 1.0 means no timing move.
        return 1.0f + static_cast<float>(lastCorrectionSamples)
                    / static_cast<float>(juce::jmax(1, baseLatencySamples));
    }

    float getArtifactRisk() const noexcept { return artifactRisk; }

private:
    struct Event
    {
        std::int64_t startOutput = 0;
        std::int64_t targetOutput = 0;
        int targetDelay = 0;
    };

    int msToSamples(float ms) const noexcept
    {
        return static_cast<int>(std::llround(
            0.001 * static_cast<double>(ms) * sampleRate));
    }

    float readAbsolute(int channel, std::int64_t absoluteSample) const noexcept
    {
        if (absoluteSample < 0)
            return 0.0f;

        const std::int64_t oldestAvailable =
            absoluteInput - static_cast<std::int64_t>(ring.getNumSamples());

        if (absoluteSample < oldestAvailable || absoluteSample >= absoluteInput)
            return 0.0f;

        const auto cap = static_cast<std::int64_t>(ring.getNumSamples());
        std::int64_t idx = absoluteSample % cap;
        if (idx < 0)
            idx += cap;

        return ring.getSample(channel, static_cast<int>(idx));
    }

    std::int64_t findQuietSplice(std::int64_t attackSample) const noexcept
    {
        const auto searchStart = juce::jmax<std::int64_t>(0, attackSample - msToSamples(80.0f));
        const auto searchEnd = juce::jmax<std::int64_t>(searchStart, attackSample - attackGuardSamples);
        const int window = juce::jmax(16, msToSamples(2.0f));
        const int step = juce::jmax(8, window / 2);

        if (searchEnd <= searchStart + window)
            return juce::jmax<std::int64_t>(0, attackSample - attackGuardSamples - transitionSamples);

        double bestEnergy = std::numeric_limits<double>::max();
        std::int64_t best = searchEnd - window;

        for (std::int64_t p = searchStart; p + window < searchEnd; p += step)
        {
            double energy = 0.0;
            for (int i = 0; i < window; i += 4)
            {
                float mono = 0.0f;
                for (int ch = 0; ch < numChannels; ++ch)
                    mono += readAbsolute(ch, p + i);
                mono /= static_cast<float>(numChannels);
                energy += static_cast<double>(mono) * static_cast<double>(mono);
            }

            if (energy < bestEnergy)
            {
                bestEnergy = energy;
                best = p;
            }
        }

        return best;
    }

    void maybeStartScheduledTransition() noexcept
    {
        if (fadeLength > 0 || eventCount <= 0)
            return;

        const auto& e = events[static_cast<size_t>(eventRead)];
        if (absoluteOutput < e.startOutput)
            return;

        oldDelay = currentDelay;
        targetDelay = e.targetDelay;
        fadePos = 0;
        fadeLength = transitionSamples;

        eventRead = (eventRead + 1) % static_cast<int>(events.size());
        --eventCount;
    }

    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxBlock = 512;

    int baseLatencySamples = 52920;
    int maxCorrectionSamples = 8820;
    int transitionSamples = 441;
    int attackGuardSamples = 970;

    juce::AudioBuffer<float> ring;
    int writePos = 0;

    std::int64_t absoluteInput = 0;
    std::int64_t absoluteOutput = 0;

    int currentDelay = 0;
    int oldDelay = 0;
    int targetDelay = 0;
    int scheduledDelay = 0;
    int fadePos = 0;
    int fadeLength = 0;

    std::array<Event, 96> events {};
    int eventRead = 0;
    int eventWrite = 0;
    int eventCount = 0;

    std::int64_t lastScheduledTargetOutput = -1;
    int lastCorrectionSamples = 0;
    float artifactRisk = 0.0f;
};
