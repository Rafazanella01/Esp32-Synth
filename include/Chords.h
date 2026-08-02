#pragma once
#include <Arduino.h>

// Qualidade do acorde aplicada sobre a nota tocada.
//
// NATURAL e reservado pro modo "segurar a tecla" (nota pura, sustentada) -
// o joystick nunca seleciona ele. As outras 10 qualidades sao escolhidas
// ao vivo pelo joystick: centro + 8 direcoes (incluindo diagonais) +
// clique. Ver Input.h pro mapeamento exato de cada posicao.
enum class ChordType : uint8_t {
    NATURAL = 0,  // so a nota, sustentada - usado ao SEGURAR a tecla
    MAJOR,        // joystick no centro (acorde padrao do arpejo)
    MAJOR7,       // cima (N)
    DOMINANT7,    // cima-direita (NE)
    AUGMENTED,    // direita (E)
    SUS4,         // baixo-direita (SE)
    MINOR7,       // baixo (S)
    MINOR,        // baixo-esquerda (SW)
    DIMINISHED,   // esquerda (W)
    SUS2,         // cima-esquerda (NW)
    SIXTH,        // clique do joystick
    COUNT
};

static const uint8_t MAX_CHORD_NOTES = 4;

struct ChordDef {
    const char *name;                       // p/ mostrar no LCD
    uint8_t numNotes;
    int8_t intervals[MAX_CHORD_NOTES];      // semitons a partir da fundamental
};

// Natural:    1            (0)              - so a nota, sustentada, sem arpejo
// Maior:      1 3 5        (0, 4, 7)
// Maior7:     1 3 5 7      (0, 4, 7, 11)
// Dominante7: 1 3 5 b7     (0, 4, 7, 10)     - o "7" simples (blues/funk)
// Aumentado:  1 3 #5       (0, 4, 8)
// Sus4:       1 4 5        (0, 5, 7)
// Menor7:     1 b3 5 b7    (0, 3, 7, 10)
// Menor:      1 b3 5       (0, 3, 7)
// Diminuto:   1 b3 b5      (0, 3, 6)
// Sus2:       1 2 5        (0, 2, 7)
// Sexta:      1 3 5 6      (0, 4, 7, 9)
static const ChordDef CHORD_TABLE[(int)ChordType::COUNT] = {
    { "Natural",    1, { 0,  0,  0,  0 } },
    { "Maior",      3, { 0,  4,  7,  0 } },
    { "Maior7",     4, { 0,  4,  7, 11 } },
    { "Dominante7", 4, { 0,  4,  7, 10 } },
    { "Aumentado",  3, { 0,  4,  8,  0 } },
    { "Sus4",       3, { 0,  5,  7,  0 } },
    { "Menor7",     4, { 0,  3,  7, 10 } },
    { "Menor",      3, { 0,  3,  7,  0 } },
    { "Diminuto",   3, { 0,  3,  6,  0 } },
    { "Sus2",       3, { 0,  2,  7,  0 } },
    { "Sexta",      4, { 0,  4,  7,  9 } },
};

inline const ChordDef &chordDef(ChordType t) {
    return CHORD_TABLE[(int)t];
}

// Nomes das notas na ordem das teclas 0..15 da matriz - uma oitava cromatica
// (C4..B4) mais 4 notas extras (C5..D#5), ja que as 16 teclas sao todas notas.
static const char *NOTE_NAMES[16] = {
    "C4", "C#4", "D4", "D#4", "E4", "F4", "F#4", "G4", "G#4", "A4", "A#4", "B4",
    "C5", "C#5", "D5", "D#5"
};

// Frequencia da nota (indice 0..15, relativo a C4) em Hz. O expoente cresce
// livremente, entao indices >= 12 caem naturalmente na oitava seguinte.
inline float noteFrequency(uint8_t noteIndex, float baseC4) {
    return baseC4 * powf(2.0f, (float)noteIndex / 12.0f);
}
