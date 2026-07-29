#pragma once
#include <Arduino.h>
#include "Chords.h"
#include "Waveforms.h"

struct SynthStatus {
    bool playing;
    uint8_t rootNote;     // 0..11, valido apenas se playing == true
    ChordType chordType;
    Waveform timbre;
    int bpm;
};

void displayInit();

// Troca de pagina do LCD: -1 (anterior) / +1 (proxima) / 0 (sem troca).
// Paginas: 0 = status (nota/acorde/timbre/bpm), 1 = ajuda (legenda das teclas).
void displaySetPage(int8_t delta);

// Redesenha o LCD somente se algo relevante mudou desde a ultima chamada.
void displayUpdate(const SynthStatus &status);
