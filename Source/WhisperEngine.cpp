#include "WhisperEngine.h"
#include <juce_dsp/juce_dsp.h>

// Include whisper.cpp C API
extern "C" {
#include <whisper.h>
}

static void logMsg(const std::function<void(const juce::String&)>& log,
    const juce::String& s)
{
    if (log) log(s);
}

WhisperEngine::WhisperEngine() {}
WhisperEngine::~WhisperEngine()
{
    if (ctx) { whisper_free(ctx); ctx = nullptr; }
}

bool WhisperEngine::loadModel(const juce::File& modelFile,
    std::function<void(const juce::String&)> logFn)
{
    if (!modelFile.existsAsFile())
    {
        logMsg(logFn, "[Whisper] Model not found: " + modelFile.getFullPathName());
        return false;
    }

    if (ctx != nullptr)
    {
        whisper_free(ctx);
        ctx = nullptr;
    }

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;

    ctx = whisper_init_from_file_with_params(modelFile.getFullPathName().toRawUTF8(),
        cparams);
    if (!ctx)
    {
        logMsg(logFn, "[Whisper] Failed to load model");
        return false;
    }

    logMsg(logFn, "[Whisper] Model loaded: " + modelFile.getFileName());
    return true;
}

juce::AudioBuffer<float> WhisperEngine::resampleTo16k(
    const juce::AudioBuffer<float>& in,
    double inRate,
    std::function<void(const juce::String&)>& logCb)
{
    constexpr double targetRate = 16000.0;

    if (in.getNumChannels() != 1 || inRate <= 0.0 || in.getNumSamples() <= 0)
    {
        logMsg(logCb, "[Whisper] resampleTo16k: invalid input");
        return {};
    }

    if (std::abs(inRate - targetRate) < 1.0)
        return in; // already close enough

    const int inSamples = in.getNumSamples();
    const double ratio = targetRate / inRate;
    const int outSamples = (int)std::ceil(inSamples * ratio) + 8;

    juce::AudioBuffer<float> out(1, outSamples);
    out.clear();

    juce::LagrangeInterpolator interp;
    const int produced = interp.process((float)ratio,
        in.getReadPointer(0),
        out.getWritePointer(0),
        inSamples);

    if (produced <= 0)
    {
        logMsg(logCb, "[Whisper] Resample produced 0 samples");
        return {};
    }

    if (produced < outSamples)
        out.setSize(1, produced, true, true, true);

    logMsg(logCb, "[Whisper] Resampled " + juce::String(inRate, 2) +
        " Hz -> 16kHz, " + juce::String(produced) + " samples");
    return out;
}

juce::String WhisperEngine::transcribe(const juce::AudioBuffer<float>& monoIn,
    double sampleRate,
    std::function<void(double)> progressCb,
    std::function<void(const juce::String&)> logCb)
{
    if (!ctx)
    {
        logMsg(logCb, "[Whisper] No model loaded");
        return {};
    }

    if (monoIn.getNumChannels() != 1 || monoIn.getNumSamples() <= 0)
    {
        logMsg(logCb, "[Whisper] Expected non-empty mono buffer");
        return {};
    }

    auto mono16 = resampleTo16k(monoIn, sampleRate,
        const_cast<std::function<void(const juce::String&)>&>(logCb));
    if (mono16.getNumSamples() <= 0)
        return {};

    const int nSamples = mono16.getNumSamples();
    std::vector<float> pcm(nSamples);
    std::memcpy(pcm.data(), mono16.getReadPointer(0), sizeof(float) * (size_t)nSamples);

    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;
    wparams.translate = false;
    wparams.language = "auto";
    wparams.n_threads = juce::jmax(1, juce::SystemStats::getNumCpus() - 1);

    wparams.progress_callback = [](whisper_context*, whisper_state*, int progress, void* user_data)
        {
            auto* cb = reinterpret_cast<std::function<void(double)>*>(user_data);
            if (cb && *cb)
                (*cb)(juce::jlimit(0.0, 1.0, progress / 100.0));
        };
    wparams.progress_callback_user_data = &progressCb;

    if (progressCb) progressCb(0.02);

    const int rc = whisper_full(ctx, wparams, pcm.data(), pcm.size());
    if (rc != 0)
    {
        logMsg(logCb, "[Whisper] whisper_full failed: " + juce::String(rc));
        if (progressCb) progressCb(0.0);
        return {};
    }

    if (progressCb) progressCb(1.0);

    juce::String transcript;
    const int nSegments = whisper_full_n_segments(ctx);
    for (int i = 0; i < nSegments; ++i)
    {
        transcript += whisper_full_get_segment_text(ctx, i);
        if (i + 1 < nSegments)
            transcript += " ";
    }

    transcript = transcript.trim();
    logMsg(logCb, "[Whisper] Transcript: " + transcript);
    return transcript;
}