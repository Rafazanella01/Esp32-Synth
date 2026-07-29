#include "Display.h"
#include "Config.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
static uint8_t currentPage = 0;
static bool firstDraw = true;
static SynthStatus lastDrawn = {false, 255, ChordType::MAJOR, Waveform::SINE, -1};

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

static void drawStatusPage(const SynthStatus &s) {
    lcd.clear();
    lcd.setCursor(0, 0);
    if (s.playing) {
        lcd.print(NOTE_NAMES[s.rootNote]);
        lcd.print(" ");
        lcd.print(chordDef(s.chordType).name);
    } else {
        lcd.print("-- silencio --");
    }
    lcd.setCursor(0, 1);
    lcd.print(waveformName(s.timbre));
    lcd.print(" ");
    lcd.print(s.bpm);
    lcd.print("bpm");
}

static void drawHelpPage() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("L4:Mai Men m7 Aum");
    lcd.setCursor(0, 1);
    lcd.print("Joy SW=stop Y=pag");
}

void displaySetPage(int8_t delta) {
    if (delta == 0) return;
    int8_t p = (int8_t)currentPage + delta;
    if (p < 0) p = 1;
    if (p > 1) p = 0;
    currentPage = (uint8_t)p;
    firstDraw = true; // forca redesenho na proxima displayUpdate()
}

void displayUpdate(const SynthStatus &status) {
    bool changed = firstDraw ||
                   status.playing != lastDrawn.playing ||
                   status.rootNote != lastDrawn.rootNote ||
                   status.chordType != lastDrawn.chordType ||
                   status.timbre != lastDrawn.timbre ||
                   status.bpm != lastDrawn.bpm;

    if (!changed) return;

    if (currentPage == 0) {
        drawStatusPage(status);
    } else {
        drawHelpPage();
    }

    lastDrawn = status;
    firstDraw = false;
}
