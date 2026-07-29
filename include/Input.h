#pragma once
#include <Arduino.h>
#include "Waveforms.h"

// Le potenciometros (tempo/timbre) e joystick (pitch-bend, navegacao, panico).
// Chamar inputInit() no setup() e inputUpdate() a cada loop().

void inputInit();
void inputUpdate();

int getTempoBPM();                 // andamento do arpejador, ja mapeado p/ ARP_BPM_MIN..MAX
Waveform getSelectedTimbre();      // timbre escolhido pelo pote de timbre
float getPitchBendSemitones();     // -1..+1 semitom, controlado pelo eixo X do joystick

// Eixo Y do joystick: retorna -1 (p/ cima), +1 (p/ baixo) ou 0 quando nao ha
// gesto novo. Um gesto so e reportado uma vez ate o stick voltar ao centro
// (evita ficar avancando pagina em rajada). Usado p/ navegar as paginas do LCD.
int8_t consumeDisplayPageGesture();

// Botao do joystick: true uma unica vez no instante em que foi pressionado
// (edge-triggered, com debounce). Usado como "panico" (silenciar tudo).
bool consumePanicPress();
