#include "Synth.h"
#include "Config.h"
#include <driver/i2s.h>

enum class NoteState : uint8_t { IDLE, PLAYING, RELEASING };

static NoteState noteState = NoteState::IDLE;
static uint8_t currentRoot = 0;
static ChordType currentChordType = ChordType::NATURAL;
static float timbrePos = 0.0f; // 0..(Waveform::COUNT-1), continuo
static int currentBPM = ARP_BPM_DEFAULT;

static uint8_t arpStep = 0;
static uint32_t samplesIntoStep = 0;
static uint32_t samplesPerStep = SAMPLE_RATE * 60 / ARP_BPM_DEFAULT / ARP_NOTE_DIV;

static float phaseAcc = 0.0f; // indice fracionario dentro da wavetable (0..WAVETABLE_SIZE)
static float noteGain = 0.0f; // envelope geral da nota (ataque ao ligar, release ao soltar)

static float targetFreq = 0.0f; // frequencia "alvo" do passo/nota atual
static float currentFreq = 0.0f; // frequencia real do oscilador, desliza ate o alvo (glide)

static int16_t audioBuf[I2S_DMA_BUF_LEN * 2]; // interleaved L/R

static const uint32_t ATTACK_MS = 6;
static const uint32_t RELEASE_MS = 25;
static const uint32_t STEP_ATTACK_MS = 4;  // ataque curto de cada passo do arpejo
static const uint32_t STEP_RELEASE_MS = 15;

// Portamento: a cada amostra, currentFreq anda uma fracao glideAlpha do
// caminho ate targetFreq (suavizacao exponencial, mesmo principio do
// filtro de ADC em Input.cpp). Controlado ao vivo por synthSetGlideMs()
// (ligado ao PIN_POT_GLIDE).
static float glideAlpha = 1000.0f / (GLIDE_MS_DEFAULT * (float)SAMPLE_RATE);

// Gate do arpejo (fracao do passo em que a nota soa) - controlado ao vivo
// por synthSetArpGate() (ligado ao PIN_POT_ARP_GATE).
static float currentArpGate = ARP_GATE_DEFAULT;

static void recomputeSamplesPerStep() {
    samplesPerStep = (uint32_t)SAMPLE_RATE * 60UL / (uint32_t)currentBPM / (uint32_t)ARP_NOTE_DIV;
    if (samplesPerStep < 8) samplesPerStep = 8; // guarda contra tempo absurdo/divisao ruim
}

static float stepFrequency() {
    const ChordDef &def = chordDef(currentChordType);
    uint8_t stepInChord = arpStep % def.numNotes;
    int8_t semis = def.intervals[stepInChord];
    uint16_t sum = (uint16_t)currentRoot + (uint16_t)semis;
    uint8_t noteIndex = sum % 12;
    uint8_t octaveShift = sum / 12;
    return noteFrequency(noteIndex, BASE_FREQ_C4) * powf(2.0f, (float)octaveShift);
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

void synthNoteOn(uint8_t rootNote) {
    if (noteState == NoteState::IDLE) {
        arpStep = 0;
        samplesIntoStep = 0;
        // currentFreq NAO e resetado aqui de proposito: se a nota anterior
        // ainda estava deslizando, a nova troca continua o glide de onde
        // parou em vez de saltar seco pro inicio.
    }
    currentRoot = rootNote;
    targetFreq = stepFrequency(); // recalcula na hora - o glide cuida da transicao suave
    noteState = NoteState::PLAYING;
}

void synthNoteOff() {
    if (noteState == NoteState::PLAYING) {
        noteState = NoteState::RELEASING;
    }
}

void synthSetChordType(ChordType type) {
    if (type == currentChordType) return;
    currentChordType = type;
    arpStep = 0;
    samplesIntoStep = 0;
    if (noteState != NoteState::IDLE) {
        targetFreq = stepFrequency(); // atualiza o alvo na hora - o glide desliza ate la
    }
}

void synthSetTimbrePosition(float pos) {
    float maxPos = (float)((int)Waveform::COUNT - 1);
    timbrePos = constrain(pos, 0.0f, maxPos);
}

void synthSetTempoBPM(int bpm) {
    if (bpm == currentBPM) return;
    currentBPM = bpm;
    recomputeSamplesPerStep();
}

void synthSetGlideMs(float ms) {
    ms = constrain(ms, GLIDE_MS_MIN, GLIDE_MS_MAX);
    if (ms < 1.0f) {
        glideAlpha = 1.0f; // praticamente instantaneo (sem glide perceptivel)
    } else {
        glideAlpha = 1000.0f / (ms * (float)SAMPLE_RATE);
    }
}

void synthSetArpGate(float gate) {
    currentArpGate = constrain(gate, ARP_GATE_MIN, ARP_GATE_MAX);
}

bool synthIsPlaying() { return noteState != NoteState::IDLE; }
uint8_t synthCurrentRoot() { return currentRoot; }
ChordType synthCurrentChordType() { return currentChordType; }

// Envelope de cada passo do arpejo (soa por ARP_GATE do passo, com
// ataque/release curtos). Em modo NATURAL (1 nota so) isso nao e usado -
// o som fica sustentado o tempo todo, so o noteGain geral manda.
static float stepGateEnvelope() {
    uint32_t gateSamples = (uint32_t)(samplesPerStep * currentArpGate);
    uint32_t attackSamples = min((uint32_t)(SAMPLE_RATE / 1000 * STEP_ATTACK_MS), gateSamples / 3 + 1);
    uint32_t releaseSamples = min((uint32_t)(SAMPLE_RATE / 1000 * STEP_RELEASE_MS), gateSamples / 3 + 1);

    if (samplesIntoStep >= gateSamples) return 0.0f;
    if (samplesIntoStep < attackSamples) return (float)samplesIntoStep / (float)attackSamples;
    if (samplesIntoStep >= gateSamples - releaseSamples) {
        return (float)(gateSamples - samplesIntoStep) / (float)releaseSamples;
    }
    return 1.0f;
}

void synthProcess() {
    bool sustainedMode = (chordDef(currentChordType).numNotes == 1);

    int loIdx = (int)timbrePos;
    int hiIdx = min(loIdx + 1, (int)Waveform::COUNT - 1);
    float timbreFrac = timbrePos - (float)loIdx;
    const int16_t *tableLo = waveTables[loIdx];
    const int16_t *tableHi = waveTables[hiIdx];

    float noteGainStepInc = 1.0f / (float)((SAMPLE_RATE / 1000) * ATTACK_MS);
    float noteGainStepDec = 1.0f / (float)((SAMPLE_RATE / 1000) * RELEASE_MS);

    for (int i = 0; i < I2S_DMA_BUF_LEN; i++) {
        int16_t sample = 0;

        if (noteState != NoteState::IDLE) {
            currentFreq += (targetFreq - currentFreq) * glideAlpha;

            float phaseInc = currentFreq * (float)WAVETABLE_SIZE / (float)SAMPLE_RATE;

            int idx = (int)phaseAcc;
            if (idx >= WAVETABLE_SIZE) idx = WAVETABLE_SIZE - 1;

            float rawSample = (float)tableLo[idx] * (1.0f - timbreFrac) + (float)tableHi[idx] * timbreFrac;

            float gate = sustainedMode ? 1.0f : stepGateEnvelope();
            sample = (int16_t)(rawSample * gate * noteGain);

            phaseAcc += phaseInc;
            if (phaseAcc >= WAVETABLE_SIZE) phaseAcc -= WAVETABLE_SIZE;

            if (!sustainedMode) {
                samplesIntoStep++;
                if (samplesIntoStep >= samplesPerStep) {
                    samplesIntoStep = 0;
                    arpStep++;
                    targetFreq = stepFrequency();
                }
            }

            // envelope geral da nota (ataque ao entrar, release ao soltar)
            if (noteState == NoteState::PLAYING) {
                if (noteGain < 1.0f) {
                    noteGain += noteGainStepInc;
                    if (noteGain > 1.0f) noteGain = 1.0f;
                }
            } else { // RELEASING
                noteGain -= noteGainStepDec;
                if (noteGain <= 0.0f) {
                    noteGain = 0.0f;
                    noteState = NoteState::IDLE;
                }
            }
        }

        audioBuf[i * 2 + 0] = sample; // canal esquerdo
        audioBuf[i * 2 + 1] = sample; // canal direito (mono duplicado)
    }

    size_t bytesWritten = 0;
    i2s_write(I2S_NUM_0, audioBuf, sizeof(audioBuf), &bytesWritten, portMAX_DELAY);
}
