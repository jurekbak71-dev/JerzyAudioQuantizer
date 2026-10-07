#pragma once
#include <JuceHeader.h>
#include <signalsmith-stretch/signalsmith-stretch.h>
#include <array>
#include <cmath>
#include <cstdint>

class StudioSegmentEngine
{
public:
    void prepare(double sr, int maxBlockSize, int channels)
    {
        sampleRate = juce::jmax(1.0, sr);
        numChannels = juce::jlimit(1, 2, channels);
        maxBlock = juce::jmax(1, maxBlockSize);

        maxAnalysisSamples = msToSamples(1400.0f);
        analysisSamples = msToSamples(850.0f);
        transientPreserveSamples = msToSamples(18.0f);
        crossfadeSamples = juce::jmax(16, msToSamples(5.0f));

        // Sized to the real studio use case rather than allocating 30 seconds
        // worth of stereo audio as the previous implementation effectively did.
        const int inCap = maxAnalysisSamples + msToSamples(2400.0f) + maxBlock + 128;
        const int outCap = maxAnalysisSamples + msToSamples(3200.0f) + maxBlock + 128;
        const int tempInCap = msToSamples(2400.0f) + maxBlock;
        const int tempOutCap = msToSamples(3100.0f) + maxBlock;

        inputRing.setSize(numChannels, inCap, false, true, true);
        outputRing.setSize(numChannels, outCap, false, true, true);
        tempIn.setSize(numChannels, tempInCap, false, true, true);
        tempOut.setSize(numChannels, tempOutCap, false, true, true);

        inputRing.clear();
        outputRing.clear();
        tempIn.clear();
        tempOut.clear();

        stretch.presetDefault(numChannels, sampleRate);
        stretch.reset();

        fixedLatencySamples = maxAnalysisSamples + stretch.inputLatency() + stretch.outputLatency();
        resetTimeline();
    }

    void reset()
    {
        inputRing.clear();
        outputRing.clear();
        tempIn.clear();
        tempOut.clear();
        stretch.reset();
        resetTimeline();
    }

    void setAnalysisMs(float ms) noexcept
    {
        analysisSamples = juce::jlimit(msToSamples(250.0f), maxAnalysisSamples,
                                      msToSamples(juce::jlimit(250.0f, 1400.0f, ms)));
    }

    void setTransientPreserveMs(float ms) noexcept
    {
        transientPreserveSamples = msToSamples(juce::jlimit(0.0f, 45.0f, ms));
    }

    int getLatencySamples() const noexcept
    {
        // Constant latency prevents repeated host/PDC reconfiguration while audio runs.
        return fixedLatencySamples;
    }

    void pushInput(const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int n = buffer.getNumSamples();
        const int sourceChannels = buffer.getNumChannels();

        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                inputRing.setSample(ch, inputWrite,
                    buffer.getSample(juce::jmin(ch, sourceChannels - 1), i));

            inputWrite = (inputWrite + 1) % inputRing.getNumSamples();
            ++absoluteInput;
        }
    }

    bool addAnchor(std::int64_t inputSample, int correctionSamples, float confidence)
    {
        if (confidence < 0.50f)
            return false;

        const std::int64_t target = inputSample + correctionSamples;

        if (inputSample <= committedInput + minAnchorSpacingSamples())
            return false;

        if (target <= committedTarget + minAnchorSpacingSamples())
            return false;

        const auto inLen = inputSample - committedInput;
        const auto requestedOutLen = target - committedTarget;

        if (inLen <= 0 || requestedOutLen <= 0)
            return false;

        // Keep segment ratios in the range where the stretcher remains transparent.
        // Large corrections are automatically softened instead of producing warble/glitches.
        const double requestedRatio = static_cast<double>(requestedOutLen) / static_cast<double>(inLen);
        const double safeRatio = juce::jlimit(0.78, 1.28, requestedRatio);
        const std::int64_t safeTarget = committedTarget
            + static_cast<std::int64_t>(std::llround(static_cast<double>(inLen) * safeRatio));

        if (!processSegment(committedInput, inputSample, committedTarget, safeTarget))
            return false;

        committedInput = inputSample;
        committedTarget = safeTarget;

        lastStretchRatio = static_cast<float>(safeRatio);
        const float ratioRisk = std::abs(lastStretchRatio - 1.0f) / 0.28f;
        const float correctionRisk = static_cast<float>(
            std::abs(static_cast<double>(safeTarget - target))
            / juce::jmax(1.0, static_cast<double>(inLen) * 0.20));
        artifactRisk = juce::jlimit(0.0f, 1.0f, 0.82f * ratioRisk + 0.18f * correctionRisk);
        return true;
    }

    void commitSafeAudio()
    {
        const std::int64_t safe = absoluteInput - analysisSamples;
        if (safe <= committedInput)
            return;

        const std::int64_t delta = safe - committedInput;
        if (processSegment(committedInput, safe, committedTarget, committedTarget + delta))
        {
            committedInput = safe;
            committedTarget += delta;
        }
    }

    void pullOutput(juce::AudioBuffer<float>& buffer) noexcept
    {
        buffer.clear();
        const int n = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
            if (latencyHold > 0)
            {
                --latencyHold;
                continue;
            }

            if (outputAvailable <= 0)
                continue;

            for (int ch = 0; ch < juce::jmin(numChannels, buffer.getNumChannels()); ++ch)
                buffer.setSample(ch, i, outputRing.getSample(ch, outputRead));

            outputRead = (outputRead + 1) % outputRing.getNumSamples();
            --outputAvailable;
        }
    }

    float getLastStretchRatio() const noexcept { return lastStretchRatio; }
    float getArtifactRisk() const noexcept { return artifactRisk; }

private:
    int msToSamples(float ms) const noexcept
    {
        return static_cast<int>(std::llround(0.001 * static_cast<double>(ms) * sampleRate));
    }

    int minAnchorSpacingSamples() const noexcept
    {
        return juce::jmax(16, msToSamples(24.0f));
    }

    void resetTimeline() noexcept
    {
        inputWrite = outputWrite = outputRead = 0;
        absoluteInput = 0;
        committedInput = 0;
        committedTarget = 0;
        outputAvailable = 0;
        latencyHold = fixedLatencySamples;
        lastStretchRatio = 1.0f;
        artifactRisk = 0.0f;
    }

    float readInput(int ch, std::int64_t absolutePos) const noexcept
    {
        const auto cap = static_cast<std::int64_t>(inputRing.getNumSamples());
        std::int64_t idx = absolutePos % cap;
        if (idx < 0)
            idx += cap;
        return inputRing.getSample(ch, static_cast<int>(idx));
    }

    void writeOutputSample(int ch, float sample) noexcept
    {
        outputRing.setSample(ch, outputWrite, sample);
    }

    void advanceOutputWrite() noexcept
    {
        outputWrite = (outputWrite + 1) % outputRing.getNumSamples();

        if (outputAvailable < outputRing.getNumSamples() - 1)
        {
            ++outputAvailable;
        }
        else
        {
            // Should not occur with normal DAW transport, but never overwrite unread
            // audio without advancing the read pointer as well.
            outputRead = (outputRead + 1) % outputRing.getNumSamples();
        }
    }

    bool processSegment(std::int64_t inStart, std::int64_t inEnd,
                        std::int64_t outStart, std::int64_t outEnd)
    {
        juce::ignoreUnused(outStart);

        const auto inLen64 = inEnd - inStart;
        const auto outLen64 = outEnd - outStart;
        if (inLen64 <= 0 || outLen64 <= 0)
            return false;

        if (inLen64 > tempIn.getNumSamples() || outLen64 > tempOut.getNumSamples())
            return false;

        const int inLen = static_cast<int>(inLen64);
        const int outLen = static_cast<int>(outLen64);

        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < inLen; ++i)
                tempIn.setSample(ch, i, readInput(ch, inStart + i));

        std::array<const float*, 2> inPtrs {};
        std::array<float*, 2> outPtrs {};

        for (int ch = 0; ch < numChannels; ++ch)
        {
            inPtrs[static_cast<size_t>(ch)] = tempIn.getReadPointer(ch);
            outPtrs[static_cast<size_t>(ch)] = tempOut.getWritePointer(ch);
        }

        // No heap allocation occurs here. The full segment is always passed through
        // the stretcher so its phase/history stays continuous between anchors.
        stretch.process(inPtrs.data(), inLen, outPtrs.data(), outLen);

        const int protect = juce::jlimit(0, juce::jmin(inLen - 1, outLen - 1),
                                         transientPreserveSamples);
        const int fade = juce::jmin(crossfadeSamples, protect);
        const int fadeStart = protect - fade;

        for (int i = 0; i < outLen; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float y = tempOut.getSample(ch, i);

                // The segment starts exactly on a previously accepted attack.
                // Copy the leading attack at 1x, then equal-power blend into the
                // stretched sustain while the stretcher still receives the full segment.
                if (i < protect && i < inLen)
                {
                    const float dryAttack = readInput(ch, inStart + i);

                    if (i < fadeStart || fade <= 1)
                    {
                        y = dryAttack;
                    }
                    else
                    {
                        const float t = static_cast<float>(i - fadeStart)
                                      / static_cast<float>(juce::jmax(1, fade));
                        const float dryGain = std::cos(t * juce::MathConstants<float>::halfPi);
                        const float wetGain = std::sin(t * juce::MathConstants<float>::halfPi);
                        y = dryAttack * dryGain + y * wetGain;
                    }
                }

                writeOutputSample(ch, y);
            }

            advanceOutputWrite();
        }

        return true;
    }

    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxBlock = 512;

    int maxAnalysisSamples = 61740;
    int analysisSamples = 37485;
    int transientPreserveSamples = 794;
    int crossfadeSamples = 220;
    int fixedLatencySamples = 0;

    juce::AudioBuffer<float> inputRing, outputRing, tempIn, tempOut;
    int inputWrite = 0;
    int outputWrite = 0;
    int outputRead = 0;
    int outputAvailable = 0;
    int latencyHold = 0;

    std::int64_t absoluteInput = 0;
    std::int64_t committedInput = 0;
    std::int64_t committedTarget = 0;

    float lastStretchRatio = 1.0f;
    float artifactRisk = 0.0f;

    signalsmith::stretch::SignalsmithStretch<float> stretch;
};
