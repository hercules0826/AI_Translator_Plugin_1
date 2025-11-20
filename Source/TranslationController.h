#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <atomic>
#include <memory>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include "WhisperEngine.h"
#include "MarianEngine.h"

class TranslationController
{
public:
    using LogCallback = std::function<void(const juce::String&)>;
    using UiCallback  = std::function<void(const juce::String& asr, const juce::String& translated)>;

    TranslationController();
    ~TranslationController();

    void prepare(double sampleRate, int numChannels, int blockSize);

    bool loadWhisperModel(const juce::File& modelFile);
    bool loadMarianModel(const juce::File& modelDir);

    void start();
    void stop();

    void pushAudio(const juce::AudioBuffer<float>& buffer);

    void setUiCallback(UiCallback cb) { uiCallback = std::move(cb); }

    juce::String getLastASR() const { return lastASR; }
    juce::String getLastTranslation() const { return lastTranslation; }

private:
    void asrResultHandler(const juce::String& text);
    void translationLoop();

    WhisperEngine whisper;
    MarianEngine marian;

    LogCallback logCallback;
    UiCallback uiCallback;

    std::thread translationThread;
    std::atomic<bool> isRunning { false };

    mutable std::mutex textMutex;
    juce::String lastASR, lastTranslation;

    // Queue for texts to translate
    std::mutex queueMutex;
    std::condition_variable queueCv;
    std::queue<juce::String> translateQueue;
};
