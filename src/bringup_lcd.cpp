// TESTE 2: Display LCD 16x2 I2C
// Primeiro escaneia o barramento I2C e imprime os enderecos encontrados
// (util pra confirmar se e 0x27 ou 0x3F), depois escreve no LCD.
// Grave com: pio run -e test_lcd -t upload
// Abra o Serial Monitor (115200) pra ver o resultado do scan.

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Config.h"

static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

static void i2cScan() {
    Serial.println("Escaneando barramento I2C...");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("  Dispositivo encontrado em 0x");
            Serial.println(addr, HEX);
            found++;
        }
    }
    if (found == 0) {
        Serial.println("  Nenhum dispositivo encontrado! Confira SDA/SCL/VCC/GND do LCD.");
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("Teste LCD I2C");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    i2cScan();

    Serial.print("Usando endereco configurado em Config.h: 0x");
    Serial.println(LCD_I2C_ADDR, HEX);
    Serial.println("Se esse endereco nao apareceu no scan acima, troque LCD_I2C_ADDR e regrave.");

    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("LCD OK!");
    lcd.setCursor(0, 1);
    lcd.print("ESP32-Synth");
}

void loop() {
    static uint32_t lastMs = 0;
    static uint32_t counter = 0;

    if (millis() - lastMs > 1000) {
        lastMs = millis();
        counter++;
        lcd.setCursor(0, 1);
        lcd.print("contador: ");
        lcd.print(counter);
        lcd.print("   "); // limpa sobra se o numero encolher
    }
}
