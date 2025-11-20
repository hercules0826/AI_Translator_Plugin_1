#include "TranslationEngine.h"

TranslationEngine::TranslationEngine()
    : env(ORT_LOGGING_LEVEL_WARNING, "marian")
{
}

TranslationEngine::~TranslationEngine()
{
    session.reset();
}

bool TranslationEngine::loadModel(
    const juce::File& folder,
    std::function<void(const juce::String&)> logger)
{
    log = logger;

    auto modelFile = folder.getChildFile("model.onnx");
    auto srcSpm = folder.getChildFile("source.spm");
    auto tgtSpm = folder.getChildFile("target.spm");

    if (!modelFile.existsAsFile() ||
        !srcSpm.existsAsFile() ||
        !tgtSpm.existsAsFile())
    {
        log("Missing ONNX or SPM tokenizer files.");
        return false;
    }

    try
    {
        // ------------------------------------------------------------
        // ONNX Runtime 1.23.2 requires WIDE STRING path on Windows
        // ------------------------------------------------------------
        std::wstring wpath = modelFile.getFullPathName().toWideCharPointer();

        Ort::SessionOptions opts;
        opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        opts.SetIntraOpNumThreads(1);
        opts.SetInterOpNumThreads(1);

        // Load ONNX model (this automatically loads model.onnx.data)
        session = std::make_unique<Ort::Session>(
            env,
            wpath.c_str(),
            opts
        );

        log("ONNX session created.");
    }
    catch (const Ort::Exception& e)
    {
        log("Failed to load ONNX model: " + juce::String(e.what()));
        return false;
    }

    // ------------------------------------------------------------
    // Load SentencePiece models (correct API: Status.ok())
    // ------------------------------------------------------------
    auto s1 = spSource.Load(srcSpm.getFullPathName().toStdString());
    if (!s1.ok())
    {
        log("Failed to load source SPM: " + juce::String(s1.ToString().c_str()));
        return false;
    }

    auto s2 = spTarget.Load(tgtSpm.getFullPathName().toStdString());
    if (!s2.ok())
    {
        log("Failed to load target SPM: " + juce::String(s2.ToString().c_str()));
        return false;
    }

    log("MarianMT model + tokenizers loaded (ONNX Runtime 1.23.2).");
    return true;
}

// ============================================================================
// Encode input text → SentencePiece IDs
// ============================================================================
std::vector<int64_t> TranslationEngine::encode(const juce::String& text)
{
    std::vector<int> ids;
    spSource.Encode(text.toStdString(), &ids);
    return std::vector<int64_t>(ids.begin(), ids.end());
}

// ============================================================================
// Decode token IDs → SentencePiece string
// ============================================================================
juce::String TranslationEngine::decode(const std::vector<int64_t>& ids)
{
    std::vector<int> v(ids.begin(), ids.end());
    std::string out;
    spTarget.Decode(v, &out);
    return juce::String(out);
}

// ============================================================================
// Main translation call (greedy decoding)
// ============================================================================
juce::String TranslationEngine::translate(const juce::String& input)
{
    if (input.isEmpty() || !session)
        return {};

    auto inputIds = encode(input);

    // Shape: {1, sequence_length}
    const int64_t batch = 1;
    const int64_t seqLen = (int64_t)inputIds.size();
    std::array<int64_t, 2> shape = { batch, seqLen };

    Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator,
        OrtMemTypeDefault
    );

    Ort::Value inputTensor = Ort::Value::CreateTensor<int64_t>(
        mem,
        inputIds.data(),
        inputIds.size(),
        shape.data(),
        shape.size()
    );

    // ------------------------------------------------------------------------
    // Correct name lookups for ORT 1.23.2 (don’t hardcode!)
    // ------------------------------------------------------------------------
    Ort::AllocatorWithDefaultOptions allocator;

    Ort::AllocatedStringPtr inputNamePtr = session->GetInputNameAllocated(0, allocator);
    Ort::AllocatedStringPtr outputNamePtr = session->GetOutputNameAllocated(0, allocator);

    const char* inNames[] = { inputNamePtr.get() };
    const char* outNames[] = { outputNamePtr.get() };

    // ------------------------------------------------------------------------
    // Run ONNX inference
    // ------------------------------------------------------------------------
    auto output = session->Run(
        Ort::RunOptions{ nullptr },
        inNames, &inputTensor, 1,
        outNames, 1
    );

    // Extract logits
    float* logits = output[0].GetTensorMutableData<float>();

    auto info = output[0].GetTensorTypeAndShapeInfo();
    auto outShape = info.GetShape();

    const int64_t outBatch = outShape[0];
    const int64_t outSeq = outShape[1];
    const int64_t vocab = outShape[2];

    // ------------------------------------------------------------------------
    // Greedy decode entire sequence (token-by-token)
    // ------------------------------------------------------------------------
    std::vector<int64_t> toks;
    toks.reserve(outSeq);

    for (int64_t t = 0; t < outSeq; ++t)
    {
        const float* row = logits + (t * vocab);

        float bestVal = row[0];
        int bestId = 0;

        for (int v = 1; v < vocab; ++v)
        {
            if (row[v] > bestVal)
            {
                bestVal = row[v];
                bestId = v;
            }
        }

        toks.push_back(bestId);
    }

    return decode(toks);
}
