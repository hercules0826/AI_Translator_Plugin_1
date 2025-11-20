// Source/PluginProcessor.cpp
#include "PluginProcessor.h"
#include "PluginEditor.h"

WhisperFreeWinAudioProcessor::WhisperFreeWinAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();

    // Optional: auto-init Marian with your fixed model path.
    // juce::String err;
    // auto modelDir = juce::File("D:/Models/opus-mt-de-en");
    // marianLoaded = translationEngine.initialise(modelDir, err);
    // if (err.isNotEmpty())
    //     appendLog("[MT] " + err);
}

WhisperFreeWinAudioProcessor::~WhisperFreeWinAudioProcessor()
{
    transport.stop();
    transport.setSource(nullptr);
    readerSource.reset();

    if (whisperThread)
    {
        whisperThread->signalThreadShouldExit();
        whisperThread->flushQueue();
        whisperThread->stopThread(3000);
        whisperThread.reset();
    }
}

void WhisperFreeWinAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate);
    transport.prepareToPlay(samplesPerBlock, getSampleRate());
}

void WhisperFreeWinAudioProcessor::releaseResources()
{
    transport.releaseResources();
}

bool WhisperFreeWinAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto main = layouts.getMainOutputChannelSet();
    return main == juce::AudioChannelSet::stereo() || main == juce::AudioChannelSet::mono();
}

void WhisperFreeWinAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);

    buffer.clear();
    juce::AudioSourceChannelInfo info(&buffer, 0, buffer.getNumSamples());
    transport.getNextAudioBlock(info);
}

juce::AudioProcessorEditor* WhisperFreeWinAudioProcessor::createEditor()
{
    return new WhisperFreeWinAudioProcessorEditor(*this);
}

// ==== App logic ====

bool WhisperFreeWinAudioProcessor::loadWhisperModel(const juce::File& modelFile)
{
    const bool ok = whisperEngine.loadModel(modelFile,
        [this](const juce::String& s) { appendLog(s); });

    if (ok && !whisperThread)
    {
        whisperThread = std::make_unique<WhisperThread>(
            whisperEngine,
            translationEngine,
            [this](double p) { handleProgress(p); },
            [this](const juce::String& s) { appendLog(s); },
            [this](const juce::String& t) { handleTranscript(t); },
            [this](const juce::String& t) { handleTranslation(t); }
        );
        whisperThread->setTranslatorLoaded(marianLoaded);
        whisperThread->startThread();
    }

    return ok;
}

bool WhisperFreeWinAudioProcessor::loadMarianModel(const juce::File& folder)
{
    juce::String err;
    marianLoaded = translationEngine.initialise(folder, err);

    if (err.isNotEmpty())
        appendLog("[MT] " + err);
    else
        appendLog("[MT] Marian model loaded from: " + folder.getFullPathName());

    if (whisperThread)
        whisperThread->setTranslatorLoaded(marianLoaded);

    return marianLoaded;
}

bool WhisperFreeWinAudioProcessor::loadWavFile(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader)
    {
        appendLog("Failed to open WAV: " + file.getFileName());
        return false;
    }

    transport.stop();
    readerSource.reset(new juce::AudioFormatReaderSource(reader.release(), true));
    const double fileRate = readerSource->getAudioFormatReader()->sampleRate;

    transport.setSource(readerSource.get(), 0, nullptr, fileRate);

    const juce::int64 total = readerSource->getAudioFormatReader()->lengthInSamples;
    if (total <= 0)
    {
        appendLog("Empty file");
        return false;
    }

    loadedSampleRate = fileRate;
    loadedMono.setSize(1, (int)total);
    loadedMono.clear();

    readerSource->getAudioFormatReader()->read(&loadedMono, 0, (int)total, 0, true, true);

    appendLog("Loaded WAV: " + file.getFileName() +
        " (" + juce::String(total) + " samples @ " +
        juce::String(fileRate) + " Hz)");
    return true;
}

void WhisperFreeWinAudioProcessor::startPlayback()
{
    transport.setPosition(0.0);
    transport.start();
}

void WhisperFreeWinAudioProcessor::stopPlayback()
{
    transport.stop();
}

bool WhisperFreeWinAudioProcessor::sendLoadedBufferToWhisper()
{
    if (!whisperEngine.isReady())
    {
        appendLog("Load Whisper model first.");
        return false;
    }

    if (loadedMono.getNumSamples() <= 0)
    {
        appendLog("Load a WAV file first.");
        return false;
    }

    if (!whisperThread)
    {
        appendLog("Internal error: WhisperThread not running.");
        return false;
    }

    appendLog("Sending buffer to Whisper (autoTranslate=" +
        juce::String(autoTranslate ? "true" : "false") + ")");

    whisperThread->sendBufferNow(loadedMono, loadedSampleRate, autoTranslate);
    return true;
}

void WhisperFreeWinAudioProcessor::appendLog(const juce::String& s)
{
    if (logSink)
    {
        juce::MessageManager::callAsync([sink = logSink, msg = s]
            {
                sink(msg);
            });
    }
}

void WhisperFreeWinAudioProcessor::handleTranscript(const juce::String& t)
{
    if (transcriptSink)
    {
        juce::MessageManager::callAsync([sink = transcriptSink, text = t]
            {
                sink(text);
            });
    }
}

void WhisperFreeWinAudioProcessor::handleTranslation(const juce::String& t)
{
    if (translationSink)
    {
        juce::MessageManager::callAsync([sink = translationSink, text = t]
            {
                sink(text);
            });
    }
}

void WhisperFreeWinAudioProcessor::handleProgress(double p)
{
    if (progressSink)
    {
        juce::MessageManager::callAsync([sink = progressSink, value = p]
            {
                sink(value);
            });
    }
}

// Required factory for JUCE plugin wrappers
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WhisperFreeWinAudioProcessor();
}
