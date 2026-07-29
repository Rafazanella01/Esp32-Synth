// TESTE 1: PCM5102A (I2S)
// Toca um tom continuo de 440Hz (nota A) pra confirmar a fiacao do DAC.
// Grave com: pio run -e test_i2s -t upload
//
// Se nao ouvir nada, confira NESSA ORDEM:
//   1) XSMT do modulo esta em 3.3V? (sem isso ele fica mudo)
//   2) SCK do modulo esta em GND?
//   3) BCK/WS/DIN batem com GPIO27/25/26?
//   4) VIN/GND do modulo tem 3.3V mesmo (meça com multimetro)?
//   5) Fone/caixa esta no jack certo e ligado/com volume?

#include <Arduino.h>
#include "Config.h"
#include <driver/i2s.h>

static const float TONE_FREQ = 440.0f; // A4
static int16_t audioBuf[I2S_DMA_BUF_LEN * 2];

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.println("Teste I2S / PCM5102A - tom de 440Hz continuo");

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

    Serial.println("I2S instalado. Se estiver tudo certo, o tom comeca agora.");
}

void loop() {
    static float phase = 0.0f;
    float phaseInc = TONE_FREQ / (float)SAMPLE_RATE; // em voltas de ciclo (0..1)

    for (int i = 0; i < I2S_DMA_BUF_LEN; i++) {
        int16_t sample = (int16_t)(sinf(phase * 2.0f * PI) * 12000.0f); // volume moderado
        phase += phaseInc;
        if (phase >= 1.0f) phase -= 1.0f;
        audioBuf[i * 2 + 0] = sample;
        audioBuf[i * 2 + 1] = sample;
    }

    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, audioBuf, sizeof(audioBuf), &bytesWritten, portMAX_DELAY);
}
