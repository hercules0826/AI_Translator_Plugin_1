#include "TranslationController.h"

TranslationController::TranslationController()
{
    whisper.setResultCallback([this](const juce::String& text)
    {
        asrResultHandler(text);
    });
}

TranslationController::~TranslationController()
{
    stop();
}

void TranslationController::prepare(double sampleRate, int numChannels, int blockSize)
{
    whisper.prepare(sampleRate, numChannels, blockSize);
}

bool TranslationController::loadWhisperModel(const juce::File& modelFile)
{
    return whisper.loadModel(modelFile, [this](const juce::String& msg)
    {
        if (logCallback) logCallback(msg);
    });
}

bool TranslationController::loadMarianModel(const juce::File& modelDir)
{
    return marian.loadModel(modelDir, [this](const juce::String& msg)
    {
        if (logCallback) logCallback(msg);
    });
}

void TranslationController::start()
{
    if (isRunning.load())
        return;

    isRunning.store(true);
    whisper.startTranscription();

    translationThread = std::thread([this] { translationLoop(); });
}

void TranslationController::stop()
{
    if (! isRunning.load())
        return;

    whisper.stop();

    isRunning.store(false);
    queueCv.notify_all();

    if (translationThread.joinable())
        translationThread.join();
}

void TranslationController::pushAudio(const juce::AudioBuffer<float>& buffer)
{
    whisper.pushAudioBlock(buffer);
}

void TranslationController::asrResultHandler(const juce::String& text)
{
    {
        std::lock_guard<std::mutex> lg(textMutex);
        lastASR = text;
    }

    {
        std::lock_guard<std::mutex> ql(queueMutex);
        translateQueue.push(text);
    }
    queueCv.notify_one();
}

void TranslationController::translationLoop()
{
    while (isRunning.load())
    {
        juce::String textToTranslate;

        {
            std::unique_lock<std::mutex> lk(queueMutex);
            queueCv.wait(lk, [this]
            {
                return ! translateQueue.empty() || ! isRunning.load();
            });

            if (! isRunning.load())
                break;

            if (! translateQueue.empty())
            {
                textToTranslate = translateQueue.front();
                translateQueue.pop();
            }
        }

        if (textToTranslate.isEmpty())
            continue;

        auto translated = marian.translate(textToTranslate);

        {
            std::lock_guard<std::mutex> lg(textMutex);
            lastTranslation = translated;
        }

        if (uiCallback)
            uiCallback(textToTranslate, translated);
    }
}
