#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <driver/i2s.h>
#include <math.h>
#include "ui.h"

// ---------- Configuração ----------
static const char* AP_SSID = "SynthCDP";
static const char* AP_PASS = "synth1234";   // mínimo 8 caracteres

static const int PIN_I2S_BCK  = 4;
static const int PIN_I2S_WS   = 5;
static const int PIN_I2S_DOUT = 6;

static const int SAMPLE_RATE = 32000;
static const int BLOCK       = 64;
static const int NUM_VOICES  = 8;
static const int LINE_LEN    = 1024;        // potência de 2: linha de delay da corda
static const int LINE_MASK   = LINE_LEN - 1;

// ---------- Parâmetros (normalizados 0..1, escritos pela rede) ----------
enum ParamId {
  P_CUTOFF, P_RES, P_ATTACK, P_DECAY, P_SUSTAIN, P_RELEASE, P_VOLUME,
  P_PICKUP, P_TONE, P_STRING, P_CLICK, P_WAH, P_WAHAMT, P_WAHMIX,
  P_COUNT
};
static const char* PARAM_NAMES[P_COUNT] = {
  "cutoff", "res", "attack", "decay", "sustain", "release", "volume",
  "pickup", "tone", "string", "click", "wah", "wahamt", "wahmix"
};
static volatile float params[P_COUNT] = {
  0.6f, 0.2f, 0.05f, 0.4f, 0.7f, 0.4f, 0.6f,
  0.35f, 0.7f, 0.35f, 0.3f, 0.5f, 0.4f, 0.4f
};
static volatile int waveform = 0;  // 0 serra, 1 quadrada, 2 triângulo (modo synth)
static volatile int mode = 1;      // 0 synth, 1 clavinet

// ---------- Eventos de nota (rede/USB -> áudio) ----------
struct NoteEvent { uint8_t type; uint8_t note; uint8_t vel; };  // type: 1 on, 0 off
static QueueHandle_t noteQueue;

void pushNote(uint8_t on, uint8_t note, uint8_t vel) {
  NoteEvent ev = { on, note, vel };
  xQueueSend(noteQueue, &ev, 0);
}

// Ponto de entrada para MIDI vindo do piano (USB host). Ver README: falta implementar o host.
void midiIn(uint8_t status, uint8_t d1, uint8_t d2) {
  uint8_t cmd = status & 0xF0;
  if (cmd == 0x90 && d2 > 0) pushNote(1, d1, d2);
  else if (cmd == 0x80 || cmd == 0x90) pushNote(0, d1, 0);
}

// ---------- Motor de áudio ----------
enum Stage { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };

struct Voice {
  Stage stage = IDLE;
  uint8_t note = 0;
  uint32_t age = 0;
  float vel = 0;
  // modo synth
  float phase = 0, inc = 0, env = 0, low = 0, band = 0;
  // modo clavinet (corda por Karplus-Strong)
  bool clav = false;
  bool released = false;
  int wpos = 0;
  float D = 100, g = 0.99f, gRel = 0.5f, lp = 0, lpc = 0.8f, peak = 0, clickEnv = 0;
  float pick = 0.2f;
  uint32_t samples = 0;
  float line[LINE_LEN];
};

static Voice voices[NUM_VOICES];
static uint32_t ageCounter = 0;
static uint32_t rngState = 22222;

static inline float frand() {  // -1..1
  rngState = rngState * 1664525u + 1013904223u;
  return ((int32_t)rngState) * (1.0f / 2147483648.0f);
}

static inline float coefFromTime(float t) { return 1.0f - expf(-1.0f / (t * SAMPLE_RATE)); }

static inline float readLine(const Voice& v, float delay) {
  float pos = (float)v.wpos - delay;
  int i0 = (int)floorf(pos);
  float fr = pos - (float)i0;
  float a = v.line[i0 & LINE_MASK];
  float b = v.line[(i0 + 1) & LINE_MASK];
  return a + (b - a) * fr;
}

static void startClav(Voice& v, uint8_t note, float velN) {
  float f = 440.0f * powf(2.0f, (note - 69) / 12.0f);
  v.lpc = 0.35f + 0.65f * params[P_TONE];
  // um polo passa-baixa atrasa ~ (1-c)/c amostras: compensa no comprimento do laço
  float D = (float)SAMPLE_RATE / f - (1.0f - v.lpc) / v.lpc;
  if (D < 2.0f) D = 2.0f;
  if (D > LINE_LEN - 4) D = LINE_LEN - 4;
  v.D = D;
  float t60 = 0.05f * powf(120.0f, params[P_STRING]);
  v.g = powf(0.001f, D / (SAMPLE_RATE * t60));
  v.gRel = powf(0.001f, D / (SAMPLE_RATE * 0.025f));   // abafador de feltro ao soltar
  v.pick = 0.05f + 0.4f * params[P_PICKUP];

  memset(v.line, 0, sizeof(v.line));
  int n = (int)ceilf(D) + 2;
  float c = 0.2f + 0.8f * velN;          // tecla fraca = excitação mais escura
  float amp = 0.3f + 0.7f * velN;
  float lpn = 0, mean = 0;
  for (int i = 0; i < n; i++) {
    lpn += c * (frand() - lpn);
    v.line[i] = lpn;
    mean += lpn;
  }
  mean /= n;
  for (int i = 0; i < n; i++) v.line[i] = (v.line[i] - mean) * amp * 2.5f;
  v.wpos = n;
  v.lp = 0;
  v.peak = 1.0f;
  v.clickEnv = velN;
  v.released = false;
  v.samples = 0;
  v.clav = true;
}

static void voiceOn(uint8_t note, uint8_t vel) {
  int pick = -1;
  uint32_t oldest = 0xFFFFFFFF;
  for (int i = 0; i < NUM_VOICES; i++) {
    if (voices[i].stage == IDLE) { pick = i; break; }
    if (voices[i].age < oldest) { oldest = voices[i].age; pick = i; }
  }
  Voice& v = voices[pick];
  v.note = note;
  v.vel = vel / 127.0f;
  v.age = ++ageCounter;
  v.stage = ATTACK;
  if (mode == 1) {
    startClav(v, note, v.vel);
  } else {
    v.clav = false;
    v.released = false;
    v.inc = 440.0f * powf(2.0f, (note - 69) / 12.0f) / SAMPLE_RATE;
    v.phase = 0;
    v.env = 0;
    v.low = v.band = 0;
  }
}

static void voiceOff(uint8_t note) {
  for (int i = 0; i < NUM_VOICES; i++) {
    Voice& v = voices[i];
    if (v.stage == IDLE || v.note != note || v.released) continue;
    if (v.clav) { v.released = true; v.stage = RELEASE; }
    else if (v.stage != RELEASE) v.stage = RELEASE;
  }
}

// estado do wah (global, depois da soma das vozes)
static float wahLow = 0, wahBand = 0, wahEnv = 0;

static void renderBlock(int16_t* out) {
  NoteEvent ev;
  while (xQueueReceive(noteQueue, &ev, 0) == pdTRUE) {
    if (ev.type) voiceOn(ev.note, ev.vel); else voiceOff(ev.note);
  }

  const float cutoff  = fminf(40.0f * powf(150.0f, params[P_CUTOFF]), 6000.0f);
  const float fcoef   = 2.0f * sinf(M_PI * cutoff / SAMPLE_RATE);
  const float q       = 2.0f - 1.9f * params[P_RES];
  const float atkRate = 1.0f / (0.002f * powf(1000.0f, params[P_ATTACK]) * SAMPLE_RATE);
  const float decCoef = coefFromTime(0.01f * powf(300.0f, params[P_DECAY]));
  const float relCoef = coefFromTime(0.01f * powf(500.0f, params[P_RELEASE]));
  const float sus     = params[P_SUSTAIN];
  const float vol     = params[P_VOLUME];
  const int   wave    = waveform;
  const float clickAmt = params[P_CLICK] * 0.4f;
  const float pickupK  = 0.05f + 0.4f * params[P_PICKUP];
  const float wahPos   = params[P_WAH];
  const float wahAmt   = params[P_WAHAMT];
  const float wahMix   = params[P_WAHMIX];
  const bool  clavMode = (mode == 1);

  float mix[BLOCK];
  for (int n = 0; n < BLOCK; n++) mix[n] = 0;

  for (int i = 0; i < NUM_VOICES; i++) {
    Voice& v = voices[i];
    if (v.stage == IDLE) continue;

    if (v.clav) {
      const float gcur = v.released ? v.gRel : v.g;
      for (int n = 0; n < BLOCK; n++) {
        float x1 = readLine(v, v.D);
        float x2 = readLine(v, fminf(v.D * (1.0f + pickupK), (float)(LINE_LEN - 4)));
        v.lp += v.lpc * (x1 - v.lp);
        v.line[v.wpos & LINE_MASK] = v.lp * gcur;
        v.wpos++;

        float o = (x1 - x2) * 0.6f;
        if (v.clickEnv > 0.001f) { o += frand() * v.clickEnv * clickAmt; v.clickEnv *= 0.97f; }
        mix[n] += o;

        float a = fabsf(o);
        v.peak = a > v.peak ? a : v.peak * 0.9995f;
        v.samples++;
      }
      if (v.samples > 256 && v.peak < 0.0003f) v.stage = IDLE;
      continue;
    }

    for (int n = 0; n < BLOCK; n++) {
      v.phase += v.inc;
      if (v.phase >= 1.0f) v.phase -= 1.0f;
      float s;
      if (wave == 0)      s = 2.0f * v.phase - 1.0f;
      else if (wave == 1) s = v.phase < 0.5f ? 1.0f : -1.0f;
      else                s = 4.0f * fabsf(v.phase - 0.5f) - 1.0f;

      switch (v.stage) {
        case ATTACK:  v.env += atkRate; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = DECAY; } break;
        case DECAY:   v.env += (sus - v.env) * decCoef; if (fabsf(v.env - sus) < 0.001f) v.stage = SUSTAIN; break;
        case SUSTAIN: v.env = sus; break;
        case RELEASE: v.env -= v.env * relCoef; if (v.env < 0.0005f) { v.stage = IDLE; v.env = 0; } break;
        default: break;
      }

      v.low  += fcoef * v.band;
      float high = s - v.low - q * v.band;
      v.band += fcoef * high;

      mix[n] += v.low * v.env * v.vel;
    }
  }

  const float gain = clavMode ? 0.4f : 0.5f;
  for (int n = 0; n < BLOCK; n++) {
    float m = mix[n];

    if (clavMode && wahMix > 0.001f) {
      float a = fabsf(m);
      wahEnv += (a > wahEnv ? 0.01f : 0.0005f) * (a - wahEnv);
      float pos = wahPos + wahAmt * wahEnv * 6.0f;
      if (pos > 1.0f) pos = 1.0f;
      float fc = 250.0f + 2500.0f * pos * pos;
      float f = 2.0f * M_PI * fc / SAMPLE_RATE;
      const float wq = 0.35f;
      float high = m - wahLow - wq * wahBand;
      wahBand += f * high;
      wahLow  += f * wahBand;
      m = m * (1.0f - wahMix) + wahBand * wq * 1.6f * wahMix;
    }

    float x = m * gain * vol;
    if (x > 1.0f) x = 1.0f; else if (x < -1.0f) x = -1.0f;
    int16_t s = (int16_t)(x * 30000.0f);
    out[2 * n] = s;
    out[2 * n + 1] = s;
  }
}

static void audioTask(void*) {
  static int16_t buf[BLOCK * 2];
  size_t written;
  for (;;) {
    renderBlock(buf);
    i2s_write(I2S_NUM_0, buf, sizeof(buf), &written, portMAX_DELAY);
  }
}

static void i2sBegin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4;
  cfg.dma_buf_len = BLOCK;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = true;
  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);

  i2s_pin_config_t pins = {};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = PIN_I2S_BCK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = PIN_I2S_DOUT;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  i2s_set_pin(I2S_NUM_0, &pins);
}

// ---------- Rede ----------
static WebServer http(80);
static WebSocketsServer ws(81);

static void sendState(uint8_t num) {
  char msg[48];
  for (int i = 0; i < P_COUNT; i++) {
    snprintf(msg, sizeof(msg), "p:%s:%.4f", PARAM_NAMES[i], (double)params[i]);
    ws.sendTXT(num, msg);
  }
  snprintf(msg, sizeof(msg), "w:%d", (int)waveform);
  ws.sendTXT(num, msg);
  snprintf(msg, sizeof(msg), "m:%d", (int)mode);
  ws.sendTXT(num, msg);
}

static void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) { sendState(num); return; }
  if (type != WStype_TEXT) return;

  String s((const char*)payload, length);
  int a = s.indexOf(':');
  if (a < 0) return;
  String cmd = s.substring(0, a);

  if (cmd == "p") {
    int b = s.indexOf(':', a + 1);
    if (b < 0) return;
    String name = s.substring(a + 1, b);
    float val = constrain(s.substring(b + 1).toFloat(), 0.0f, 1.0f);
    for (int i = 0; i < P_COUNT; i++) {
      if (name == PARAM_NAMES[i]) { params[i] = val; break; }
    }
  } else if (cmd == "w") {
    waveform = constrain(s.substring(a + 1).toInt(), 0, 2);
  } else if (cmd == "m") {
    mode = constrain(s.substring(a + 1).toInt(), 0, 1);
  } else if (cmd == "on") {
    int b = s.indexOf(':', a + 1);
    if (b < 0) return;
    pushNote(1, constrain(s.substring(a + 1, b).toInt(), 0, 127), constrain(s.substring(b + 1).toInt(), 1, 127));
  } else if (cmd == "off") {
    pushNote(0, constrain(s.substring(a + 1).toInt(), 0, 127), 0);
  }
}

void setup() {
  Serial.begin(115200);
  noteQueue = xQueueCreate(32, sizeof(NoteEvent));

  i2sBegin();
  xTaskCreatePinnedToCore(audioTask, "audio", 8192, NULL, 5, NULL, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("Abra http://");
  Serial.println(WiFi.softAPIP());

  http.on("/", []() { http.send_P(200, "text/html; charset=utf-8", INDEX_HTML); });
  http.begin();
  ws.begin();
  ws.onEvent(onWsEvent);
}

void loop() {
  http.handleClient();
  ws.loop();
}
