#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

class WhisperFreeWinAudioProcessorEditor : public juce::AudioProcessorEditor,
    private juce::Button::Listener
{
public:
    explicit WhisperFreeWinAudioProcessorEditor(WhisperFreeWinAudioProcessor&);
    ~WhisperFreeWinAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    WhisperFreeWinAudioProcessor& processor;

    juce::TextButton loadBtn{ "Load WAV file..." };
    juce::TextButton playBtn{ "Play" };
    juce::TextButton stopBtn{ "Stop" };
    juce::TextButton loadModelBtn{ "Load Model..." };
    juce::TextButton sendBtn{ "Transcribe" };

    juce::Label transcriptLabel;
    juce::TextEditor logConsole;   // scrolling console
    juce::ProgressBar progressBar{ progressValue };
    double progressValue = 0.0;

    std::unique_ptr<juce::FileChooser> chooser;

    void appendLogUI(const juce::String& s);
    void showTranscript(const juce::String& t);
    void setProgress(double p);

    void buttonClicked(juce::Button* b) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WhisperFreeWinAudioProcessorEditor)
};
