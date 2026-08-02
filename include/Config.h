#pragma once
#include <Arduino.h>

// =============================================================================
// MAPA DE PINOS - ESP32 DevKit generico (30/38 pinos)
// Se sua placa for S3/C3/outra, os numeros de GPIO abaixo continuam validos
// como referencia, mas confira o pinout serigrafado da sua placa especifica.
// =============================================================================

// -----------------------------------------------------------------------------
// I2S -> modulo GY-PCM5102 (PCM5102A) - saida de audio digital
// -----------------------------------------------------------------------------
// SCK  do modulo -> GND   (deixa o PCM5102A gerar o clock interno a partir do BCK)
// FMT  do modulo -> GND   (formato I2S padrao, nao PCM)
// XSMT do modulo -> 3.3V  (sem isso o modulo fica em soft-mute permanente!)
// VIN -> 3.3V ; GND -> GND
static const int PIN_I2S_BCK  = 27; // bit clock
static const int PIN_I2S_WS   = 25; // word select / LRCK
static const int PIN_I2S_DOUT = 26; // data (-> pino DIN do PCM5102A)

// -----------------------------------------------------------------------------
// I2C -> display LCD 16x2 com backpack (4 pinos: GND, VCC, SDA, SCL)
// -----------------------------------------------------------------------------
// Pinos default de hardware I2C do ESP32 DevKit - use estes sempre que possivel.
static const int PIN_I2C_SDA = 21;
static const int PIN_I2C_SCL = 22;
static const uint8_t LCD_I2C_ADDR = 0x27; // troque p/ 0x3F se o seu backpack usar esse endereco
static const uint8_t LCD_COLS = 16;
static const uint8_t LCD_ROWS = 2;

// -----------------------------------------------------------------------------
// Matriz de botoes 4x4 (modulo com 8 saidas: R1-R4, C1-C4)
// -----------------------------------------------------------------------------
// Linhas = saidas do ESP32 (uma por vez em LOW) | Colunas = entradas c/ pull-up interno
// Pinos conferidos contra o pinout real da placa (DevKit 30 pinos): D16/D17
// nessa placa aparecem serigrafados como RX2/TX2, mas sao os mesmos GPIOs
// (16/17) e estao livres, ja que UART2 nao e usado neste projeto.
static const int ROW_PINS[4] = {13, 14, 18, 19};   // -> R1 R2 R3 R4 do modulo
static const int COL_PINS[4] = {23, 16, 17, 2};    // -> C1 C2 C3 C4 do modulo (16=RX2, 17=TX2)

// Layout fisico sugerido (serigrafar/anotar nas teclas):
//   Linha 1:  C    C#   D    D#
//   Linha 2:  E    F    F#   G
//   Linha 3:  G#   A    A#   B
//   Linha 4:  MAJ  MIN  MIN7 AUG
//
// As 12 primeiras teclas (linhas 1-3) tocam/alternam o acorde com aquela nota
// como fundamental. As 4 teclas da linha 4 trocam o tipo de acorde (fica
// "travado" ate voce escolher outro tipo).

// -----------------------------------------------------------------------------
// Joystick analogico (VRx, VRy, SW) -> navegacao de menu / controle expressivo
// -----------------------------------------------------------------------------
static const int PIN_JOY_X  = 34; // ADC1, input-only - ok, nao precisa de pull-up
static const int PIN_JOY_Y  = 35; // ADC1, input-only
static const int PIN_JOY_SW = 4;  // digital, INPUT_PULLUP (botao ativo em LOW)

// -----------------------------------------------------------------------------
// Potenciometros 10K (ADC1 - funcionam mesmo com o "WiFi" desligado, sem ruido de ADC2)
// -----------------------------------------------------------------------------
static const int PIN_POT_TEMPO  = 32; // gira o andamento (BPM) do arpejador
static const int PIN_POT_TIMBRE = 33; // varre entre os timbres (morph/seleciona forma de onda)
// GPIO36 (VP) e GPIO39 (VN) ficam livres para um 3o/4o pote (ex: decay do envelope)
static const int PIN_POT_SPARE1 = 36;
static const int PIN_POT_SPARE2 = 39;

// -----------------------------------------------------------------------------
// Audio
// -----------------------------------------------------------------------------
static const int SAMPLE_RATE = 44100;
static const int I2S_DMA_BUF_COUNT = 6;
static const int I2S_DMA_BUF_LEN = 256;

static const float BASE_FREQ_C4 = 261.6256f; // C4 (Do central) em Hz

// -----------------------------------------------------------------------------
// Arpejador
// -----------------------------------------------------------------------------
static const int ARP_BPM_MIN = 60;
static const int ARP_BPM_MAX = 240;
static const int ARP_BPM_DEFAULT = 120;
static const int ARP_NOTE_DIV = 2;     // 1 = seminima, 2 = colcheia, 4 = semicolcheia
static const float ARP_GATE = 0.75f;   // fracao do passo em que a nota soa (resto = silencio)

static const uint32_t DEBOUNCE_MS = 25;
static const uint32_t ADC_POLL_MS = 30; // intervalo de leitura de pots/joystick
