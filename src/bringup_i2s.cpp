// TESTE 1: PCM5102A (I2S)
// Toca a aberturazinha de "Ode a Alegria" em loop, pra confirmar a fiacao
// do DAC de um jeito facil de reconhecer (bem melhor que so um tom fixo).
// Grave com: pio run -e test_i2s -t upload
//
// Se nao ouvir nada, confira NESSA ORDEM:
//   1) XSMT do modulo esta em 3.3V? (sem isso ele fica mudo)
//   2) SCK do modulo esta em GND?
//   3) BCK/DIN/LRCK batem com GPIO27/26/25?
//   4) VIN/GND do modulo tem 3.3V mesmo (meça com multimetro)?
//   5) LROUT/AGND estao soldados no jack, e o fone/caixa esta plugado e com volume?

#include <Arduino.h>
#include "Config.h"
#include <driver/i2s.h>

// Notas em Hz (oitava 4) usadas na melodia
static const float NOTE_C4 = 261.63f;
static const float NOTE_D4 = 293.66f;
static const float NOTE_E4 = 329.63f;
static const float NOTE_F4 = 349.23f;
static const float NOTE_G4 = 392.00f;

struct MelodyNote {
    float freq;
    uint32_t durationMs;
};

// Abertura de "Ode a Alegria" (Beethoven), ritmo simplificado
static const MelodyNote MELODY[] = {
    {NOTE_E4, 300}, {NOTE_E4, 300}, {NOTE_F4, 300}, {NOTE_G4, 300},
    {NOTE_G4, 300}, {NOTE_F4, 300}, {NOTE_E4, 300}, {NOTE_D4, 300},
    {NOTE_C4, 300}, {NOTE_C4, 300}, {NOTE_D4, 300}, {NOTE_E4, 300},
    {NOTE_E4, 450}, {NOTE_D4, 150}, {NOTE_D4, 600},
};
static const int MELODY_LEN = sizeof(MELODY) / sizeof(MELODY[0]);

static int16_t audioBuf[I2S_DMA_BUF_LEN * 2];

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("Teste I2S / PCM5102A - tocando Ode a Alegria em loop");

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = I2S_DMA_BUF_COUNT,
        .dma_buf_len = I2S_DMA_BUF_LEN,
        .use_apll = false,
        .tx_desc_auto_clear = true
    };
    i2s_pin_config_t pin_config = {
        .bck_io_num = PIN_I2S_BCK,
        .ws_io_num = PIN_I2S_WS,
        .data_out_num = PIN_I2S_DOUT,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    i2s_zero_dma_buffer(I2S_NUM_0);

    Serial.println("I2S instalado. Se estiver tudo certo, a musica comeca agora.");
}

void loop() {
    static int noteIndex = 0;
    static uint32_t samplesIntoNote = 0;
    static float phase = 0.0f;

    uint32_t noteDurationSamples = (uint32_t)SAMPLE_RATE * MELODY[noteIndex].durationMs / 1000;
    uint32_t gateSamples = (uint32_t)(noteDurationSamples * 0.85f); // 85% soa, 15% silencio (articulacao entre notas)
    float phaseInc = MELODY[noteIndex].freq / (float)SAMPLE_RATE;

    for (int i = 0; i < I2S_DMA_BUF_LEN; i++) {
        int16_t sample = 0;

        if (samplesIntoNote < gateSamples) {
            sample = (int16_t)(sinf(phase * 2.0f * PI) * 12000.0f);
            phase += phaseInc;
            if (phase >= 1.0f) phase -= 1.0f;
        }

        audioBuf[i * 2 + 0] = sample;
        audioBuf[i * 2 + 1] = sample;

        samplesIntoNote++;
        if (samplesIntoNote >= noteDurationSamples) {
            samplesIntoNote = 0;
            noteIndex = (noteIndex + 1) % MELODY_LEN;
            phase = 0.0f; // evita "clique" de fase ao trocar de nota
            noteDurationSamples = (uint32_t)SAMPLE_RATE * MELODY[noteIndex].durationMs / 1000;
            gateSamples = (uint32_t)(noteDurationSamples * 0.85f);
            phaseInc = MELODY[noteIndex].freq / (float)SAMPLE_RATE;
        }
    }

    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, audioBuf, sizeof(audioBuf), &bytesWritten, portMAX_DELAY);
}
