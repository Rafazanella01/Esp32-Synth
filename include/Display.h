#pragma once
#include <Arduino.h>
#include "Chords.h"
#include "Waveforms.h"

struct SynthStatus {
    bool playing;
    uint8_t rootNote;     // 0..15, valido apenas se playing == true
    ChordType chordType;
    Waveform timbre;
    int bpm;
};

void displayInit();

// Redesenha o LCD somente se algo relevante mudou desde a ultima chamada.
void displayUpdate(const SynthStatus &status);
