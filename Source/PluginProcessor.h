#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "WhisperThread.h"
#include "WhisperEngine.h"
#include <juce_audio_devices/sources/juce_AudioTransportSource.h>

class WhisperFreeWinAudioProcessor : public juce::AudioProcessor
{
public:
    WhisperFreeWinAudioProcessor();
    ~WhisperFreeWinAudioProcessor() override;

    const juce::String getName() const override { return "WhisperFreeWin"; }
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    //==============================================================================
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Program handling (JUCE requires these even if unused)
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // State persistence
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // ===== UI hooks
    void setLogSink(std::function<void(const juce::String&)> sink) { logSink = std::move(sink); }
    void setTranscriptSink(std::function<void(const juce::String&)> sink) { transcriptSink = std::move(sink); }
    void setProgressSink(std::function<void(double)> sink) { progressSink = std::move(sink); }
    void appendLog(const juce::String& s);
    void onTranscript(const juce::String& t);
    void onProgress(double p) { if (progressSink) juce::MessageManager::callAsync([sink = progressSink, p] { sink(p); });}

    // ===== WAV loading + playback
    bool loadWavFile(const juce::File& file);
    void play();
    void stop();
    bool isPlaying() const { return transport.isPlaying(); }

    // Whisper
    bool loadModel(const juce::File& modelFile)
    {
        return engine.loadModel(modelFile, [this](const juce::String& s) { appendLog(s); });
    }

    // ===== Whisper send (NEW)
    bool sendLoadedBufferToWhisper();        // <— expose clean call
    bool hasLoadedAudio() const { return loadedMono.getNumSamples() > 0 && loadedSampleRate > 0.0; }

    // Access
    WhisperThread* getWhisper() { return whisperThread.get(); }
    double getLoadedSampleRate() const { return loadedSampleRate; }

private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;

    WhisperEngine engine;
    std::unique_ptr<WhisperThread> whisperThread;
    std::function<void(const juce::String&)> logSink;
    std::function<void(const juce::String&)> transcriptSink;
    std::function<void(double)> progressSink;

    juce::AudioBuffer<float> loadedMono;  // cached mono copy for upload
    double loadedSampleRate = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WhisperFreeWinAudioProcessor)
};
