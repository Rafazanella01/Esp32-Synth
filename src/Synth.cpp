#include "Synth.h"
#include "Config.h"
#include <driver/i2s.h>

static bool playing = false;
static uint8_t currentRoot = 0;
static ChordType currentChordType = ChordType::MAJOR;
static Waveform currentTimbre = Waveform::SINE;
static int currentBPM = ARP_BPM_DEFAULT;
static float pitchBendSemis = 0.0f;

static uint8_t arpStep = 0;
static uint32_t samplesIntoStep = 0;
static uint32_t samplesPerStep = SAMPLE_RATE * 60 / ARP_BPM_DEFAULT / ARP_NOTE_DIV;

static float phaseAcc = 0.0f; // indice fracionario dentro da wavetable (0..WAVETABLE_SIZE)

static int16_t audioBuf[I2S_DMA_BUF_LEN * 2]; // interleaved L/R

static const uint32_t ATTACK_MS = 4;
static const uint32_t RELEASE_MS = 15;

static void recomputeSamplesPerStep() {
    samplesPerStep = (uint32_t)SAMPLE_RATE * 60UL / (uint32_t)currentBPM / (uint32_t)ARP_NOTE_DIV;
    if (samplesPerStep < 8) samplesPerStep = 8; // guarda contra tempo absurdo/divisao ruim
}

void synthInit() {
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

    recomputeSamplesPerStep();
}

void synthTriggerRoot(uint8_t rootNote) {
    if (playing && currentRoot == rootNote) {
        synthStop();
        return;
    }
    currentRoot = rootNote;
    playing = true;
    arpStep = 0;
    samplesIntoStep = 0;
}

void synthSetChordType(ChordType type) {
    currentChordType = type;
    arpStep = 0;
    samplesIntoStep = 0;
}

void synthSetTimbre(Waveform w) { currentTimbre = w; }

void synthSetTempoBPM(int bpm) {
    if (bpm == currentBPM) return;
    currentBPM = bpm;
    recomputeSamplesPerStep();
}

void synthSetPitchBendSemitones(float semis) { pitchBendSemis = semis; }

void synthStop() {
    playing = false;
    samplesIntoStep = 0;
    arpStep = 0;
}

bool synthIsPlaying() { return playing; }
uint8_t synthCurrentRoot() { return currentRoot; }
ChordType synthCurrentChordType() { return currentChordType; }

static float currentStepFrequency() {
    const ChordDef &def = chordDef(currentChordType);
    uint8_t stepInChord = arpStep % def.numNotes;
    int8_t semis = def.intervals[stepInChord];
    uint8_t noteIndex = (currentRoot + semis) % 12;
    uint8_t octaveShift = (currentRoot + semis) / 12; // acordes que passam de B voltam pra proxima oitava
    float freq = noteFrequency(noteIndex, BASE_FREQ_C4) * powf(2.0f, (float)octaveShift);
    float bendMul = powf(2.0f, pitchBendSemis / 12.0f);
    return freq * bendMul;
}

static int16_t envelopeSample(int16_t raw) {
    uint32_t gateSamples = (uint32_t)(samplesPerStep * ARP_GATE);
    uint32_t attackSamples = min((uint32_t)(SAMPLE_RATE / 1000 * ATTACK_MS), gateSamples / 3 + 1);
    uint32_t releaseSamples = min((uint32_t)(SAMPLE_RATE / 1000 * RELEASE_MS), gateSamples / 3 + 1);

    float amp;
    if (samplesIntoStep >= gateSamples) {
        amp = 0.0f; // parte "silenciosa" do passo (deixa o groove respirar)
    } else if (samplesIntoStep < attackSamples) {
        amp = (float)samplesIntoStep / (float)attackSamples;
    } else if (samplesIntoStep >= gateSamples - releaseSamples) {
        amp = (float)(gateSamples - samplesIntoStep) / (float)releaseSamples;
    } else {
        amp = 1.0f;
    }
    return (int16_t)((float)raw * amp);
}

void synthProcess() {
    static float freq = 0;

    if (playing && samplesIntoStep == 0) {
        freq = currentStepFrequency();
    }

    const int16_t *table = waveTables[(int)currentTimbre];

    for (int i = 0; i < I2S_DMA_BUF_LEN; i++) {
        int16_t sample = 0;

        if (playing) {
            // recalculado por amostra (barato p/ ESP32) pra nao "arrastar" a
            // frequencia velha quando um passo do arpejo muda no meio do buffer
            float phaseInc = freq * (float)WAVETABLE_SIZE / (float)SAMPLE_RATE;

            int idx = (int)phaseAcc;
            if (idx >= WAVETABLE_SIZE) idx = WAVETABLE_SIZE - 1;
            sample = envelopeSample(table[idx]);

            phaseAcc += phaseInc;
            if (phaseAcc >= WAVETABLE_SIZE) phaseAcc -= WAVETABLE_SIZE;

            samplesIntoStep++;
            if (samplesIntoStep >= samplesPerStep) {
                samplesIntoStep = 0;
                arpStep++;
                freq = currentStepFrequency();
            }
        }

        audioBuf[i * 2 + 0] = sample; // canal esquerdo
        audioBuf[i * 2 + 1] = sample; // canal direito (mono duplicado)
    }

    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, audioBuf, sizeof(audioBuf), &bytesWritten, portMAX_DELAY);
}
