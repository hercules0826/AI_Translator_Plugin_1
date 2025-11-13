//#pragma once
//#include <juce_core/juce_core.h>
//#include <juce_audio_basics/juce_audio_basics.h>
//#include <juce_audio_formats/juce_audio_formats.h>
//#include "WavEncoderJob.h"
//
///** Encodes buffer → POSTs to local Whisper server → returns transcript. */
//class WhisperThread
//{
//public:
//    WhisperThread(const juce::URL& endpoint,
//        std::function<void(double)> onProgress,
//        std::function<void(const juce::String&)> onLog,
//        std::function<void(juce::String)> onTranscript)
//        : url(endpoint), progressCb(std::move(onProgress)),
//        logCb(std::move(onLog)), resultCb(std::move(onTranscript)) {
//    }
//
//    ~WhisperThread() { encoder.stopEncode(); }
//
//    void sendBuffer(const juce::AudioBuffer<float>& buffer, double sampleRate)
//    {
//        if (logCb) logCb("[HTTP] Queueing upload...");
//        encoder.startEncode(buffer, sampleRate, logCb,
//            [this](const juce::MemoryBlock& wav)
//            {
//                if (wav.getSize() == 0) { if (logCb) logCb("[HTTP] Skipped (empty wav)"); return; }
//                postWav(wav);
//            },
//            16, 16384, true);
//        if (progressCb) progressCb(-1.0); // indeterminate during network
//    }
//
//private:
//    juce::URL url;
//    WavEncoderJob encoder;
//    std::function<void(double)>       progressCb;
//    std::function<void(const juce::String&)> logCb;
//    std::function<void(juce::String)> resultCb;
//
//    void postWav(const juce::MemoryBlock& wav)
//    {
//        // JSON body with base64 wav
//        juce::String b64 = juce::Base64::toBase64(wav.getData(), (int)wav.getSize());
//        auto* obj = new juce::DynamicObject();
//        obj->setProperty("file_b64", b64);
//        juce::String body = juce::JSON::toString(juce::var(obj));
//        juce::StringPairArray headers;
//        headers.set("Content-Type", "application/json; charset=utf-8");
//        headers.set("Accept", "application/json");  
//
//        int status = 0;
//        auto in = juce::URL("http://localhost:8000/asr").createInputStream(true, nullptr, nullptr, body, 0, &headers, &status, 5, "POST");
//        if (!in) { if (logCb) logCb("[HTTP] Failed to open stream"); if (progressCb) progressCb(0.0); return; }
//
//        const auto resp = in->readEntireStreamAsString();
//        if (logCb) logCb("[HTTP] Status " + juce::String(status) + ", " + juce::String(resp.substring(0, 200)));
//        if (progressCb) progressCb(1.0);
//
//        const juce::var parsed = juce::JSON::parse(resp);
//        if (auto* robj = parsed.getDynamicObject())
//        {
//            const auto text = robj->getProperty("text").toString();
//            if (text.isNotEmpty() && resultCb) resultCb(text);
//        }
//        if (progressCb) progressCb(0.0);
//    }
//};


#pragma once
#include <deque>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "WhisperEngine.h"

// Background worker; encodes/queues buffers and calls WhisperEngine
class WhisperThread : public juce::Thread
{
public:
    WhisperThread(WhisperEngine& engine,
        std::function<void(double)> onProgress,
        std::function<void(const juce::String&)> onLog,
        std::function<void(juce::String)> onTranscript)
        : Thread("WhisperThread"),
        asr(engine),
        progressCb(std::move(onProgress)),
        resultCb(std::move(onTranscript)),
        logCb(std::move(onLog))
    {
    }

    ~WhisperThread() override
    {
        signalThreadShouldExit();
        flushQueue();
        stopThread(3000);
    }

    void sendBufferNow(const juce::AudioBuffer<float>& buf,
        double sampleRate,
        double positionSec)
    {
        juce::ScopedLock sl(queueLock);
        Task t; t.buffer.makeCopyOf(buf); t.sampleRate = sampleRate; t.positionSec = positionSec;
        queue.push_back(std::move(t));
        notify();
    }

    void flushQueue()
    {
        juce::ScopedLock sl(queueLock);
        queue.clear();
    }

    void run() override
    {
        while (!threadShouldExit())
        {
            Task task;
            {
                juce::ScopedLock sl(queueLock);
                if (!queue.empty()) { task = std::move(queue.front()); queue.pop_front(); }
            }

            if (task.buffer.getNumSamples() > 0)
            {
                if (logCb) logCb("[ASR] Queue popped: " + juce::String(task.buffer.getNumSamples()) + " samples");
                juce::AudioBuffer<float> mono16k = asr.resampleTo16k(task.buffer, task.sampleRate, logCb);
                //auto text = asr.transcribe(task.buffer, task.sampleRate, progressCb, logCb);
                auto text = asr.transcribe(mono16k, 16000.0, progressCb, logCb);
                if (text.isNotEmpty() && resultCb) resultCb(text);
                if (progressCb) progressCb(0.0);
                continue;
            }

            wait(100);
        }
    }

private:
    struct Task { juce::AudioBuffer<float> buffer; double sampleRate = 48000.0; double positionSec = 0.0; };

    WhisperEngine& asr;
    std::function<void(double)>       progressCb;
    std::function<void(juce::String)> resultCb;
    std::function<void(const juce::String&)> logCb;

    juce::CriticalSection queueLock;
    std::deque<Task> queue;
};
