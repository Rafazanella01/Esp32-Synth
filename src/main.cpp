#include <Arduino.h>
#include "Config.h"
#include "Chords.h"
#include "Waveforms.h"
#include "Keypad.h"
#include "Input.h"
#include "Display.h"
#include "Synth.h"

// Teclas 0..11  -> nota fundamental (linhas 1-3 da matriz)
// Teclas 12..15 -> tipo de acorde (linha 4: Maior, Menor, Menor7, Aumentado)
static ChordType chordTypeForKey(int8_t key) {
    return (ChordType)(key - 12);
}

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

    // 2) entradas
    int8_t key = keypadPoll();
    if (key >= 0) {
        if (key < 12) {
            synthTriggerRoot((uint8_t)key);
        } else {
            synthSetChordType(chordTypeForKey(key));
        }
    }

    inputUpdate();
    synthSetTempoBPM(getTempoBPM());
    synthSetTimbre(getSelectedTimbre());
    synthSetPitchBendSemitones(getPitchBendSemitones());

    if (consumePanicPress()) {
        synthStop();
    }

    displaySetPage(consumeDisplayPageGesture());

    // 3) display
    SynthStatus status;
    status.playing = synthIsPlaying();
    status.rootNote = synthCurrentRoot();
    status.chordType = synthCurrentChordType();
    status.timbre = getSelectedTimbre();
    status.bpm = getTempoBPM();
    displayUpdate(status);
}
