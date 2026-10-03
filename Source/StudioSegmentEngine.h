#pragma once
#include <JuceHeader.h>
#include <signalsmith-stretch.h>
#include <vector>
#include <cmath>
#include <cstdint>

class StudioSegmentEngine
{
public:
    void prepare(double sr, int maxBlockSize, int channels)
    {
        sampleRate = sr;
        numChannels = juce::jmax(1, channels);
        maxBlock = juce::jmax(1, maxBlockSize);

        const int inCap = static_cast<int>(sampleRate * 8.0) + maxBlock + 64;
        const int outCap = static_cast<int>(sampleRate * 12.0) + maxBlock + 64;
        inputRing.setSize(numChannels, inCap);
        outputRing.setSize(numChannels, outCap);
        inputRing.clear();
        outputRing.clear();

        tempIn.setSize(numChannels, static_cast<int>(sampleRate * 4.0));
        tempOut.setSize(numChannels, static_cast<int>(sampleRate * 6.0));

        inputWrite = outputWrite = outputRead = 0;
        absoluteInput = 0;
        committedInput = 0;
        committedTarget = 0;
        outputAvailable = 0;
        startup = true;

        stretch.presetDefault(numChannels, sampleRate);
        stretch.reset();
    }

    void reset()
    {
        inputRing.clear();
        outputRing.clear();
        inputWrite = outputWrite = outputRead = 0;
        absoluteInput = committedInput = committedTarget = 0;
        outputAvailable = 0;
        startup = true;
        stretch.reset();
    }

    void setAnalysisMs(float ms)
    {
        analysisSamples = static_cast<int>(sampleRate * 0.001 * juce::jlimit(150.0f, 1800.0f, ms));
    }

    int getLatencySamples() const
    {
        return analysisSamples + stretch.inputLatency() + stretch.outputLatency();
    }

    void pushInput(const juce::AudioBuffer<float>& buffer)
    {
        const int n = buffer.getNumSamples();
        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                inputRing.setSample(ch, inputWrite, buffer.getSample(juce::jmin(ch, buffer.getNumChannels()-1), i));
            inputWrite = (inputWrite + 1) % inputRing.getNumSamples();
            ++absoluteInput;
        }
    }

    void addAnchor(std::int64_t inputSample, int correctionSamples, float confidence)
    {
        if (confidence < 0.45f)
            return;

        const std::int64_t target = inputSample + correctionSamples;
        if (target <= committedTarget || inputSample <= committedInput)
            return;

        processSegment(committedInput, inputSample, committedTarget, target);
        committedInput = inputSample;
        committedTarget = target;

        const auto inLen = juce::jmax<std::int64_t>(1, inputSample - lastAnchorInput);
        const auto outLen = juce::jmax<std::int64_t>(1, target - lastAnchorTarget);
        lastStretchRatio = static_cast<float>(static_cast<double>(outLen) / static_cast<double>(inLen));
        artifactRisk = juce::jlimit(0.0f, 1.0f, std::abs(lastStretchRatio - 1.0f) / 0.25f);

        lastAnchorInput = inputSample;
        lastAnchorTarget = target;
        startup = false;
    }

    void commitSafeAudio()
    {
        const std::int64_t safe = absoluteInput - analysisSamples;
        if (safe <= committedInput)
            return;

        const std::int64_t delta = safe - committedInput;
        processSegment(committedInput, safe, committedTarget, committedTarget + delta);
        committedInput = safe;
        committedTarget += delta;
    }

    void pullOutput(juce::AudioBuffer<float>& buffer)
    {
        buffer.clear();
        const int n = buffer.getNumSamples();

        for (int i = 0; i < n; ++i)
        {
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
    float readInput(int ch, std::int64_t absolutePos) const
    {
        const auto cap = static_cast<std::int64_t>(inputRing.getNumSamples());
        std::int64_t idx = absolutePos % cap;
        if (idx < 0) idx += cap;
        return inputRing.getSample(ch, static_cast<int>(idx));
    }

    void writeOutputSample(int ch, float sample)
    {
        outputRing.setSample(ch, outputWrite, sample);
    }

    void advanceOutputWrite()
    {
        outputWrite = (outputWrite + 1) % outputRing.getNumSamples();
        outputAvailable = juce::jmin(outputAvailable + 1, outputRing.getNumSamples() - 1);
        if (outputAvailable == outputRing.getNumSamples() - 1)
            outputRead = (outputRead + 1) % outputRing.getNumSamples();
    }

    void processSegment(std::int64_t inStart, std::int64_t inEnd,
                        std::int64_t outStart, std::int64_t outEnd)
    {
        const int inLen = static_cast<int>(inEnd - inStart);
        int outLen = static_cast<int>(outEnd - outStart);

        if (inLen <= 0 || outLen <= 0)
            return;

        const float ratio = static_cast<float>(outLen) / static_cast<float>(inLen);

        // Safety rail: outside this range we reduce correction rather than produce obvious artifacts.
        const float safeRatio = juce::jlimit(0.72f, 1.38f, ratio);
        outLen = juce::jmax(1, static_cast<int>(std::round(inLen * safeRatio)));

        if (inLen > tempIn.getNumSamples())
            return;
        if (outLen > tempOut.getNumSamples())
            return;

        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < inLen; ++i)
                tempIn.setSample(ch, i, readInput(ch, inStart + i));

        std::vector<const float*> inPtrs(static_cast<size_t>(numChannels));
        std::vector<float*> outPtrs(static_cast<size_t>(numChannels));
        for (int ch = 0; ch < numChannels; ++ch)
        {
            inPtrs[(size_t)ch] = tempIn.getReadPointer(ch);
            outPtrs[(size_t)ch] = tempOut.getWritePointer(ch);
        }

        stretch.process(inPtrs.data(), inLen, outPtrs.data(), outLen);

        const int fade = juce::jmin(outLen / 4, static_cast<int>(0.008 * sampleRate));

        for (int i = 0; i < outLen; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float s = tempOut.getSample(ch, i);

                if (fade > 0 && i < fade && outputAvailable > 0)
                {
                    const float t = static_cast<float>(i) / static_cast<float>(fade);
                    const int prevIndex = (outputWrite - 1 + outputRing.getNumSamples()) % outputRing.getNumSamples();
                    const float prev = outputRing.getSample(ch, prevIndex);
                    s = prev * (1.0f - t) + s * t;
                }

                writeOutputSample(ch, s);
            }
            advanceOutputWrite();
        }
    }

    double sampleRate = 44100.0;
    int numChannels = 2;
    int maxBlock = 512;
    int analysisSamples = 22050;

    juce::AudioBuffer<float> inputRing, outputRing, tempIn, tempOut;
    int inputWrite = 0, outputWrite = 0, outputRead = 0;
    int outputAvailable = 0;

    std::int64_t absoluteInput = 0;
    std::int64_t committedInput = 0;
    std::int64_t committedTarget = 0;
    std::int64_t lastAnchorInput = 0;
    std::int64_t lastAnchorTarget = 0;

    float lastStretchRatio = 1.0f;
    float artifactRisk = 0.0f;
    bool startup = true;

    signalsmith::stretch::SignalsmithStretch<float> stretch;
};
