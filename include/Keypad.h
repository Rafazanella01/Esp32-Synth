#pragma once
#include <Arduino.h>

// Scanner simples da matriz 4x4 (sem lib externa).
// Indice da tecla = row * 4 + col, 0..15 (ver layout comentado em Config.h).
// Todas as 16 teclas sao notas (ver Chords.h / NOTE_NAMES).
//
// Reporta tanto o instante em que uma tecla e pressionada quanto o instante
// em que e solta, pra quem estiver usando (ver main.cpp) poder distinguir
// um toque rapido (dispara o arranjo/arpejo e continua sozinho) de segurar
// a tecla (toca so enquanto estiver segurando).

void keypadInit();

// Chamar a cada iteracao do loop principal - varre a matriz e atualiza o
// estado interno (com debounce).
void keypadUpdate();

// Indice (0..15) da tecla que acabou de ser PRESSIONADA neste ciclo
// (edge-triggered), ou -1 se nenhuma. So reporta uma vez por toque.
int8_t keypadJustPressed();

// Indice (0..15) da tecla que acabou de ser SOLTA neste ciclo
// (edge-triggered), ou -1 se nenhuma. So reporta uma vez por soltada.
int8_t keypadJustReleased();
