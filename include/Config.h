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

// Layout fisico sugerido (serigrafar/anotar nas teclas) - todas as 16 sao
// notas (hold-to-sustain: toca enquanto segura, para ao soltar):
//   Linha 1:  C4   C#4  D4   D#4
//   Linha 2:  E4   F4   F#4  G4
//   Linha 3:  G#4  A4   A#4  B4
//   Linha 4:  C5   C#5  D5   D#5
//
// A qualidade do acorde (natural/maior/menor/maior7/menor7/aumentado) e
// escolhida ao vivo pelo joystick, nao pela matriz - ver Input.h.

// -----------------------------------------------------------------------------
// Joystick analogico (VRx, VRy, SW) -> escolhe a qualidade do acorde ao vivo
// -----------------------------------------------------------------------------
static const int PIN_JOY_X  = 34; // ADC1, input-only - ok, nao precisa de pull-up
static const int PIN_JOY_Y  = 35; // ADC1, input-only
static const int PIN_JOY_SW = 4;  // digital, INPUT_PULLUP (botao ativo em LOW)

// Zona morta do joystick, ja em unidades NORMALIZADAS (0..1 de deflexao),
// nao em contagens de ADC. Isso importa: o ADC do ESP32 comprime a leitura
// perto de 3.3V, entao o curso "pra cima" costuma ter bem menos contagens
// que o curso "pra baixo". Normalizando cada lado pelo seu proprio curso
// (ver Input.cpp), as 8 direcoes ficam com sensibilidade parecida.
static const float JOY_DEADZONE_NORM = 0.35f;

// Quantas amostras usar pra descobrir o centro real do joystick no boot.
// O joystick precisa estar EM REPOUSO quando a placa liga.
static const int JOY_CALIB_SAMPLES = 32;

// -----------------------------------------------------------------------------
// Potenciometros 10K (ADC1 - funcionam mesmo com o "WiFi" desligado, sem ruido de ADC2)
// -----------------------------------------------------------------------------
static const int PIN_POT_TEMPO    = 32; // gira o andamento (BPM) do arpejador
static const int PIN_POT_TIMBRE   = 33; // varre entre os timbres (morph continuo entre formas de onda)
static const int PIN_POT_GLIDE    = 36; // quanto a nota "desliza" (portamento) entre uma troca e outra
static const int PIN_POT_ARP_GATE = 39; // staccato (picotado) <-> legato (quase continuo) no arpejo

// Pino de ADC sem nada ligado NAO le "zero" - ele flutua, oscilando sozinho.
// Como o gate do arpejo e o glide saem desses dois pinos, um pino solto faz
// o som picotar/sumir sem motivo aparente. Enquanto esses potenciometros
// nao estiverem fisicamente ligados, deixe as flags abaixo em false: o
// firmware ignora a leitura e usa o valor padrao fixo.
// Ligou o potenciometro? Vire a flag correspondente pra true e regrave.
static const bool POT_GLIDE_CONNECTED = false;
static const bool POT_ARP_GATE_CONNECTED = false;

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

// Gate do arpejo (fracao do passo em que a nota soa) - controlado ao vivo
// pelo PIN_POT_ARP_GATE, entre staccato (curto) e legato (quase continuo).
static const float ARP_GATE_MIN = 0.15f;
static const float ARP_GATE_MAX = 0.95f;
static const float ARP_GATE_DEFAULT = 0.75f;

// Glide (portamento) entre notas - controlado ao vivo pelo PIN_POT_GLIDE,
// de instantaneo (0ms) ate bem "escorregado".
static const float GLIDE_MS_MIN = 0.0f;
static const float GLIDE_MS_MAX = 150.0f;
static const float GLIDE_MS_DEFAULT = 25.0f;

static const uint32_t DEBOUNCE_MS = 25;
static const uint32_t ADC_POLL_MS = 12; // intervalo de leitura de pots/joystick (baixo = resposta mais fluida)
