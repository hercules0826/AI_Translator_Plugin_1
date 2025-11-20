// Source/TranslationEngine.cpp
#include "TranslationEngine.h"

TranslationEngine::TranslationEngine() = default;

TranslationEngine::~TranslationEngine()
{
    if (translator != nullptr)
    {
        marianDestroyTranslator(translator);
        translator = nullptr;
    }
}

bool TranslationEngine::initialise(const juce::File& modelDir, juce::String& errorMessage)
{
    if (!modelDir.exists() || !modelDir.isDirectory())
    {
        errorMessage = "Model directory does not exist: " + modelDir.getFullPathName();
        return false;
    }

    char errorBuf[512] = {};
    translator = marianCreateTranslator(modelDir.getFullPathName().toRawUTF8(),
        errorBuf,
        (int)sizeof(errorBuf));

    if (translator == nullptr)
    {
        errorMessage = "Marian initialisation failed: " + juce::String(errorBuf);
        return false;
    }

    errorMessage.clear();
    return true;
}

juce::String TranslationEngine::translate(const juce::String& input, std::function<void(const juce::String&)> logCb)
{
    if (translator == nullptr)
        return input;

    constexpr int kMaxOut = 8192;
    char outBuf[kMaxOut] = {};

    const juce::String inUtf8 = input;
    const bool ok = marianTranslate(translator,
        inUtf8.toRawUTF8(),
        outBuf,
        kMaxOut,
        logCb);

    if (!ok || outBuf[0] == '\0')
        return input;

    return juce::String(outBuf);
}
