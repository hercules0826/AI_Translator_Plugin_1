// Source/PluginProcessor.h
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "WhisperEngine.h"
#include "TranslationEngine.h"
#include "WhisperThread.h"
#include <juce_audio_devices/sources/juce_AudioTransportSource.h>

class WhisperFreeWinAudioProcessor : public juce::AudioProcessor
{
public:
    WhisperFreeWinAudioProcessor();
    ~WhisperFreeWinAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "WhisperFreeWin"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // UI sinks
    void setLogSink(std::function<void(const juce::String&)> s) { logSink = std::move(s); }
    void setTranscriptSink(std::function<void(const juce::String&)> s) { transcriptSink = std::move(s); }
    void setTranslationSink(std::function<void(const juce::String&)> s) { translationSink = std::move(s); }
    void setProgressSink(std::function<void(double)> s) { progressSink = std::move(s); }

    // Actions from UI
    bool loadWavFile(const juce::File& file);
    void startPlayback();
    void stopPlayback();

    bool loadWhisperModel(const juce::File& modelFile);
    bool loadMarianModel(const juce::File& folder);
    bool sendLoadedBufferToWhisper();

    void setAutoTranslate(bool b) { autoTranslate = b; }

private:
    void appendLog(const juce::String& s);
    void handleTranscript(const juce::String& t);
    void handleTranslation(const juce::String& t);
    void handleProgress(double p);

    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;

    juce::AudioBuffer<float> loadedMono;
    double loadedSampleRate = 48000.0;

    WhisperEngine      whisperEngine;
    TranslationEngine  translationEngine;
    std::unique_ptr<WhisperThread> whisperThread;

    std::function<void(const juce::String&)> logSink;
    std::function<void(const juce::String&)> transcriptSink;
    std::function<void(const juce::String&)> translationSink;
    std::function<void(double)>              progressSink;

    bool autoTranslate = false;
    bool marianLoaded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WhisperFreeWinAudioProcessor)
};
