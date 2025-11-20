// Source/marian_c_api.h
#pragma once
#include <juce_core/juce_core.h>

extern "C"
{
    struct MarianTranslator; // opaque handle

    MarianTranslator* marianCreateTranslator(const char* modelDir,
        char* errorBuffer,
        int  errorBufferSize);

    void marianDestroyTranslator(MarianTranslator* t);

    bool marianTranslate(MarianTranslator* t,
        const char* src,
        char* dstBuf,
        int               dstBufSize,
        std::function<void(const juce::String&)> logCb);
}
