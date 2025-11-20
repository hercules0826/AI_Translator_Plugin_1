// Source/TranslationEngine.h
#pragma once

#include <juce_core/juce_core.h>
#include "marian_c_api.h"

class TranslationEngine
{
public:
    TranslationEngine();
    ~TranslationEngine();

    bool initialise(const juce::File& modelDir, juce::String& errorMessage);
    bool isReady() const noexcept { return translator != nullptr; }

    juce::String translate(const juce::String& input,
        std::function<void(const juce::String&)> logCb);

private:
    MarianTranslator* translator = nullptr;
};
