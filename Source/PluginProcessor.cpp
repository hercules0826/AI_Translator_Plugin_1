#include "PluginProcessor.h"
#include "PluginEditor.h"

WhisperFreeWinAudioProcessor::WhisperFreeWinAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.registerBasicFormats();

    // Fixed local Whisper endpoint
    /*juce::URL endpoint("http://127.0.0.1:8000/asr");

    whisper = std::make_unique<WhisperThread>(
        endpoint,
        [this](double p) { if (progressSink)
        juce::MessageManager::callAsync([sink = progressSink, p] { sink(p); }); },
        [this](const juce::String& s) { appendLog(s); },
        [this](juce::String t) { onTranscript(t); });*/
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

void WhisperFreeWinAudioProcessor::appendLog(const juce::String& s)
{
    if (logSink)
        juce::MessageManager::callAsync([sink = logSink, msg = s] { sink(msg); });
}

void WhisperFreeWinAudioProcessor::onTranscript(const juce::String& t)
{
    if (transcriptSink)
        juce::MessageManager::callAsync([sink = transcriptSink, t] { sink(t); });
}

void WhisperFreeWinAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Nothing special; we use file playback
    juce::ignoreUnused(samplesPerBlock);
    transport.prepareToPlay(samplesPerBlock, sampleRate);
}

void WhisperFreeWinAudioProcessor::releaseResources()
{
    transport.releaseResources();
}

bool WhisperFreeWinAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    auto out = layouts.getMainOutputChannelSet();
    auto in = layouts.getMainInputChannelSet();
    return out == juce::AudioChannelSet::stereo() && (in.isDisabled() || in == juce::AudioChannelSet::stereo());
}

void WhisperFreeWinAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // Just pass-through (or clear) for now
    juce::ignoreUnused(midi);

    juce::ScopedNoDenormals noDenormals;
    buffer.clear(); // this plugin is utility; no live processing
    juce::AudioSourceChannelInfo info(&buffer, 0, buffer.getNumSamples());
    transport.getNextAudioBlock(info);
}

bool WhisperFreeWinAudioProcessor::loadWavFile(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (!reader) { appendLog("Failed to open: " + file.getFileName()); return false; }

    // Transport (playback will be at the file's rate; host resamples as needed)
    transport.stop();
    readerSource.reset(new juce::AudioFormatReaderSource(reader.release(), true));
    transport.setSource(readerSource.get(), 0, nullptr, readerSource->getAudioFormatReader()->sampleRate);

    // Keep mono copy for upload + remember rate
    loadedSampleRate = readerSource->getAudioFormatReader()->sampleRate;
    const juce::int64 total = readerSource->getAudioFormatReader()->lengthInSamples;
    if (total <= 0) { appendLog("Empty file"); return false; }

    loadedMono.setSize(1, (int)total);
    loadedMono.clear();
    readerSource->getAudioFormatReader()->read(&loadedMono, 0, (int)total, 0, true, true);

    appendLog("Loaded: " + file.getFileName() + " (" + juce::String(total) + " samples @ "
        + juce::String(loadedSampleRate) + " Hz)");
    return true;
}

void WhisperFreeWinAudioProcessor::play() { transport.start(); }
void WhisperFreeWinAudioProcessor::stop() { transport.stop(); }

// ===== NEW: simple, safe entrypoint for the UI
bool WhisperFreeWinAudioProcessor::sendLoadedBufferToWhisper()
{
    if (!engine.isReady()) { appendLog("Load a Whisper model first."); return false; }
    if (!hasLoadedAudio()) { appendLog("Load a WAV first."); return false; }

    if (!whisperThread)
    {
        whisperThread = std::make_unique<WhisperThread>(
            engine,
            [this](double p) { onProgress(p); },
            [this](const juce::String& s) { appendLog(s); },
            [this](juce::String t) { onTranscript(t); }
        );
        whisperThread->startThread();
    }

    appendLog("[ASR] Sending current WAV to Whisper...");
    whisperThread->sendBufferNow(loadedMono, loadedSampleRate, 0.0);
    return true;
}

juce::AudioProcessorEditor* WhisperFreeWinAudioProcessor::createEditor()
{
    return new WhisperFreeWinAudioProcessorEditor(*this);
}

// Required factory for JUCE plugin wrappers
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WhisperFreeWinAudioProcessor();
}
