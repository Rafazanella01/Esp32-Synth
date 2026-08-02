// TESTE 4: Joystick (VRx/VRy/SW) + potenciometros
// Mostra a leitura crua, a deflexao normalizada (-1..+1 por eixo, ja
// relativa ao centro calibrado no boot) e qual das 8 direcoes o firmware
// esta reconhecendo. Grave com: pio run -e test_joystick -t upload
//
// IMPORTANTE: deixe o joystick EM REPOUSO enquanto a placa liga - o centro
// e medido nesse momento.
//
// O que conferir:
//   - Joystick parado: nx e ny perto de 0.00, direcao = "centro"
//   - Empurrando pra cada lado (inclusive diagonais): nx/ny devem chegar
//     perto de +-1.00 de forma parecida em TODAS as direcoes, e o nome da
//     direcao deve bater com o lado pra onde voce empurrou
//   - Apertando o botao: SW = PRESSIONADO
//   - Girando cada potenciometro: o numero dele varia de ~0 a ~4095

#include <Arduino.h>
#include "Config.h"

static const int ADC_MAX = 4095;

static float joyCenterX = (float)(ADC_MAX / 2);
static float joyCenterY = (float)(ADC_MAX / 2);

static float normalizeAxis(float raw, float center) {
    if (raw >= center) {
        float span = (float)ADC_MAX - center;
        if (span < 1.0f) return 0.0f;
        return constrain((raw - center) / span, 0.0f, 1.0f);
    }
    float span = center;
    if (span < 1.0f) return 0.0f;
    return constrain((raw - center) / span, -1.0f, 0.0f);
}

static const char *directionName(float nx, float ny) {
    float mag2 = nx * nx + ny * ny;
    if (mag2 <= (JOY_DEADZONE_NORM * JOY_DEADZONE_NORM)) return "centro";

    float angleDeg = atan2f(ny, nx) * 180.0f / PI;
    if (angleDeg < 0) angleDeg += 360.0f;
    int sector = ((int)((angleDeg + 22.5f) / 45.0f)) % 8;

    static const char *NAMES[8] = {
        "direita (E)", "cima-direita (NE)", "cima (N)", "cima-esquerda (NW)",
        "esquerda (W)", "baixo-esquerda (SW)", "baixo (S)", "baixo-direita (SE)"
    };
    return NAMES[sector];
}

void setup() {
    Serial.begin(115200);
    delay(300);
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_JOY_X, ADC_11db);
    analogSetPinAttenuation(PIN_JOY_Y, ADC_11db);
    pinMode(PIN_JOY_SW, INPUT_PULLUP);

    Serial.println("Teste joystick + potenciometros");
    Serial.println("Calibrando centro (deixe o joystick parado)...");

    long sumX = 0;
    long sumY = 0;
    for (int i = 0; i < JOY_CALIB_SAMPLES; i++) {
        sumX += analogRead(PIN_JOY_X);
        sumY += analogRead(PIN_JOY_Y);
        delay(2);
    }
    joyCenterX = (float)sumX / (float)JOY_CALIB_SAMPLES;
    joyCenterY = (float)sumY / (float)JOY_CALIB_SAMPLES;

    Serial.print("Centro medido: X=");
    Serial.print(joyCenterX, 1);
    Serial.print("  Y=");
    Serial.println(joyCenterY, 1);
}

void loop() {
    float joyX = analogRead(PIN_JOY_X);
    float joyY = analogRead(PIN_JOY_Y);
    float nx = normalizeAxis(joyX, joyCenterX);
    float ny = normalizeAxis(joyY, joyCenterY);
    bool sw = (digitalRead(PIN_JOY_SW) == LOW);

    Serial.print("X=");
    Serial.print((int)joyX);
    Serial.print(" Y=");
    Serial.print((int)joyY);
    Serial.print("  nx=");
    Serial.print(nx, 2);
    Serial.print(" ny=");
    Serial.print(ny, 2);
    Serial.print("  -> ");
    Serial.print(directionName(nx, ny));
    Serial.print(sw ? "  [CLIQUE]" : "         ");

    Serial.print("  tempo=");
    Serial.print(analogRead(PIN_POT_TEMPO));
    Serial.print(" timbre=");
    Serial.print(analogRead(PIN_POT_TIMBRE));
    Serial.print(" glide=");
    Serial.print(analogRead(PIN_POT_GLIDE));
    Serial.print(" gate=");
    Serial.println(analogRead(PIN_POT_ARP_GATE));

    delay(200);
}
