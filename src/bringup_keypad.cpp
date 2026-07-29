// TESTE 3: Matriz de botoes 4x4
// Imprime no Serial qual tecla foi pressionada (nome da nota ou do acorde).
// Grave com: pio run -e test_keypad -t upload
// Abra o Serial Monitor (115200) e va apertando as 16 teclas, uma por vez,
// conferindo se o nome impresso bate com a tecla fisica que voce apertou.
//
// Se uma tecla nao responder ou responder errado, confira a fiacao de R/C
// daquela linha/coluna especifica (é mais facil de achar assim, uma por vez,
// do que só olhando o firmware completo rodando).

#include <Arduino.h>
#include "Config.h"
#include "Keypad.h"

static const char *KEY_LABELS[16] = {
    "C", "C#", "D", "D#",
    "E", "F", "F#", "G",
    "G#", "A", "A#", "B",
    "MAIOR", "MENOR", "MENOR7", "AUMENTADO"
};

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("Teste da matriz 4x4 - aperte as teclas uma de cada vez");
    keypadInit();
}

void loop() {
    int8_t key = keypadPoll();
    if (key >= 0) {
        Serial.print("Tecla ");
        Serial.print(key);
        Serial.print(" -> ");
        Serial.println(KEY_LABELS[key]);
    }
}
