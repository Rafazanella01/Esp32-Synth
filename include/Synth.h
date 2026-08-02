#pragma once
#include <Arduino.h>
#include "Chords.h"
#include "Waveforms.h"

// Motor de audio: I2S + oscilador por wavetable + arpejador.
// synthProcess() deve ser chamado o mais frequente possivel no loop() -
// e ele quem alimenta o DMA do I2S (a chamada bloqueia levemente, o que
// acaba servindo de "metronomo" natural pro loop principal).
//
// Dois jeitos de tocar o MESMO acorde, escolhidos por synthSetPlayMode():
//
//   SUSTAINED - todas as notas do acorde soam juntas e ficam seguradas
//               (acorde em bloco). E o modo de "segurar a tecla".
//   ARPEGGIO  - as notas do acorde tocam uma de cada vez, em ciclo, no
//               andamento do BPM. E o modo de "toque rapido".
//
// A qualidade do acorde (synthSetChordType) e independente do modo: da pra
// trocar de acorde no meio, e trocar de modo, sem cortar/reiniciar o som.
//
// Transicoes suaves: trocar de nota desliza a afinacao (glide/portamento)
// em vez de saltar seco, e o timbre e uma mistura continua entre as
// wavetables adjacentes (nao troca abrupta entre formas de onda).

enum class PlayMode : uint8_t {
    SUSTAINED, // acorde em bloco, segurado
    ARPEGGIO   // notas do acorde em ciclo, no tempo do BPM
};

void synthInit();
void synthProcess();

void synthNoteOn(uint8_t rootNote);   // liga/troca a nota (0..15, ver Chords.h)
void synthNoteOff();                  // inicia o release e desliga

void synthSetChordType(ChordType type); // pode ser chamado com uma nota ja tocando
void synthSetPlayMode(PlayMode mode);   // idem - troca no meio do som, sem cortar

// pos = posicao continua entre timbres, 0..(Waveform::COUNT - 1). Valores
// fracionarios misturam os dois timbres vizinhos (ex: 1.5 = metade
// Triangular, metade Dente-de-serra).
void synthSetTimbrePosition(float pos);

void synthSetTempoBPM(int bpm);
void synthSetGlideMs(float ms);   // 0 = sem glide (troca instantanea), maior = mais "escorregado"
void synthSetArpGate(float gate); // 0..1, fracao do passo em que a nota soa (staccato <-> legato)

bool synthIsPlaying();
uint8_t synthCurrentRoot();
ChordType synthCurrentChordType();
