// Source/marian_c_api.cpp
#include "marian_c_api.h"

#include <string>
#include <vector>
#include <memory>
#include <cstring>

#include <ctranslate2/translator.h>
#include <sentencepiece_processor.h>
#include <juce_core/juce_core.h>
// #include <onnxruntime_cxx_api.h>

struct MarianTranslator
{
    std::unique_ptr<sentencepiece::SentencePieceProcessor> spSrc;
    std::unique_ptr<sentencepiece::SentencePieceProcessor> spTgt;
    std::unique_ptr<ctranslate2::Translator>               translator;
    std::string                                            modelDir;
};

static void writeError(char* buf, int size, const char* msg)
{
    if (!buf || size <= 0 || msg == nullptr)
        return;

    std::strncpy(buf, msg, static_cast<size_t>(size - 1));
    buf[size - 1] = '\0';
}

extern "C"
{

    MarianTranslator* marianCreateTranslator(const char* modelDirCStr,
        char* errorBuffer,
        int  errorBufferSize)
    {
        if (modelDirCStr == nullptr || modelDirCStr[0] == '\0')
        {
            writeError(errorBuffer, errorBufferSize, "modelDir is empty");
            return nullptr;
        }

        try
        {
            auto t = std::make_unique<MarianTranslator>();
            t->modelDir = modelDirCStr;

            const std::string spSrcPath = t->modelDir + "/source.spm";
            const std::string spTgtPath = t->modelDir + "/target.spm";
            const std::string ct2Path = t->modelDir + "/ct2-opus-mt-de-en";

            t->spSrc = std::make_unique<sentencepiece::SentencePieceProcessor>();
            t->spTgt = std::make_unique<sentencepiece::SentencePieceProcessor>();

            auto statusSrc = t->spSrc->Load(spSrcPath);
            if (!statusSrc.ok())
            {
                writeError(errorBuffer, errorBufferSize,
                    ("Failed loading source.spm: " + statusSrc.ToString()).c_str());
                return nullptr;
            }

            auto statusTgt = t->spTgt->Load(spTgtPath);
            if (!statusTgt.ok())
            {
                writeError(errorBuffer, errorBufferSize,
                    ("Failed loading target.spm: " + statusTgt.ToString()).c_str());
                return nullptr;
            }

            ctranslate2::Device device = ctranslate2::Device::CPU;
            ctranslate2::ComputeType ctype = ctranslate2::ComputeType::DEFAULT;

            t->translator = std::make_unique<ctranslate2::Translator>(
                ct2Path, device, ctype
            );

            return t.release();
        }
        catch (const std::exception& e)
        {
            writeError(errorBuffer, errorBufferSize, e.what());
            return nullptr;
        }
        catch (...)
        {
            writeError(errorBuffer, errorBufferSize, "Unknown exception in marianCreateTranslator");
            return nullptr;
        }
    }

    void marianDestroyTranslator(MarianTranslator* t)
    {
        delete t;
    }

    bool marianTranslate(MarianTranslator* t,
        const char* src,
        char* dstBuf,
        int               dstBufSize,
        std::function<void(const juce::String&)> logCb)
    {
        if (!t || !dstBuf || dstBufSize <= 0)
            return false;

        if (!src) src = "";

        try
        {
            if (logCb) logCb("[MT] Input: " + juce::String(src));
            std::vector<std::string> srcTokens;
            {
                std::vector<std::string> pieces;
                auto status = t->spSrc->Encode(src, &pieces);
                if (!status.ok()) {
                    if (logCb) logCb("[MT] SentencePiece encode failed: " + status.ToString());
                    return false;
                }
                srcTokens = std::move(pieces);
            }
            std::vector<std::vector<std::string>> batch;
            batch.emplace_back(std::move(srcTokens));
            /*ctranslate2::TranslationOptions opts;
            opts.output_type = ctranslate2::TranslationResult::hypotheses::Tokens;*/

            auto results = t->translator->translate_batch(batch);
            if (results.empty() || results[0].output().empty())
            {
                if (logCb) logCb("[MT] translate_batch returned empty!");
                return false;
            }
            const auto& outTokens = results[0].output();

            std::string detok;
            auto status = t->spTgt->Decode(outTokens, &detok);
            if (!status.ok()) {
                if (logCb) logCb("[MT] SentencePiece decode failed: " + status.ToString());
                return false;
            }
            if (logCb) logCb("[MT] Final translation: " + detok);

            if ((int)detok.size() + 1 > dstBufSize)
                detok.resize(static_cast<size_t>(dstBufSize - 1));

            std::memcpy(dstBuf, detok.c_str(), detok.size());
            dstBuf[detok.size()] = '\0';

            return true;
        }
        catch (const std::exception& e) {
            if (logCb) logCb("[MT] exception: " + juce::String(e.what()));
            return false;
        }
        catch (...) {
            if (logCb) logCb("[MT] unknown error");
            return false;
        }
    }

} // extern "C"
