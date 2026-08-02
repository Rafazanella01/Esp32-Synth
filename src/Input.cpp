#include "Input.h"
#include "Config.h"

static int tempoBPM = ARP_BPM_DEFAULT;
static float timbrePosition = 0.0f;
static float glideMs = GLIDE_MS_DEFAULT;
static float arpGate = ARP_GATE_DEFAULT;
static ChordType joystickChord = ChordType::MAJOR;

static uint32_t lastPollMs = 0;

// botao do joystick (debounced, estado atual)
static bool lastButtonRaw = false;
static bool buttonStable = false;
static uint32_t buttonLastChangeMs = 0;

static const int ADC_MAX = 4095;

// Se "cima"/"baixo" (ou toda a rotacao) sair invertida na pratica - depende
// de como o eixo Y do seu joystick fisico foi montado - so virar isso pra
// true e testar de novo. Nao precisa mexer em mais nada.
static const bool JOY_Y_INVERTED = false;

// -----------------------------------------------------------------------------
// Suavizacao de ADC: leitura crua de potenciometro/joystick sempre tem um
// pouco de ruido eletrico (mesmo com a fiacao certa). Sem filtro, esse ruido
// pode fazer o valor "tremer" o suficiente pra cruzar limites de faixa (ex:
// o acorde "piscar" perto da fronteira entre duas direcoes). Um filtro de
// media movel exponencial (EMA) resolve isso sem precisar de capacitor
// nenhum no circuito - e ja de quebra deixa o timbre variando suave.
static const float ADC_SMOOTHING_ALPHA = 0.2f; // 0..1 - menor = mais suave (e mais lento)

static float readSmoothed(int pin, float &state) {
    int raw = analogRead(pin);
    state += ADC_SMOOTHING_ALPHA * ((float)raw - state);
    return state;
}

static float filteredTempoRaw = (float)(ADC_MAX / 2);
static float filteredTimbreRaw = (float)(ADC_MAX / 2);
static float filteredGlideRaw = (float)(ADC_MAX / 2);
static float filteredArpGateRaw = (float)(ADC_MAX / 2);
static float filteredJoyXRaw = (float)(ADC_MAX / 2);
static float filteredJoyYRaw = (float)(ADC_MAX / 2);

// Centro real de cada eixo, medido no boot em vez de assumir 2047 - joystick
// barato quase nunca repousa exatamente no meio da escala.
static float joyCenterX = (float)(ADC_MAX / 2);
static float joyCenterY = (float)(ADC_MAX / 2);

// Debounce por "voto": so troca de direcao do joystick depois de N leituras
// seguidas apontando pra outra zona, pra nao ficar piscando perto de uma
// fronteira entre direcoes. Baixo de proposito (a suavizacao do EMA acima
// ja segura a maior parte do ruido, entao nao precisa de muitas votacoes
// pra responder rapido).
static const int CHORD_ZONE_CONFIRM_COUNT = 2;
static int8_t pendingChordZone = (int8_t)ChordType::MAJOR;
static bool pendingCentered = true;
static bool joystickCentered = true;
static int chordZoneStreak = 0;

void inputInit() {
    analogReadResolution(12); // 0..4095
    // Atenuacao maxima nos eixos do joystick: sem isso a faixa util do ADC
    // fica bem menor que 3.3V e o curso pra um dos lados satura cedo.
    analogSetPinAttenuation(PIN_JOY_X, ADC_11db);
    analogSetPinAttenuation(PIN_JOY_Y, ADC_11db);
    pinMode(PIN_JOY_SW, INPUT_PULLUP);

    // Calibracao do centro: le o joystick parado algumas vezes e guarda a
    // media como "repouso". Tudo depois disso e medido em relacao a esse
    // ponto, nao ao meio teorico da escala.
    long sumX = 0;
    long sumY = 0;
    for (int i = 0; i < JOY_CALIB_SAMPLES; i++) {
        sumX += analogRead(PIN_JOY_X);
        sumY += analogRead(PIN_JOY_Y);
        delay(2);
    }
    joyCenterX = (float)sumX / (float)JOY_CALIB_SAMPLES;
    joyCenterY = (float)sumY / (float)JOY_CALIB_SAMPLES;
    filteredJoyXRaw = joyCenterX;
    filteredJoyYRaw = joyCenterY;
}

static void pollPots() {
    float tempoRaw = readSmoothed(PIN_POT_TEMPO, filteredTempoRaw);
    tempoBPM = map((long)tempoRaw, 0, ADC_MAX, ARP_BPM_MIN, ARP_BPM_MAX);

    // Timbre e continuo (sem zona/degrau) - o proprio valor suavizado ja
    // e a posicao de mistura entre wavetables, entao girar o pote soa
    // como um morph fluido em vez de trocar bruscamente de forma de onda.
    float timbreRaw = readSmoothed(PIN_POT_TIMBRE, filteredTimbreRaw);
    float maxPos = (float)((int)Waveform::COUNT - 1);
    timbrePosition = constrain((timbreRaw / (float)ADC_MAX) * maxPos, 0.0f, maxPos);

    // Pots opcionais: se nao estiverem ligados, nem le o pino (um pino solto
    // flutua e faria o glide/gate variarem sozinhos). Ver Config.h.
    if (POT_GLIDE_CONNECTED) {
        float glideRaw = readSmoothed(PIN_POT_GLIDE, filteredGlideRaw);
        glideMs = GLIDE_MS_MIN + (glideRaw / (float)ADC_MAX) * (GLIDE_MS_MAX - GLIDE_MS_MIN);
    }
    if (POT_ARP_GATE_CONNECTED) {
        float gateRaw = readSmoothed(PIN_POT_ARP_GATE, filteredArpGateRaw);
        arpGate = ARP_GATE_MIN + (gateRaw / (float)ADC_MAX) * (ARP_GATE_MAX - ARP_GATE_MIN);
    }
}

// Converte a leitura crua de um eixo pra -1..+1, medindo cada lado pelo seu
// PROPRIO curso a partir do centro calibrado. E isso que iguala a
// sensibilidade entre "pra cima" e "pra baixo" mesmo com o ADC do ESP32
// comprimindo a parte alta da escala.
static float normalizeAxis(float raw, float center) {
    if (raw >= center) {
        float span = (float)ADC_MAX - center;
        if (span < 1.0f) return 0.0f;
        return constrain((raw - center) / span, 0.0f, 1.0f);
    }
    float span = center;
    if (span < 1.0f) return 0.0f;
    return constrain((raw - center) / span, -1.0f, 0.0f);
}

// Classifica a posicao do joystick em uma das 10 zonas: centro, clique, ou
// uma das 8 direcoes (4 cardeais + 4 diagonais), usando o angulo da
// deflexao em vez de so comparar dx/dy - assim as diagonais funcionam
// direito, nao so os 4 eixos principais.
static ChordType classifyJoystick(float nx, float ny, bool clicked, bool &centeredOut) {
    if (JOY_Y_INVERTED) ny = -ny;

    centeredOut = false;

    if (clicked) return ChordType::SIXTH;

    float mag2 = nx * nx + ny * ny;
    bool deflected = mag2 > (JOY_DEADZONE_NORM * JOY_DEADZONE_NORM);

    // Centro = Maior, o acorde "padrao" do arpejo. NATURAL (nota pura) nao
    // aparece aqui de proposito: quem decide que "centro" vira nota pura e
    // o main.cpp, e so quando a tecla esta sendo segurada (ver
    // isJoystickCentered) - senao um toque rapido com o joystick parado
    // sairia como nota sozinha em vez de arranjo.
    if (!deflected) {
        centeredOut = true;
        return ChordType::MAJOR;
    }

    // angulo em graus, 0 = direita (E), crescendo anti-horario (convencao
    // trigonometrica padrao: ny positivo = "pra cima")
    float angleDeg = atan2f(ny, nx) * 180.0f / PI;
    if (angleDeg < 0) angleDeg += 360.0f;

    int sector = ((int)((angleDeg + 22.5f) / 45.0f)) % 8;

    static const ChordType SECTOR_CHORD[8] = {
        ChordType::AUGMENTED,   // 0: direita       (E)
        ChordType::DOMINANT7,   // 1: cima-direita  (NE)
        ChordType::MAJOR7,      // 2: cima          (N)
        ChordType::SUS2,        // 3: cima-esquerda (NW)
        ChordType::DIMINISHED,  // 4: esquerda      (W)
        ChordType::MINOR,       // 5: baixo-esquerda(SW)
        ChordType::MINOR7,      // 6: baixo         (S)
        ChordType::SUS4,        // 7: baixo-direita (SE)
    };
    return SECTOR_CHORD[sector];
}

static void pollJoystick() {
    float xRaw = readSmoothed(PIN_JOY_X, filteredJoyXRaw);
    float yRaw = readSmoothed(PIN_JOY_Y, filteredJoyYRaw);
    float nx = normalizeAxis(xRaw, joyCenterX);
    float ny = normalizeAxis(yRaw, joyCenterY);

    bool raw = (digitalRead(PIN_JOY_SW) == LOW); // ativo em LOW
    uint32_t now = millis();
    if (raw != lastButtonRaw) {
        lastButtonRaw = raw;
        buttonLastChangeMs = now;
    } else if ((now - buttonLastChangeMs) > DEBOUNCE_MS) {
        buttonStable = raw;
    }

    bool centered = false;
    ChordType zone = classifyJoystick(nx, ny, buttonStable, centered);

    if (zone == (ChordType)pendingChordZone) {
        chordZoneStreak++;
    } else {
        pendingChordZone = (int8_t)zone;
        pendingCentered = centered;
        chordZoneStreak = 1;
    }
    if (chordZoneStreak >= CHORD_ZONE_CONFIRM_COUNT) {
        joystickChord = zone;
        joystickCentered = pendingCentered;
    }
}

void inputUpdate() {
    uint32_t now = millis();
    if (now - lastPollMs < ADC_POLL_MS) return;
    lastPollMs = now;

    pollPots();
    pollJoystick();
}

int getTempoBPM() { return tempoBPM; }
float getTimbrePosition() { return timbrePosition; }
float getGlideMs() { return glideMs; }
float getArpGate() { return arpGate; }
ChordType getJoystickChordType() { return joystickChord; }
bool isJoystickCentered() { return joystickCentered; }
