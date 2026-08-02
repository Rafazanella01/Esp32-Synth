#include "Keypad.h"
#include "Config.h"

// Estado de debounce por tecla (16 teclas)
static bool stableState[16];      // true = pressionada (debounced)
static bool lastRawState[16];     // ultima leitura crua
static uint32_t lastChangeMs[16]; // timestamp da ultima mudanca crua

static int8_t pendingPressed = -1;
static int8_t pendingReleased = -1;

void keypadInit() {
    for (int r = 0; r < 4; r++) {
        pinMode(ROW_PINS[r], OUTPUT);
        digitalWrite(ROW_PINS[r], HIGH); // inativo = HIGH
    }
    for (int c = 0; c < 4; c++) {
        pinMode(COL_PINS[c], INPUT_PULLUP);
    }
    for (int i = 0; i < 16; i++) {
        stableState[i] = false;
        lastRawState[i] = false;
        lastChangeMs[i] = 0;
    }
    pendingPressed = -1;
    pendingReleased = -1;
}

void keypadUpdate() {
    uint32_t now = millis();

    for (int r = 0; r < 4; r++) {
        digitalWrite(ROW_PINS[r], LOW);
        delayMicroseconds(10); // deixa a linha assentar antes de ler

        for (int c = 0; c < 4; c++) {
            int idx = r * 4 + c;
            bool raw = (digitalRead(COL_PINS[c]) == LOW); // LOW = pressionada

            if (raw != lastRawState[idx]) {
                lastRawState[idx] = raw;
                lastChangeMs[idx] = now;
            } else if ((now - lastChangeMs[idx]) > DEBOUNCE_MS) {
                if (raw != stableState[idx]) {
                    stableState[idx] = raw;
                    if (raw) {
                        pendingPressed = idx;
                    } else {
                        pendingReleased = idx;
                    }
                }
            }
        }

        digitalWrite(ROW_PINS[r], HIGH);
    }
}

int8_t keypadJustPressed() {
    int8_t k = pendingPressed;
    pendingPressed = -1;
    return k;
}

int8_t keypadJustReleased() {
    int8_t k = pendingReleased;
    pendingReleased = -1;
    return k;
}
