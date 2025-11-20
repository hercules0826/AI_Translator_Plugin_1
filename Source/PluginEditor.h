// Source/PluginEditor.h
#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class WhisperFreeWinAudioProcessorEditor : public juce::AudioProcessorEditor,
    public juce::Button::Listener
{
public:
    explicit WhisperFreeWinAudioProcessorEditor(WhisperFreeWinAudioProcessor&);
    ~WhisperFreeWinAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void buttonClicked(juce::Button*) override;

private:
    WhisperFreeWinAudioProcessor& processor;

    juce::TextButton loadWavButton{ "Load WAV..." };
    juce::TextButton playButton{ "Play" };
    juce::TextButton stopButton{ "Stop" };
    juce::TextButton sendButton{ "Send to Whisper" };
    juce::TextButton loadWhisperBtn{ "Load Whisper Model..." };
    juce::TextButton loadMarianBtn{ "Load Marian Model Folder..." };

    juce::ToggleButton autoTranslateToggle{ "Auto translate (de→en)" };

    juce::TextEditor logBox;
    juce::TextEditor transcriptBox;
    juce::TextEditor translationBox;

    double progressValue = 0.0;
    juce::ProgressBar progressBar;

    void appendLog(const juce::String& s);
    void setTranscript(const juce::String& s);
    void setTranslation(const juce::String& s);
    void setProgress(double p);
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WhisperFreeWinAudioProcessorEditor)
};
