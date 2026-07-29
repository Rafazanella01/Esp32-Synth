#pragma once
#include <Arduino.h>

// Scanner simples da matriz 4x4 (sem lib externa).
// Indice da tecla = row * 4 + col, 0..15 (ver layout comentado em Config.h).

void keypadInit();

// Chamar a cada iteracao do loop principal. Retorna o indice (0..15) da tecla
// que acabou de ser PRESSIONADA neste ciclo (edge-triggered, com debounce), ou
// -1 se nenhuma tecla nova foi pressionada.
int8_t keypadPoll();
