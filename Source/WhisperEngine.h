#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

// whisper.cpp C API
extern "C" {
#include <whisper.h>
}
// Forward-declare to avoid including whisper.h in every file
struct whisper_context;

class WhisperEngine
{
public:
    WhisperEngine();
    ~WhisperEngine();

    // Load a model (.bin / .ggml) once; returns false on failure
    bool loadModel(const juce::File& modelFile, std::function<void(const juce::String&)> logCb);

    // Transcribe a mono float buffer at sampleRate (any rate). Internally resamples to 16k.
    // Returns the transcript string (empty on failure). Progress/log callbacks are optional.
    juce::String transcribe(const juce::AudioBuffer<float>& mono,
                            double sampleRate,
                            std::function<void(double)> progressCb,
                            std::function<void(const juce::String&)> logCb);

    bool isReady() const { return ctx != nullptr; }
    juce::File getModelPath() const { return modelPath; }
    juce::AudioBuffer<float> resampleTo16k(const juce::AudioBuffer<float>& in, double inRate,
        std::function<void(const juce::String&)> logCb);

private:
    whisper_context* ctx = nullptr;
    juce::File modelPath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WhisperEngine)
};
