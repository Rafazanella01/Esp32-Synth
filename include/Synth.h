#pragma once
#include <Arduino.h>
#include "Chords.h"
#include "Waveforms.h"

// Motor de audio: I2S + oscilador por wavetable + arpejador.
// synthProcess() deve ser chamado o mais frequente possivel no loop() -
// e ele quem alimenta o DMA do I2S (a chamada bloqueia levemente, o que
// acaba servindo de "metronomo" natural pro loop principal).
//
// Modelo de toque: aperta a tecla e ja toca/troca na hora (nao precisa
// segurar); apertar a mesma nota que ja esta tocando desliga. Enquanto uma
// nota esta ligada, a qualidade do acorde pode mudar livremente
// (synthSetChordType) sem cortar/reiniciar o som.
//
// Se o ChordType atual tiver so 1 nota (ChordType::NATURAL), o som e
// sustentado continuo (sem arpejo). Com mais de 1 nota, o motor arpeja
// ciclando entre os tons do acorde.
//
// Transicoes suaves: trocar de nota desliza a afinacao (glide/portamento)
// em vez de saltar seco, e o timbre e uma mistura continua entre as
// wavetables adjacentes (nao troca abrupta entre formas de onda).

void synthInit();
void synthProcess();

void synthNoteOn(uint8_t rootNote);   // liga/troca a nota (0..15, ver Chords.h)
void synthNoteOff();                  // inicia o release e desliga

void synthSetChordType(ChordType type); // pode ser chamado com uma nota ja tocando

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
