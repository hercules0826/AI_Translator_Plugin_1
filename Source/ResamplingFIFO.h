#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

class ResamplingFIFO
{
public:
    ResamplingFIFO() = default;

    void prepare(double inputSampleRate, int numChannels, int blockSize)
    {
        jassert(numChannels > 0);
        srIn = inputSampleRate;
        srOut = 16000.0;
        channels = numChannels;

        fifoBuffer.setSize(numChannels, blockSize * 8); // safety
        fifoBuffer.clear();

        resampler.reset();
        resampler.prepare({ srIn, (juce::uint32) blockSize, (juce::uint32) numChannels });
        resampler.setResamplingRatio(srIn / srOut);

        tempBlock = juce::dsp::AudioBlock<float>(tempBuffer);
        outBlock  = juce::dsp::AudioBlock<float>(outBuffer);
    }

    void push(const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        if (fifoBuffer.getNumSamples() < numSamples + writePos)
            fifoBuffer.setSize(numChannels, writePos + numSamples, true, true, true);

        for (int ch = 0; ch < numChannels; ++ch)
            fifoBuffer.copyFrom(ch, writePos, buffer, ch, 0, numSamples);

        writePos += numSamples;
    }

    /** Pull resampled mono 16k data. Returns number of samples written into dest. */
    int pullResampled(juce::AudioBuffer<float>& dest, int maxSamples)
    {
        if (writePos <= 0)
            return 0;

        // use temp buffer for input block
        tempBuffer.setSize(channels, writePos, false, false, true);
        for (int ch = 0; ch < channels; ++ch)
            tempBuffer.copyFrom(ch, 0, fifoBuffer, ch, 0, writePos);

        juce::dsp::AudioBlock<float> inBlock(tempBuffer);
        juce::dsp::ProcessContextReplacing<float> ctx(inBlock);
        // actually we want output, so allocate outBuffer
        outBuffer.setSize(channels, maxSamples, false, false, true);
        outBlock = juce::dsp::AudioBlock<float>(outBuffer);

        juce::dsp::ProcessContextNonReplacing<float> ctxResample(inBlock, outBlock);
        resampler.process(ctxResample);

        const int produced = (int) outBlock.getNumSamples();

        // mixdown to mono
        dest.setSize(1, produced, false, false, true);
        dest.clear();
        for (int ch = 0; ch < channels; ++ch)
            dest.addFrom(0, 0, outBuffer, ch, 0, produced, 1.0f / (float) channels);

        // reset writePos (simple “consume all” FIFO)
        writePos = 0;
        return produced;
    }

private:
    double srIn = 48000.0;
    double srOut = 16000.0;
    int channels = 1;

    juce::AudioBuffer<float> fifoBuffer;
    int writePos = 0;

    juce::AudioBuffer<float> tempBuffer, outBuffer;
    juce::dsp::AudioBlock<float> tempBlock, outBlock;
    juce::dsp::ResamplingAudioSource resampleSource { nullptr, false, 1 };
    juce::dsp::StateVariableTPTFilter<float> dummyFilter; // ignore: just placeholder

    juce::dsp::ProcessorDuplicator<
        juce::dsp::IIR::Filter<float>,
        juce::dsp::IIR::Coefficients<float>> iir;

    juce::dsp::Oversampling<float> oversampling { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple };

    juce::dsp::ResamplingAudioSource dummySource { nullptr, false, 1 }; // not used
    juce::dsp::Resampler resampler;
};
