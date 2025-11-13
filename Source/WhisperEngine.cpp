#include "WhisperEngine.h"
#include <juce_dsp/juce_dsp.h>

// Include whisper.cpp C API
extern "C" {
#include <whisper.h>
}

WhisperEngine::WhisperEngine() {}
WhisperEngine::~WhisperEngine()
{
    if (ctx) { whisper_free(ctx); ctx = nullptr; }
}

bool WhisperEngine::loadModel(const juce::File& modelFile,
    std::function<void(const juce::String&)> logCb)
{
    if (!modelFile.existsAsFile())
    {
        if (logCb) logCb("[Whisper] Model not found: " + modelFile.getFullPathName());
        return false;
    }

    // Free old model
    if (ctx != nullptr)
    {
        whisper_free(ctx);
        ctx = nullptr;
    }

    if (logCb) logCb("[Whisper] Loading model: " + modelFile.getFileName());

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = false;  // Safe default

    ctx = whisper_init_from_file_with_params(modelFile.getFullPathName().toRawUTF8(), cparams);
    if (ctx == nullptr)
    {
        if (logCb) logCb("[Whisper] Failed to load Whisper model");
        return false;
    }

    modelPath = modelFile;

    if (logCb) logCb("[Whisper] Model loaded successfully");
    return true;
}

//
// Force resample to exactly 16 kHz
//
juce::AudioBuffer<float> WhisperEngine::resampleTo16k(const juce::AudioBuffer<float>& in, double inRate,
    std::function<void(const juce::String&)> logCb)
{
    constexpr double target = 16000.0;

    const int inSamples = in.getNumSamples();
    if (inSamples <= 0)
    {
        if (logCb) logCb("[Whisper] Empty input buffer for resample");
        return {};
    }

    // Mixdown to mono
    juce::AudioBuffer<float> mono(1, inSamples);
    mono.clear();
    for (int ch = 0; ch < in.getNumChannels(); ++ch)
        mono.addFrom(0, 0, in, ch, 0, in.getNumSamples(), 1.0f / in.getNumChannels());

    // Normalize amplitude
    float peak = mono.getMagnitude(0, mono.getNumSamples());
    if (peak > 0.0f)
        mono.applyGain(0.8f / peak);

    if (std::abs(inRate - target) < 1.0)
    {
        if (logCb) logCb("[Whisper] Input already 16kHz");
        return mono;
    }

    const double ratio = target / inRate;  // 8k → 16k = 2.0, 48k → 16k ≈ 0.3333
    const int outSamples = (int)std::ceil(mono.getNumSamples() * ratio);

    if (logCb)
    {
        logCb("[Whisper] Resampling: " +
            juce::String(inRate) + " Hz → 16000 Hz, ratio = " + juce::String(ratio, 4));
        logCb("[Whisper] Input samples: " + juce::String(inSamples) +
            " → Output samples: " + juce::String(outSamples));
    }

    juce::AudioBuffer<float> out(1, outSamples);
    out.clear();

    //// --- High-quality resampling (better than Lagrange for large rate differences)
    //juce::dsp::ResamplingAudioSource resampler(nullptr, false, 1);
    //juce::AudioSourceChannelInfo info(&out, 0, outSamples);

    //juce::AudioBuffer<float> temp(in);
    //juce::AudioSourceChannelInfo inputInfo(&temp, 0, inSamples);

    //juce::dsp::AudioBlock<float> block(temp);
    //juce::dsp::ProcessContextReplacing<float> ctx(block);


    juce::LagrangeInterpolator interp;
    interp.reset();
    int produced = interp.process(ratio,
        mono.getReadPointer(0),
        out.getWritePointer(0),
        mono.getNumSamples());

    if (logCb)
        logCb("[Whisper] Resample produced " + juce::String(produced) + " samples");

    return out;
}

juce::String WhisperEngine::transcribe(const juce::AudioBuffer<float>& monoIn,
    double sampleRate,
    std::function<void(double)> progressCb,
    std::function<void(const juce::String&)> logCb)
{
    if (ctx == nullptr)
    {
        if (logCb) logCb("[Whisper] No model loaded.");
        return {};
    }

    if (monoIn.getNumChannels() != 1)
    {
        if (logCb) logCb("[Whisper] Buffer is not mono");
        return {};
    }

    if (monoIn.getNumSamples() == 0)
    {
        if (logCb) logCb("[Whisper] Buffer is empty");
        return {};
    }

    // 1) FORCE RESAMPLE TO 16K
    //juce::AudioBuffer<float> mono16k = resampleTo16k(monoIn, sampleRate, logCb);
    juce::AudioBuffer<float> mono16k = monoIn;
    if (mono16k.getNumSamples() == 0)
    {
        if (logCb) logCb("[Whisper] Resample failed");
        return {};
    }

    const int nSamples = mono16k.getNumSamples();

    if (logCb)
    {
        logCb("[Whisper] Running inference on " +
            juce::String(nSamples) + " samples @ 16000 Hz");
    }

    // 2) Copy to vector<float> (required by whisper.cpp)
    std::vector<float> pcm;
    pcm.assign(mono16k.getReadPointer(0),
        mono16k.getReadPointer(0) + nSamples);

    if (progressCb) progressCb(0.05);

    // 3) Configure whisper params
    whisper_full_params wparams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    wparams.print_realtime = false;
    wparams.print_progress = false;
    wparams.print_timestamps = false;

    wparams.language = "auto";
    wparams.translate = false;
    wparams.n_threads = juce::jmax(1, juce::SystemStats::getNumCpus() - 1);

    // Forward progress from Whisper to UI
    wparams.progress_callback = [](whisper_context*, whisper_state*, int progress, void* user_data)
        {
            auto* cb = reinterpret_cast<std::function<void(double)>*>(user_data);
            if (cb && *cb)
                (*cb)(juce::jlimit(0.0, 1.0, progress / 100.0));
        };
    wparams.progress_callback_user_data = &progressCb;

    // 4) Run whisper
    int rc = whisper_full(ctx, wparams, pcm.data(), pcm.size());
    if (rc != 0)
    {
        if (logCb) logCb("[Whisper] whisper_full failed: " + juce::String(rc));
        if (progressCb) progressCb(0.0);
        return {};
    }

    if (progressCb) progressCb(1.0);

    // 5) Extract output text
    juce::String out;
    const int nSegments = whisper_full_n_segments(ctx);

    for (int i = 0; i < nSegments; i++)
    {
        out += whisper_full_get_segment_text(ctx, i);
        if (i + 1 < nSegments)
            out += " ";
    }

    out = out.trim();

    if (logCb)
    {
        logCb("[Whisper] Done. Segments = " + juce::String(nSegments));
        logCb("[Whisper] Transcript = " + out);
    }

    return out;
}