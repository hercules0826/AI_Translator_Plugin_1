// Source/PluginEditor.cpp
#include "PluginEditor.h"

WhisperFreeWinAudioProcessorEditor::WhisperFreeWinAudioProcessorEditor(WhisperFreeWinAudioProcessor& p)
    : AudioProcessorEditor(&p),
    processor(p),
    progressBar(progressValue)
{
    setSize(900, 500);

    addAndMakeVisible(loadWavButton);
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);
    addAndMakeVisible(sendButton);
    addAndMakeVisible(loadWhisperBtn);
    addAndMakeVisible(loadMarianBtn);
    addAndMakeVisible(autoTranslateToggle);

    loadWavButton.addListener(this);
    playButton.addListener(this);
    stopButton.addListener(this);
    sendButton.addListener(this);
    loadWhisperBtn.addListener(this);
    loadMarianBtn.addListener(this);

    autoTranslateToggle.onClick = [this]
        {
            processor.setAutoTranslate(autoTranslateToggle.getToggleState());
        };

    logBox.setMultiLine(true);
    logBox.setReadOnly(true);
    logBox.setScrollbarsShown(true);
    logBox.setFont(juce::Font(13.0f));
    logBox.setColour(juce::TextEditor::backgroundColourId, juce::Colours::black);
    logBox.setColour(juce::TextEditor::textColourId, juce::Colours::lawngreen);
    addAndMakeVisible(logBox);

    transcriptBox.setMultiLine(true);
    transcriptBox.setReadOnly(true);
    transcriptBox.setScrollbarsShown(true);
    transcriptBox.setFont(juce::Font(14.0f));
    addAndMakeVisible(transcriptBox);

    translationBox.setMultiLine(true);
    translationBox.setReadOnly(true);
    translationBox.setScrollbarsShown(true);
    translationBox.setFont(juce::Font(14.0f));
    addAndMakeVisible(translationBox);

    addAndMakeVisible(progressBar);

    processor.setLogSink([this](const juce::String& s) { appendLog(s); });
    processor.setTranscriptSink([this](const juce::String& t) { setTranscript(t); });
    processor.setTranslationSink([this](const juce::String& t) { setTranslation(t); });
    processor.setProgressSink([this](double p) { setProgress(p); });
}

void WhisperFreeWinAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
}

void WhisperFreeWinAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(8);

    auto top = area.removeFromTop(30);
    loadWavButton.setBounds(top.removeFromLeft(130).reduced(2));
    playButton.setBounds(top.removeFromLeft(70).reduced(2));
    stopButton.setBounds(top.removeFromLeft(70).reduced(2));
    sendButton.setBounds(top.removeFromLeft(150).reduced(2));
    loadWhisperBtn.setBounds(top.removeFromLeft(160).reduced(2));
    loadMarianBtn.setBounds(top.removeFromLeft(190).reduced(2));
    autoTranslateToggle.setBounds(top.removeFromLeft(160).reduced(2));

    area.removeFromTop(8);
    progressBar.setBounds(area.removeFromTop(20));
    area.removeFromTop(8);

    auto bottom = area.removeFromBottom(area.getHeight() / 2);
    logBox.setBounds(bottom);

    auto half = area;
    transcriptBox.setBounds(half.removeFromLeft(half.getWidth() / 2).reduced(4));
    translationBox.setBounds(half.reduced(4));
}

void WhisperFreeWinAudioProcessorEditor::buttonClicked(juce::Button* b)
{
    if (b == &loadWavButton)
    {
        chooser = std::make_unique<juce::FileChooser>("Select a WAV file...", juce::File(), "*.wav, *.mp3");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    processor.loadWavFile(f);
                }
            });
    }
    else if (b == &playButton)
    {
        processor.startPlayback();
    }
    else if (b == &stopButton)
    {
        processor.stopPlayback();
    }
    else if (b == &sendButton)
    {
        processor.sendLoadedBufferToWhisper();
    }
    else if (b == &loadWhisperBtn)
    {
        chooser = std::make_unique<juce::FileChooser>("Load a Whisper model...", juce::File(), "*.bin");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    processor.loadWhisperModel(f);
                }
            });
    }
    else if (b == &loadMarianBtn)
    {
        chooser = std::make_unique<juce::FileChooser>("Select Marian model folder...");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser& fc)
            {
                auto dir = fc.getResult();
                if (dir.isDirectory())
                {
                    processor.loadMarianModel(dir);
                }
            });
    }
}

void WhisperFreeWinAudioProcessorEditor::appendLog(const juce::String& s)
{
    logBox.moveCaretToEnd();
    logBox.insertTextAtCaret(
        "[" + juce::Time::getCurrentTime().formatted("%H:%M:%S") + "] " + s + "\n");
}

void WhisperFreeWinAudioProcessorEditor::setTranscript(const juce::String& s)
{
    transcriptBox.setText(s);
}

void WhisperFreeWinAudioProcessorEditor::setTranslation(const juce::String& s)
{
    translationBox.setText(s);
}

void WhisperFreeWinAudioProcessorEditor::setProgress(double p)
{
    progressValue = p;
}
