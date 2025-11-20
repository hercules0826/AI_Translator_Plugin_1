// Source/WhisperThread.h
#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <deque>
#include "WhisperEngine.h"
#include "TranslationEngine.h"

class WhisperThread : public juce::Thread
{
public:
    WhisperThread(WhisperEngine& asrEngine,
        TranslationEngine& trEngine,
        std::function<void(double)> onProgress,
        std::function<void(const juce::String&)> onLog,
        std::function<void(const juce::String&)> onTranscript,
        std::function<void(const juce::String&)> onTranslation)
        : Thread("WhisperThread"),
        asr(asrEngine),
        translator(trEngine),
        progressCb(std::move(onProgress)),
        logCb(std::move(onLog)),
        transcriptCb(std::move(onTranscript)),
        translationCb(std::move(onTranslation))
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
        bool autoTranslateFlag)
    {
        juce::ScopedLock sl(queueLock);
        Task t;
        t.buffer.makeCopyOf(buf);
        t.sampleRate = sampleRate;
        t.autoTranslate = autoTranslateFlag;
        queue.push_back(std::move(t));
        notify();
    }

    void flushQueue()
    {
        juce::ScopedLock sl(queueLock);
        queue.clear();
    }

    void setTranslatorLoaded(bool b) { translatorLoaded = b; }

    void run() override
    {
        while (!threadShouldExit())
        {
            Task task;

            {
                juce::ScopedLock sl(queueLock);
                if (!queue.empty())
                {
                    task = std::move(queue.front());
                    queue.pop_front();
                }
            }

            if (threadShouldExit())
                break;

            if (task.buffer.getNumSamples() > 0)
            {
                try
                {
                    if (logCb)
                        logCb("[ASR] Processing " +
                            juce::String(task.buffer.getNumSamples()) + " samples");

                    auto text = asr.transcribe(task.buffer,
                        task.sampleRate,
                        progressCb,
                        logCb);

                    if (text.isNotEmpty() && transcriptCb)
                        transcriptCb(text);

                    if (task.autoTranslate && translatorLoaded && text.isNotEmpty())
                    {
                        auto translated = translator.translate(text, logCb);
                        if (translated.isNotEmpty() && translationCb)
                            translationCb(translated);
                    }

                    if (progressCb)
                        progressCb(0.0);
                }
                catch (const std::exception& e)
                {
                    if (logCb)
                        logCb("[ASR] Exception: " + juce::String(e.what()));
                }

                continue;
            }

            wait(100);
        }
    }

private:
    struct Task
    {
        juce::AudioBuffer<float> buffer;
        double sampleRate = 16000.0;
        bool   autoTranslate = false;
    };

    WhisperEngine& asr;
    TranslationEngine& translator;

    std::function<void(double)>              progressCb;
    std::function<void(const juce::String&)> logCb;
    std::function<void(const juce::String&)> transcriptCb;
    std::function<void(const juce::String&)> translationCb;

    juce::CriticalSection queueLock;
    std::deque<Task>      queue;

    bool translatorLoaded = false;
};
