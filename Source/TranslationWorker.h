#pragma once
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <juce_audio_basics/juce_audio_basics.h>

class TranslationWorker : private juce::Timer
{
public:
    TranslationWorker();
    ~TranslationWorker() override;

    // Launch Python worker
    bool startWorker(const juce::File& pythonExe,
                     const juce::File& scriptFile,
                     std::function<void(const juce::String&)> logCallback);

    // Stop the worker process
    void stopWorker();

    // Send text to translate (non-blocking)
    void translate(const juce::String& text,
                   std::function<void(const juce::String&)> onResult);

private:
    void timerCallback() override;   // Polls python output

    std::unique_ptr<juce::ChildProcess> process;
    std::function<void(const juce::String&)> onResultCb;
    std::function<void(const juce::String&)> logCb;

    bool workerRunning = false;
};
