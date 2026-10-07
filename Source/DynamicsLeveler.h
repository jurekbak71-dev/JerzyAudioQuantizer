#pragma once
#include <JuceHeader.h>
#include <cmath>

class DynamicsLeveler
{
public:
    void prepare(double sr)
    {
        sampleRate = juce::jmax(1.0, sr);
        envelope = 0.0f;
        gain = 1.0f;
        attackCoeff = coeff(8.0);
        releaseCoeff = coeff(140.0);
    }

    void reset() noexcept
    {
        envelope = 0.0f;
        gain = 1.0f;
        gainReductionDb = 0.0f;
    }

    void setAmount(float amount01) noexcept
    {
        amount = juce::jlimit(0.0f, 1.0f, amount01);
    }

    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        if (amount <= 0.0001f)
        {
            gainReductionDb = 0.0f;
            return;
        }

        const int channels = buffer.getNumChannels();
        const int samples = buffer.getNumSamples();
        const float threshold = juce::Decibels::decibelsToGain(-17.0f);
        const float ratio = 1.0f + 3.5f * amount;
        const float exponent = 1.0f / ratio - 1.0f;
        const float makeup = juce::Decibels::decibelsToGain(1.2f * amount);

        for (int i = 0; i < samples; ++i)
        {
            float detector = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                detector = juce::jmax(detector, std::abs(buffer.getSample(ch, i)));

            const float envCoeff = detector > envelope ? attackCoeff : releaseCoeff;
            envelope += envCoeff * (detector - envelope);

            float desired = 1.0f;
            if (envelope > threshold)
                desired = std::pow(envelope / threshold, exponent);

            // Smooth gain itself to avoid zipper noise around pick transients.
            const float gainCoeff = desired < gain ? attackCoeff : releaseCoeff;
            gain += gainCoeff * (desired - gain);

            const float g = gain * makeup;
            for (int ch = 0; ch < channels; ++ch)
                buffer.setSample(ch, i, buffer.getSample(ch, i) * g);
        }

        gainReductionDb = juce::jmax(0.0f, -juce::Decibels::gainToDecibels(gain, -80.0f));
    }

    float getGainReductionDb() const noexcept { return gainReductionDb; }

private:
    float coeff(double ms) const noexcept
    {
        return static_cast<float>(1.0 - std::exp(-1.0 / (0.001 * ms * sampleRate)));
    }

    double sampleRate = 44100.0;
    float amount = 0.0f;
    float envelope = 0.0f;
    float gain = 1.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float gainReductionDb = 0.0f;
};
