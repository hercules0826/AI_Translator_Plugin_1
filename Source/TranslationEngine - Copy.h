#pragma once

#include <juce_core/juce_core.h>
#include <onnxruntime_cxx_api.h>
#include "sentencepiece_processor.h"
#include <mutex>
#include <vector>

class TranslationEngine
{
public:
    using LogFn = std::function<void(const juce::String&)>;

    TranslationEngine();
    ~TranslationEngine();

    /** Load ONNX (model.onnx + .data) and SentencePiece tokenizers */
    bool loadModel(const juce::File& folder, LogFn logger = nullptr);

    /** Translate German → English using ONNX Runtime + greedy decoding */
    juce::String translate(const juce::String& germanText);

private:
    // --------------------------------------------------------------------
    //  ONNX Runtime Members
    // --------------------------------------------------------------------
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;
    Ort::SessionOptions sessionOptions;

    // --------------------------------------------------------------------
    //  Tokenizers
    // --------------------------------------------------------------------
    sentencepiece::SentencePieceProcessor spSource;
    sentencepiece::SentencePieceProcessor spTarget;

    // --------------------------------------------------------------------
    //  Synchronization + logging
    // --------------------------------------------------------------------
    LogFn log;
    std::mutex mutex;

    // --------------------------------------------------------------------
    //  Internal helpers
    // --------------------------------------------------------------------
    std::vector<int64_t> encode(const juce::String& text);
    juce::String decode(const std::vector<int64_t>& ids);

    /** Greedy decode logits → token IDs */
    std::vector<int64_t> greedyDecode(
        const float* logits,
        int64_t sequenceLen,
        int64_t vocabSize
    );
};
