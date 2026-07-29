#pragma once
#include <Arduino.h>

// Timbres disponiveis. Cada um e uma wavetable de 256 amostras pre-calculada.
enum class Waveform : uint8_t {
    SINE = 0,
    TRIANGLE,
    SAW,
    SQUARE,
    COUNT // nao e um timbre, so marca o total
};

static const int WAVETABLE_SIZE = 256;

// waveTables[timbre][amostra] -> valores int16_t (-32767..32767)
extern int16_t waveTables[(int)Waveform::COUNT][WAVETABLE_SIZE];

// Preenche todas as wavetables. Chamar uma vez no setup().
void generateWaveTables();

// Nome curto (max 8 chars) para mostrar no LCD.
const char *waveformName(Waveform w);

// Timbre seguinte, com wrap-around (usado pelo pote/joystick de timbre).
Waveform nextWaveform(Waveform w);
