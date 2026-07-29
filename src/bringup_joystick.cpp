// TESTE 4: Joystick (VRx/VRy/SW) + potenciometros (tempo/timbre)
// Imprime os valores brutos de ADC (0-4095) a cada 200ms.
// Grave com: pio run -e test_joystick -t upload
//
// O que conferir:
//   - Joystick parado no centro: VRx e VRy devem ficar por volta de ~2048
//     (nao precisa ser exato, mas deve ficar longe de 0 ou 4095)
//   - Mexendo o joystick pra cada lado: o numero correspondente deve variar
//     suavemente de ~0 a ~4095
//   - Apertando o botao do joystick: SW deve virar "PRESSIONADO"
//   - Girando cada potenciometro: o numero dele deve variar de ~0 a ~4095

#include <Arduino.h>
#include "Config.h"

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("Teste joystick + potenciometros");
    analogReadResolution(12);
    pinMode(PIN_JOY_SW, INPUT_PULLUP);
}

void loop() {
    int joyX = analogRead(PIN_JOY_X);
    int joyY = analogRead(PIN_JOY_Y);
    bool sw = (digitalRead(PIN_JOY_SW) == LOW);
    int potTempo = analogRead(PIN_POT_TEMPO);
    int potTimbre = analogRead(PIN_POT_TIMBRE);

    Serial.print("VRx=");
    Serial.print(joyX);
    Serial.print("  VRy=");
    Serial.print(joyY);
    Serial.print("  SW=");
    Serial.print(sw ? "PRESSIONADO" : "solto");
    Serial.print("  potTempo=");
    Serial.print(potTempo);
    Serial.print("  potTimbre=");
    Serial.println(potTimbre);

    delay(200);
}
