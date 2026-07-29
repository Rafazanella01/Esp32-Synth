#pragma once
#include <Arduino.h>

// Tipos de acorde disponiveis nas 4 teclas da ultima linha da matriz.
enum class ChordType : uint8_t {
    MAJOR = 0,
    MINOR,
    MINOR7,
    AUGMENTED,
    COUNT
};

static const uint8_t MAX_CHORD_NOTES = 4;

struct ChordDef {
    const char *name;                       // p/ mostrar no LCD
    uint8_t numNotes;
    int8_t intervals[MAX_CHORD_NOTES];      // semitons a partir da fundamental
};

// Maior:  1 3 5        (0, 4, 7)
// Menor:  1 b3 5       (0, 3, 7)
// Menor7: 1 b3 5 b7    (0, 3, 7, 10)
// Aumentado: 1 3 #5    (0, 4, 8)
static const ChordDef CHORD_TABLE[(int)ChordType::COUNT] = {
    { "Maior",     3, { 0, 4, 7,  0 } },
    { "Menor",     3, { 0, 3, 7,  0 } },
    { "Menor7",    4, { 0, 3, 7, 10 } },
    { "Aumentado", 3, { 0, 4, 8,  0 } },
};

inline const ChordDef &chordDef(ChordType t) {
    return CHORD_TABLE[(int)t];
}

// Nomes das notas na ordem das teclas 0..11 da matriz (linhas 1-3).
static const char *NOTE_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Frequencia da nota (indice 0..11, relativo a oitava de C4) em Hz.
inline float noteFrequency(uint8_t noteIndex, float baseC4) {
    return baseC4 * powf(2.0f, (float)noteIndex / 12.0f);
}
