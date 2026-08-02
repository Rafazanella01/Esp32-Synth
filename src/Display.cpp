#include "Display.h"
#include "Config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
static bool firstDraw = true;
static SynthStatus lastDrawn = {false, 255, ChordType::NATURAL, Waveform::SINE, -1};

void displayInit() {
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ESP32 Synth");
    lcd.setCursor(0, 1);
    lcd.print("iniciando...");
}

void displayUpdate(const SynthStatus &status) {
    bool changed = firstDraw ||
                   status.playing != lastDrawn.playing ||
                   status.rootNote != lastDrawn.rootNote ||
                   status.chordType != lastDrawn.chordType ||
                   status.timbre != lastDrawn.timbre ||
                   status.bpm != lastDrawn.bpm;

    if (!changed) return;

    lcd.clear();
    lcd.setCursor(0, 0);
    if (status.playing) {
        lcd.print(NOTE_NAMES[status.rootNote]);
        lcd.print(" ");
        lcd.print(chordDef(status.chordType).name);
    } else {
        lcd.print("ESP32 Synth");
    }
    lcd.setCursor(0, 1);
    lcd.print(waveformName(status.timbre));
    lcd.print(" ");
    lcd.print(status.bpm);
    lcd.print("bpm");

    lastDrawn = status;
    firstDraw = false;
}
