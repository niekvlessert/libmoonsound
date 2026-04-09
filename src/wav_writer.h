#ifndef WAV_WRITER_H
#define WAV_WRITER_H

#include <stdio.h>
#include <stdint.h>

typedef struct {
    char chunkId[4];
    uint32_t chunkSize;
    char format[4];
    char subchunk1Id[4];
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char subchunk2Id[4];
    uint32_t subchunk2Size;
} WavHeader;

static inline void write_wav_header(FILE* f, uint32_t sampleRate, uint16_t numChannels, uint32_t numSamples) {
    WavHeader header;
    uint32_t dataSize = numSamples * numChannels * sizeof(int16_t);
    
    memcpy(header.chunkId, "RIFF", 4);
    header.chunkSize = 36 + dataSize;
    memcpy(header.format, "WAVE", 4);
    memcpy(header.subchunk1Id, "fmt ", 4);
    header.subchunk1Size = 16;
    header.audioFormat = 1; // PCM
    header.numChannels = numChannels;
    header.sampleRate = sampleRate;
    header.bitsPerSample = 16;
    header.byteRate = sampleRate * numChannels * (header.bitsPerSample / 8);
    header.blockAlign = numChannels * (header.bitsPerSample / 8);
    memcpy(header.subchunk2Id, "data", 4);
    header.subchunk2Size = dataSize;

    fwrite(&header, sizeof(WavHeader), 1, f);
}

static inline void update_wav_header(FILE* f, uint32_t numSamples) {
    uint32_t dataSize = numSamples * 2 * sizeof(int16_t); // Assuming 2 channels 16-bit
    uint32_t chunkSize = 36 + dataSize;
    
    fseek(f, 4, SEEK_SET);
    fwrite(&chunkSize, 4, 1, f);
    
    fseek(f, 40, SEEK_SET);
    fwrite(&dataSize, 4, 1, f);
    
    fseek(f, 0, SEEK_END);
}

#endif
