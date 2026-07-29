#pragma once
#include <Arduino.h>
#include "Chords.h"
#include "Waveforms.h"

// Motor de audio: I2S + oscilador por wavetable + arpejador.
// synthProcess() deve ser chamado o mais frequente possivel no loop() -
// e ele quem alimenta o DMA do I2S (a chamada bloqueia levemente, o que
// acaba servindo de "metronomo" natural pro loop principal).

void synthInit();
void synthProcess();

// Inicia (ou troca) o acorde arpejado a partir da nota "rootNote" (0..11).
// Se rootNote for igual ao que ja esta tocando, para o som (toggle).
void synthTriggerRoot(uint8_t rootNote);

void synthSetChordType(ChordType type); // troca o tipo com o acorde ja tocando
void synthSetTimbre(Waveform w);
void synthSetTempoBPM(int bpm);
void synthSetPitchBendSemitones(float semis);
void synthStop();

bool synthIsPlaying();
uint8_t synthCurrentRoot();
ChordType synthCurrentChordType();
