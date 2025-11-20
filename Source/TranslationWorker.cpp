#include "TranslationWorker.h"

TranslationWorker::TranslationWorker()
{
}

TranslationWorker::~TranslationWorker()
{
    stopWorker();
}

bool TranslationWorker::startWorker(const juce::File& pythonExe,
                                    const juce::File& scriptFile,
                                    std::function<void(const juce::String&)> logCallback)
{
    logCb = logCallback;

    if (!pythonExe.existsAsFile())
    {
        if (logCb) logCb("[TranslationWorker] Python executable not found: " + pythonExe.getFullPathName());
        return false;
    }

    if (!scriptFile.existsAsFile())
    {
        if (logCb) logCb("[TranslationWorker] translation_worker.py not found: " + scriptFile.getFullPathName());
        return false;
    }

    stopWorker();

    process = std::make_unique<juce::ChildProcess>();

    juce::StringArray args;
    args.add(pythonExe.getFullPathName());
    args.add(scriptFile.getFullPathName());

    if (!process->start(args))
    {
        if (logCb) logCb("[TranslationWorker] Failed to start Python worker.");
        process = nullptr;
        return false;
    }

    workerRunning = true;
    startTimer(50);  // poll stdout every 50ms

    if (logCb) logCb("[TranslationWorker] Worker started.");
    return true;
}

void TranslationWorker::stopWorker()
{
    if (process != nullptr)
    {
        if (logCb) logCb("[TranslationWorker] Stopping worker...");
        workerRunning = false;
        stopTimer();
        process->kill();
        process = nullptr;
    }
}

void TranslationWorker::translate(const juce::String& text,
                                  std::function<void(const juce::String&)> onResult)
{
    if (!workerRunning || process == nullptr)
    {
        if (logCb) logCb("[TranslationWorker] translate(): worker is not running.");
        return;
    }

    onResultCb = onResult;

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("text", text);

    juce::var json(juce::var(obj.get()));
    juce::String line = juce::JSON::toString(json) + "\n";

    /*if (!process->writeToProcessInput(line))
    {
        if (logCb) logCb("[TranslationWorker] Failed to write to python stdin");
    }*/
}

void TranslationWorker::timerCallback()
{
    if (!workerRunning || process == nullptr)
        return;

    juce::String output = process->readAllProcessOutput().trim();

    if (output.isNotEmpty())
    {
        if (logCb) logCb("[TranslationWorker] >>> " + output);

        juce::var parsed = juce::JSON::parse(output);
        if (auto* obj = parsed.getDynamicObject())
        {
            juce::String translated = obj->getProperty("translated").toString();
            if (translated.isNotEmpty() && onResultCb)
            {
                onResultCb(translated);
            }
        }
    }

    // Also detect worker death
    if (!process->isRunning())
    {
        workerRunning = false;
        stopTimer();
        if (logCb) logCb("[TranslationWorker] Worker died.");
    }
}
