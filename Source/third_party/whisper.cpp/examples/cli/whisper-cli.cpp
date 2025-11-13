#include "whisper.h"
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <cstdint>

static bool load_wav(const std::string &path, std::vector<float> &pcm) {
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) return false;

    uint32_t chunkId, riffType, chunkSize;
    uint16_t audioFormat, numChannels;
    uint32_t sampleRate, byteRate;
    uint16_t blockAlign, bitsPerSample;

    file.read((char*)&chunkId,       4);
    file.read((char*)&chunkSize,     4);
    file.read((char*)&riffType,      4);

    // Skip fmt chunk header:
    file.seekg(12);

    file.read((char*)&chunkId,       4); // "fmt "
    file.read((char*)&chunkSize,     4);
    file.read((char*)&audioFormat,   2);
    file.read((char*)&numChannels,   2);
    file.read((char*)&sampleRate,    4);
    file.read((char*)&byteRate,      4);
    file.read((char*)&blockAlign,    2);
    file.read((char*)&bitsPerSample, 2);

    // Skip extra fmt bytes if any
    if (chunkSize > 16) file.seekg(chunkSize - 16, std::ios::cur);

    // Seek until "data" chunk
    while (true) {
        file.read((char*)&chunkId, 4);
        file.read((char*)&chunkSize, 4);
        if (chunkId == 0x64617461) break; // "data"
        file.seekg(chunkSize, std::ios::cur);
    }

    // Read PCM samples:
    int numSamples = chunkSize / (bitsPerSample / 8);
    std::vector<int16_t> tmp(numSamples);

    file.read((char*)tmp.data(), chunkSize);

    pcm.reserve(numSamples);
    for (int i = 0; i < numSamples; i++) {
        pcm.push_back(tmp[i] / 32768.0f);
    }

    return true;
}

int main(int argc, char **argv) {

    std::string model;
    std::string audio;

    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "-m" && i + 1 < argc) model = argv[++i];
        else if (a == "-f" && i + 1 < argc) audio = argv[++i];
    }

    if (model.empty() || audio.empty()) {
        printf("Usage: whisper-cli -m model.bin -f audio.wav\n");
        return 1;
    }

    printf("Loading model: %s\n", model.c_str());
    whisper_context *ctx = whisper_init_from_file(model.c_str());
    if (!ctx) {
        printf("Failed to load model\n");
        return 1;
    }

    std::vector<float> pcm;
    if (!load_wav(audio, pcm)) {
        printf("Failed to load WAV: %s\n", audio.c_str());
        return 1;
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_realtime = false;
    params.print_progress = false;
    params.print_timestamps = false;

    printf("Running inference...\n");
    if (whisper_full(ctx, params, pcm.data(), pcm.size()) != 0) {
        printf("Whisper inference failed.\n");
        whisper_free(ctx);
        return 1;
    }

    printf("\n--- Transcript ---\n");
    int n = whisper_full_n_segments(ctx);
    for (int i = 0; i < n; i++) {
        printf("%s", whisper_full_get_segment_text(ctx, i));
    }
    printf("\n");

    whisper_free(ctx);
    return 0;
}
