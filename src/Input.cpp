#include "Input.h"
#include "Config.h"

static int tempoBPM = ARP_BPM_DEFAULT;
static Waveform selectedTimbre = Waveform::SINE;
static float pitchBendSemitones = 0.0f;

static uint32_t lastPollMs = 0;

// joystick Y: maquina de estado p/ so disparar 1 gesto por vez que sai do centro
static bool yGestureArmed = true;
static int8_t pendingPageGesture = 0;

// botao do joystick
static bool lastButtonRaw = false;
static bool buttonStable = false;
static uint32_t buttonLastChangeMs = 0;
static bool pendingPanic = false;

static const int ADC_MAX = 4095;
static const int ADC_CENTER = ADC_MAX / 2;
static const int JOY_DEADZONE = 400; // em contagens de ADC, em torno do centro

void inputInit() {
    analogReadResolution(12); // 0..4095
    pinMode(PIN_JOY_SW, INPUT_PULLUP);
}

static void pollPots() {
    int rawTempo = analogRead(PIN_POT_TEMPO);
    tempoBPM = map(rawTempo, 0, ADC_MAX, ARP_BPM_MIN, ARP_BPM_MAX);

    int rawTimbre = analogRead(PIN_POT_TIMBRE);
    int zone = map(rawTimbre, 0, ADC_MAX, 0, (int)Waveform::COUNT - 1);
    zone = constrain(zone, 0, (int)Waveform::COUNT - 1);
    selectedTimbre = (Waveform)zone;
}

static void pollJoystick() {
    int rawX = analogRead(PIN_JOY_X);
    int dx = rawX - ADC_CENTER;
    if (abs(dx) < JOY_DEADZONE) {
        pitchBendSemitones = 0.0f;
    } else {
        int span = ADC_CENTER - JOY_DEADZONE;
        float norm = (float)(abs(dx) - JOY_DEADZONE) / (float)span; // 0..1
        norm = constrain(norm, 0.0f, 1.0f);
        pitchBendSemitones = (dx > 0 ? norm : -norm); // -1..+1 semitom
    }

    int rawY = analogRead(PIN_JOY_Y);
    int dy = rawY - ADC_CENTER;
    if (abs(dy) < JOY_DEADZONE) {
        yGestureArmed = true; // voltou pro centro, libera novo gesto
    } else if (yGestureArmed) {
        pendingPageGesture = (dy > 0) ? 1 : -1;
        yGestureArmed = false;
    }

    bool raw = (digitalRead(PIN_JOY_SW) == LOW); // ativo em LOW
    uint32_t now = millis();
    if (raw != lastButtonRaw) {
        lastButtonRaw = raw;
        buttonLastChangeMs = now;
    } else if ((now - buttonLastChangeMs) > DEBOUNCE_MS) {
        if (raw != buttonStable) {
            buttonStable = raw;
            if (raw) {
                pendingPanic = true;
            }
        }
    }
}

void inputUpdate() {
    uint32_t now = millis();
    if (now - lastPollMs < ADC_POLL_MS) return;
    lastPollMs = now;

    pollPots();
    pollJoystick();
}

int getTempoBPM() { return tempoBPM; }
Waveform getSelectedTimbre() { return selectedTimbre; }
float getPitchBendSemitones() { return pitchBendSemitones; }

int8_t consumeDisplayPageGesture() {
    int8_t g = pendingPageGesture;
    pendingPageGesture = 0;
    return g;
}

bool consumePanicPress() {
    if (pendingPanic) {
        pendingPanic = false;
        return true;
    }
    return false;
}
