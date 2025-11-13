#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

/** Cancellable, chunked WAV encoder (safe for long clips, Debug-friendly).
 *  Use startEncode(...). Call stopEncode() in your destructor.
 */
class WavEncoderJob : public juce::Thread
{
public:
    using LogFn = std::function<void(const juce::String&)>;
    using DoneFn = std::function<void(const juce::MemoryBlock&)>;

    WavEncoderJob() : juce::Thread("WavEncoderJob") {}
    ~WavEncoderJob() override { stopEncode(); }

    void startEncode(const juce::AudioBuffer<float>& input,
        double sampleRate,
        LogFn onLog,
        DoneFn onDone,
        int bitsPerSample = 16,
        int chunkSamples = 16384,
        bool diskFallback = true)
    {
        stopEncode();

        job = std::make_shared<Job>();
        job->sr = sampleRate;
        job->bits = bitsPerSample;
        job->chunk = juce::jlimit(1024, 131072, chunkSamples);
        job->disk = diskFallback;
        job->log = std::move(onLog);
        job->done = std::move(onDone);

        const int n = input.getNumSamples();
        const int c = juce::jmax(1, input.getNumChannels());
        job->mono.setSize(1, n);
        job->mono.clear();
        for (int ch = 0; ch < c; ++ch)
            job->mono.addFrom(0, 0, input, ch, 0, n, 1.0f / float(c));

        // sanitize NaN/Inf
        int fixed = 0;
        auto* w = job->mono.getWritePointer(0);
        for (int i = 0; i < n; ++i) { float s = w[i]; if (!std::isfinite(s)) { w[i] = 0.0f; ++fixed; } }
        if (job->log)
        {
            job->log("[WAV] Job queued (" + juce::String(n) + " samples @ " + juce::String(sampleRate) + " Hz)");
            if (fixed > 0) job->log("[WAV] Sanitized " + juce::String(fixed) + " samples");
        }

        shouldCancel = false;
        startThread(); // will call run()
    }

    void stopEncode()
    {
        shouldCancel = true;
        stopThread(4000);
        job.reset();
    }

private:
    struct Job {
        juce::AudioBuffer<float> mono;
        double sr = 16000.0;
        int bits = 16;
        int chunk = 16384;
        bool disk = true;
        LogFn  log;
        DoneFn done;
    };

    std::shared_ptr<Job> job;
    std::atomic<bool> shouldCancel{ false };

    void run() override
    {
        auto p = job;
        if (!p) return;

        try
        {
            juce::WavAudioFormat wav;
            juce::MemoryBlock out;

            if (p->disk)
            {
                auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getNonexistentChildFile("whisper_enc_", ".wav");
                if (p->log) p->log("[WAV] Using temp file: " + tmp.getFullPathName());
                std::unique_ptr<juce::FileOutputStream> fos(tmp.createOutputStream());
                if (!fos) { finishEmpty(p, "[WAV] Could not create temp stream"); return; }

                std::unique_ptr<juce::AudioFormatWriter> w(
                    wav.createWriterFor(fos.get(), (int)p->sr, 1, p->bits, {}, 0));
                if (!w) { finishEmpty(p, "[WAV] createWriterFor failed"); return; }
                fos.release(); // writer owns

                const int total = p->mono.getNumSamples();
                for (int pos = 0; pos < total && !shouldCancel; pos += p->chunk)
                {
                    const int n = juce::jmin(p->chunk, total - pos);
                    if (p->log) p->log("[WAV] Writing chunk " + juce::String(pos) + " .. " + juce::String(pos + n));
                    w->writeFromAudioSampleBuffer(p->mono, pos, n);
                }
                w.reset();
                tmp.loadFileAsData(out);
                tmp.deleteFile();
            }
            else
            {
                auto mem = std::make_unique<juce::MemoryOutputStream>();
                std::unique_ptr<juce::AudioFormatWriter> w(
                    wav.createWriterFor(mem.get(), (int)p->sr, 1, p->bits, {}, 0));
                if (!w) { finishEmpty(p, "[WAV] createWriterFor failed"); return; }
                const int total = p->mono.getNumSamples();
                for (int pos = 0; pos < total && !shouldCancel; pos += p->chunk)
                {
                    const int n = juce::jmin(p->chunk, total - pos);
                    if (p->log) p->log("[WAV] Writing chunk " + juce::String(pos) + " .. " + juce::String(pos + n));
                    w->writeFromAudioSampleBuffer(p->mono, pos, n);
                }
                w->flush(); w.reset();
                out = mem->getMemoryBlock();
            }

            if (shouldCancel) { finishEmpty(p, "[WAV] Cancelled"); return; }

            if (p->done)
            {
                juce::MemoryBlock copy(out);
                juce::MessageManager::callAsync([d = p->done, copy]() { d(copy); });
            }
            if (p->log) p->log("[WAV] Done (" + juce::String(out.getSize() / 1024.0, 2) + " KB)");
        }
        catch (const std::exception& e) { finishEmpty(p, juce::String("[WAV] Exception: ") + e.what()); }
        catch (...) { finishEmpty(p, "[WAV] Unknown exception"); }
    }

    void finishEmpty(const std::shared_ptr<Job>& p, const juce::String& msg)
    {
        if (p && p->log) p->log(msg);
        if (p && p->done)
            juce::MessageManager::callAsync([d = p->done] { juce::MemoryBlock m; d(m); });
    }
};
