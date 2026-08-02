#include <Arduino.h>
#include "Config.h"
#include "Chords.h"
#include "Waveforms.h"
#include "Keypad.h"
#include "Input.h"
#include "Display.h"
#include "Synth.h"

// Como o toque funciona:
//
//   SEGURAR uma tecla  -> toca SO aquela nota, pura e sustentada, enquanto
//                         estiver segurando. Solta, para.
//   TOCAR rapido (tap) -> dispara o ARRANJO (arpejo) daquela nota, que
//                         continua tocando sozinho depois de soltar.
//                         A qualidade do acorde vem do joystick, ao vivo.
//   Tocar de novo a mesma nota que esta arpejando -> para.
//
// Nao da pra saber se e tap ou hold no instante em que aperta, entao o
// firmware sempre comeca tocando a nota pura; se soltar antes de
// TAP_THRESHOLD_MS, ai sim vira arranjo e segue tocando.
static const uint32_t TAP_THRESHOLD_MS = 250;

enum class TouchMode : uint8_t {
    NONE,     // nada tocando
    HOLDING,  // tecla segurada: nota pura, sustentada
    ARPEGGIO  // tap disparou o arranjo: toca sozinho ate mandar parar
};

static TouchMode touchMode = TouchMode::NONE;
static int8_t trackingKey = -1;
static uint32_t trackingStartMs = 0;

void setup() {
    generateWaveTables();

    keypadInit();
    inputInit();
    displayInit();
    synthInit();
}

void loop() {
    // 1) audio - chamar sempre, e o que mantem o som sem estalos/cortes
    synthProcess();

    // 2) entradas - matriz
    keypadUpdate();

    int8_t pressed = keypadJustPressed();
    if (pressed >= 0) {
        bool stoppingCurrentArpeggio = (touchMode == TouchMode::ARPEGGIO) &&
                                       synthIsPlaying() &&
                                       synthCurrentRoot() == (uint8_t)pressed;
        if (stoppingCurrentArpeggio) {
            synthNoteOff();
            touchMode = TouchMode::NONE;
            trackingKey = -1;
        } else {
            // toda tecla comeca como nota pura sustentada; vira arranjo
            // depois, se o toque for curto (ver bloco de "solta" abaixo)
            synthNoteOn((uint8_t)pressed);
            touchMode = TouchMode::HOLDING;
            trackingKey = pressed;
            trackingStartMs = millis();
        }
    }

    int8_t released = keypadJustReleased();
    if (released >= 0 && released == trackingKey) {
        uint32_t heldMs = millis() - trackingStartMs;
        if (heldMs < TAP_THRESHOLD_MS) {
            touchMode = TouchMode::ARPEGGIO; // tap: o arranjo assume e continua sozinho
        } else {
            synthNoteOff();                  // hold: soltou, para
            touchMode = TouchMode::NONE;
        }
        trackingKey = -1;
    }

    // 2b) entradas - joystick e potenciometros
    inputUpdate();
    synthSetTempoBPM(getTempoBPM());
    synthSetTimbrePosition(getTimbrePosition());
    synthSetGlideMs(getGlideMs());
    synthSetArpGate(getArpGate());

    // Segurar vs tocar rapido NAO muda o acorde - muda o jeito de tocar:
    // segurando o acorde fica em bloco (sustentado), no tap ele arpeja.
    // O joystick manda na qualidade do acorde ao vivo nos dois modos; a
    // unica diferenca e o centro, que ao segurar vira a nota pura.
    if (touchMode == TouchMode::HOLDING) {
        synthSetPlayMode(PlayMode::SUSTAINED);
        synthSetChordType(isJoystickCentered() ? ChordType::NATURAL : getJoystickChordType());
    } else {
        synthSetPlayMode(PlayMode::ARPEGGIO);
        synthSetChordType(getJoystickChordType());
    }

    // 3) display (mostra o timbre mais proximo da posicao continua atual)
    SynthStatus status;
    status.playing = synthIsPlaying();
    status.rootNote = synthCurrentRoot();
    status.chordType = synthCurrentChordType();
    status.timbre = (Waveform)constrain((int)roundf(getTimbrePosition()), 0, (int)Waveform::COUNT - 1);
    status.bpm = getTempoBPM();
    displayUpdate(status);
}
