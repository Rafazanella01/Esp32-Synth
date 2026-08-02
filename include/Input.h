#pragma once
#include <Arduino.h>
#include "Waveforms.h"
#include "Chords.h"

// Le potenciometros (tempo/timbre) e a posicao do joystick, que controla ao
// vivo a qualidade do acorde aplicada sobre a nota tocada.
// Chamar inputInit() no setup() e inputUpdate() a cada loop().

void inputInit();
void inputUpdate();

int getTempoBPM(); // andamento do arpejador, ja mapeado p/ ARP_BPM_MIN..MAX

// Posicao continua entre timbres, 0..(Waveform::COUNT - 1). Passar direto
// pra synthSetTimbrePosition() - a mistura entre formas de onda vizinhas
// acontece no motor de audio, entao girar o pote soa como um morph suave.
float getTimbrePosition();

// Quantidade de glide/portamento, em ms - passar direto pra synthSetGlideMs().
float getGlideMs();

// Gate do arpejo (staccato <-> legato), 0..1 - passar direto pra synthSetArpGate().
float getArpGate();

// Qualidade do acorde de acordo com a posicao atual do joystick - usa as 8
// direcoes (incluindo diagonais) mais o centro e o clique, 10 zonas ao
// todo. Ver o mapeamento completo em Chords.h (enum ChordType). E lida ao
// vivo - pode mudar a qualquer momento, inclusive com uma nota ja soando.
// No centro retorna ChordType::MAJOR (o acorde padrao).
ChordType getJoystickChordType();

// true quando o joystick esta parado no centro E sem clique. Serve pra quem
// chama decidir o que "centro" significa em cada contexto: segurando a
// tecla, centro = nota pura; no arpejo, centro = acorde maior.
bool isJoystickCentered();
