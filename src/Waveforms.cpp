#include "Waveforms.h"

int16_t waveTables[(int)Waveform::COUNT][WAVETABLE_SIZE];

void generateWaveTables() {
    for (int i = 0; i < WAVETABLE_SIZE; i++) {
        float phase = (float)i / (float)WAVETABLE_SIZE; // 0..1

        // Seno puro
        waveTables[(int)Waveform::SINE][i] =
            (int16_t)(sinf(phase * 2.0f * PI) * 32000.0f);

        // Triangular
        float tri = (phase < 0.5f) ? (phase * 4.0f - 1.0f) : (3.0f - phase * 4.0f);
        waveTables[(int)Waveform::TRIANGLE][i] = (int16_t)(tri * 32000.0f);

        // Dente de serra
        float saw = phase * 2.0f - 1.0f;
        waveTables[(int)Waveform::SAW][i] = (int16_t)(saw * 32000.0f);

        // Quadrada (com leve suavizacao das bordas nao e feita aqui de proposito -
        // e um som mais "cru", proposital para dar contraste com o seno)
        float sq = (phase < 0.5f) ? 1.0f : -1.0f;
        waveTables[(int)Waveform::SQUARE][i] = (int16_t)(sq * 28000.0f); // um pouco mais baixo, e mais "presente"
    }
}

const char *waveformName(Waveform w) {
    switch (w) {
        case Waveform::SINE:     return "Seno";
        case Waveform::TRIANGLE: return "Triang.";
        case Waveform::SAW:      return "Dente-Serra";
        case Waveform::SQUARE:   return "Quadrada";
        default:                 return "?";
    }
}

Waveform nextWaveform(Waveform w) {
    uint8_t n = ((uint8_t)w + 1) % (uint8_t)Waveform::COUNT;
    return (Waveform)n;
}
