#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

/**
 * Asynchronous, thread-safe WAV encoder.
 * - Deep-copies input (no races)
 * - Sanitizes NaN/Inf
 * - Writes in chunks to avoid huge single memcpy
 * - Calls onDone on the JUCE message thread
 */
class WavEncoderAsync
{
public:
    using LogFn = std::function<void(const juce::String&)>;
    using DoneFn = std::function<void(const juce::MemoryBlock&)>;

    void encode(const juce::AudioBuffer<float>& input,
        double sampleRate,
        LogFn onLog,
        DoneFn onDone,
        int bitsPerSample = 16,
        int chunkSamples = 16384 /* ~0.34s @ 48k */)
    {
        if (onLog)  onLog("[WAV] Async encode request...");

        if (input.getNumSamples() <= 0 || sampleRate <= 0.0)
        {
            if (onLog) onLog("[WAV] Invalid input or sampleRate");
            // Return empty block to keep flow consistent
            if (onDone) juce::MessageManager::callAsync([onDone] { juce::MemoryBlock m; onDone(m); });
            return;
        }

        // Build a job payload (shared across thread)
        struct Job {
            juce::AudioBuffer<float> mono;
            double sampleRate = 16000.0;
            int chunk = 16384;
            int bits = 16;
            WavEncoderAsync::LogFn  log;
            WavEncoderAsync::DoneFn done;
        };
        auto job = std::make_shared<Job>();

        // Convert to mono (deep copy)
        const int n = input.getNumSamples();
        const int chans = juce::jmax(1, input.getNumChannels());
        job->mono.setSize(1, n);
        job->mono.clear();
        for (int ch = 0; ch < chans; ++ch)
            job->mono.addFrom(0, 0, input, ch, 0, n, 1.0f / float(chans));

        // Sanitize
        int fixed = 0;
        auto* d = job->mono.getWritePointer(0);
        for (int i = 0; i < n; ++i) { float s = d[i]; if (!std::isfinite(s)) { d[i] = 0.0f; ++fixed; } }

        job->sampleRate = sampleRate;
        job->chunk = juce::jmax(512, chunkSamples);
        job->bits = bitsPerSample;
        job->log = std::move(onLog);
        job->done = std::move(onDone);

        if (job->log)
        {
            job->log("[WAV] Encoding started (1 ch, " + juce::String(n) +
                " samples @ " + juce::String(sampleRate) + " Hz)");
            if (fixed > 0) job->log("[WAV] Sanitized " + juce::String(fixed) + " samples");
        }

        // Fire background work
        std::thread([job]()
            {
                try
                {
                    auto memStream = std::make_shared<juce::MemoryOutputStream>();
                    juce::WavAudioFormat wav;

                    if (job->log) job->log("[WAV] Creating writer...");
                    std::unique_ptr<juce::AudioFormatWriter> writer(
                        wav.createWriterFor(memStream.get(),
                            (int)job->sampleRate,
                            1,
                            job->bits,
                            {},
                            0));

                    if (!writer)
                        throw std::runtime_error("createWriterFor failed");

                    if (job->log)
                        job->log("[WAV] Writer OK. Header size = " + juce::String(memStream->getDataSize()) + " bytes");

                    // Chunked write
                    const int total = job->mono.getNumSamples();
                    int pos = 0;
                    while (pos < total)
                    {
                        const int toWrite = juce::jmin(job->chunk, total - pos);
                        if (job->log)
                            job->log("[WAV] Writing chunk " + juce::String(pos) + " .. " + juce::String(pos + toWrite));

                        writer->writeFromAudioSampleBuffer(job->mono, pos, toWrite);
                        pos += toWrite;
                    }

                    writer->flush();
                    writer.reset();

                    const auto finalSize = memStream->getDataSize();
                    if (job->log)
                        job->log("[WAV] Finished. Total stream size = " + juce::String(finalSize) + " bytes");

                    juce::MemoryBlock result = memStream->getMemoryBlock();

                    // Hand back on the message thread
                    if (job->done)
                    {
                        juce::MessageManager::callAsync([done = job->done, result]() {
                            done(result);
                            });
                    }
                    if (job->log)
                        job->log("[WAV] Done (" + juce::String(result.getSize() / 1024.0, 2) + " KB)");
                }
                catch (const std::exception& e)
                {
                    if (job->log) job->log(juce::String("[WAV] Exception: ") + e.what());
                    if (job->done)
                        juce::MessageManager::callAsync([done = job->done] { juce::MemoryBlock m; done(m); });
                }
                catch (...)
                {
                    if (job->log) job->log("[WAV] Unknown exception in async encoder");
                    if (job->done)
                        juce::MessageManager::callAsync([done = job->done] { juce::MemoryBlock m; done(m); });
                }
            }).detach();
    }
};
