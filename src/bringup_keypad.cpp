// TESTE 3: Matriz de botoes 4x4
// Imprime no Serial qual tecla foi pressionada e qual foi solta (todas as
// 16 sao notas). Grave com: pio run -e test_keypad -t upload
// Abra o Serial Monitor (115200) e aperte/solte as 16 teclas, uma por vez,
// conferindo se o nome impresso bate com a tecla fisica que voce apertou.
//
// Se uma tecla nao responder ou responder errado, confira a fiacao de R/C
// daquela linha/coluna especifica (é mais facil de achar assim, uma por vez,
// do que só olhando o firmware completo rodando).

#include <Arduino.h>
#include "Config.h"
#include "Chords.h"
#include "Keypad.h"

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("Teste da matriz 4x4 - aperte e solte as teclas uma de cada vez");
    keypadInit();
}

void loop() {
    keypadUpdate();

    int8_t pressed = keypadJustPressed();
    if (pressed >= 0) {
        Serial.print("Pressionada ");
        Serial.print(pressed);
        Serial.print(" -> ");
        Serial.println(NOTE_NAMES[pressed]);
    }

    int8_t released = keypadJustReleased();
    if (released >= 0) {
        Serial.print("Solta      ");
        Serial.print(released);
        Serial.print(" -> ");
        Serial.println(NOTE_NAMES[released]);
    }
}
