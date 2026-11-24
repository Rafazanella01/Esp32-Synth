// Motor de áudio do sintetizador (referência testável em JavaScript).
// Duas partes (A e B), cada uma com instrumento, parâmetros e efeitos próprios.
// Modos de teclado: 0 normal (só A), 1 camada (A + B juntas), 2 dividido (A abaixo do ponto, B a partir dele).
// Instrumentos: clav (Clavinet), organ (órgão de tonewheels), rhodes (piano elétrico), moog (baixo/lead).
// Efeitos por parte: wah (clav), drive, Leslie, phaser, tremolo (rhodes), chorus, eco. Reverb é compartilhado.
// O firmware (src/main.cpp) ainda NÃO tem este motor: precisa ser portado.

const SR = sampleRate;
const TWO_PI = Math.PI * 2;
const NV = 16;
const LINE_LEN = 2048;
const LINE_MASK = LINE_LEN - 1;
const MAXN = 128;

const ENG = { clav: 0, organ: 1, rhodes: 2, moog: 3 };

const PDEF = {
  cutoff: 0.5, res: 0.2, attack: 0.02, decay: 0.4, sustain: 0.7, release: 0.3, volume: 0.7,
  pickup: 0.35, tone: 0.7, string: 0.35, click: 0.3, wah: 0.5, wahamt: 0.4, wahmix: 0,
  d0: 1, d1: 1, d2: 1, d3: 0, d4: 0, d5: 0, d6: 0, d7: 0, d8: 0,
  perc: 0, percharm: 0, percfast: 1, kclick: 0.3,
  tine: 0.5, bark: 0.5, rdecay: 0.5, trem: 0.3, trate: 0.4,
  osc2: 0.5, osc2mix: 0.8, sub: 0.5, fenv: 0.5, fdecay: 0.4, fsus: 0.2, glide: 0, mono: 1,
  drive: 0, leslie: 0, lfast: 0, reverb: 0.15, trim: 1,
  phaser: 0, phrate: 0.3, chorus: 0, delay: 0, dtime: 0.4, dfb: 0.3
};
const G = { bend: 0.5, mod: 0, vcurve: 0.5 };      // globais de performance
const GLOBAL_NAMES = new Set(['bend', 'mod', 'vcurve']);

let editPart = 0;
let mode = 0;
let splitNote = 60;
let transpose = 0;
let sustainDown = false;
let vphase = 0;
let rng = 22222;
let ageCounter = 0;
const tmap = new Map();

function frand() {
  rng = (Math.imul(rng, 1664525) + 1013904223) >>> 0;
  return (rng | 0) / 2147483648;
}

// ---------- tabelas e utilidades ----------
const TS = 4096;
const sinT = new Float32Array(TS + 1);
for (let i = 0; i <= TS; i++) sinT[i] = Math.sin(TWO_PI * i / TS);
function sinTurns(x) {
  x -= Math.floor(x);
  const p = x * TS;
  const i = p | 0;
  const f = p - i;
  return sinT[i] + (sinT[i + 1] - sinT[i]) * f;
}
function fastTanh(x) {
  if (x < -3) return -1;
  if (x > 3) return 1;
  const x2 = x * x;
  return x * (27 + x2) / (27 + 9 * x2);
}
function polyBlep(t, dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1; }
  if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
  return 0;
}
function expCoef(t) { return Math.exp(-1 / (t * SR)); }
function noteFreq(n) { return 440 * Math.pow(2, (n - 69) / 12); }
function readBuf(buf, wi, delay) {
  let p = wi - delay;
  while (p < 0) p += buf.length;
  const i0 = p | 0;
  const f = p - i0;
  return buf[i0 % buf.length] + (buf[(i0 + 1) % buf.length] - buf[i0 % buf.length]) * f;
}
function shaper(x, g, bias) { return fastTanh(g * x + bias) - fastTanh(bias); }

// ---------- partes e efeitos por parte ----------
function makeFx() {
  return {
    wah: { low: 0, band: 0, env: 0 },
    drv: { xprev: 0, dc: 0, lp1: 0, lp2: 0 },
    les: { hornPh: 0, drumPh: 0, hornSp: 0.8, drumSp: 0.7, lo1: 0, lo2: 0, wi: 0,
           hornBuf: new Float32Array(2048), drumBuf: new Float32Array(2048) },
    trm: { ph: 0 },
    phs: { z: [0, 0, 0, 0], y: 0, ph: 0 },
    cho: { l: new Float32Array(4096), r: new Float32Array(4096), i: 0, ph: 0 },
    ech: { l: new Float32Array(Math.ceil(1.0 * SR)), r: new Float32Array(Math.ceil(1.0 * SR)), i: 0, dt: 0.3 * SR, lpl: 0, lpr: 0, ph: 0 }
  };
}
function newPart() {
  return { P: Object.assign({}, PDEF), engine: ENG.clav, fx: makeFx(), gain: 0.7, tail: 0,
           D: { wts: new Float32Array(9) }, L: new Float32Array(MAXN), R: new Float32Array(MAXN), bus: new Float32Array(MAXN) };
}
const parts = [newPart(), newPart()];
const held = [[], []];     // pilha de notas seguradas por parte (modo mono do moog)

// ---------- reverb compartilhado ----------
function makeReverb() {
  const k = SR / 44100;
  return {
    comb: [1116, 1188, 1277, 1356].map(n => ({ buf: new Float32Array(Math.round(n * k)), i: 0, f: 0 })),
    ap: [556, 441].map(n => ({ buf: new Float32Array(Math.round(n * k)), i: 0 }))
  };
}
const rv = makeReverb();
function reverbTick(inp, fb, damp) {
  let sum = 0;
  const x = inp * 0.03;
  for (const c of rv.comb) {
    const o = c.buf[c.i];
    c.f = o * (1 - damp) + c.f * damp;
    c.buf[c.i] = x + c.f * fb;
    if (++c.i >= c.buf.length) c.i = 0;
    sum += o;
  }
  for (const a of rv.ap) {
    const b = a.buf[a.i];
    const out = -sum + b;
    a.buf[a.i] = sum + b * 0.5;
    if (++a.i >= a.buf.length) a.i = 0;
    sum = out;
  }
  return sum;
}

// ---------- vozes ----------
const IDLE = 0, ON = 1, REL = 2;
const ORG_K = [1, 3, 2, 4, 6, 8, 10, 12, 16];

function newVoice() {
  return {
    stage: IDLE, part: 0, pp: null, eng: 0, note: 0, age: 0, vel: 0, held: false,
    env: 0, ph: 0, inc: 0,
    line: new Float32Array(LINE_LEN), wpos: 0, D: 100, g: 0.99, gRel: 0.5, lp: 0, lpc: 0.8,
    peak: 0, clickEnv: 0, samples: 0,
    perc: 0, kclk: 0,
    envM: 0, envT: 0, coefA: 0, coefM: 0, coefT: 0, i1: 0, i2: 0, tineAmp: 0,
    f: 440, ft: 440, p1: 0, p2: 0, p3: 0, es: 0, fe: 1, y1: 0, y2: 0, y3: 0, y4: 0
  };
}
const voices = Array.from({ length: NV }, newVoice);

function allocVoice() {
  let pick = -1, oldest = Infinity;
  for (let i = 0; i < NV; i++) if (voices[i].stage === IDLE) return i;
  for (let i = 0; i < NV; i++)
    if (voices[i].stage === REL && voices[i].age < oldest) { oldest = voices[i].age; pick = i; }
  if (pick >= 0) return pick;
  for (let i = 0; i < NV; i++)
    if (voices[i].age < oldest) { oldest = voices[i].age; pick = i; }
  return pick;
}

function startClav(v, note, velN) {
  const P = v.pp;
  const f = noteFreq(note);
  v.lpc = 0.35 + 0.65 * P.tone;
  let D = SR / f - (1 - v.lpc) / v.lpc;
  if (D < 2) D = 2;
  if (D > LINE_LEN - 4) D = LINE_LEN - 4;
  v.D = D;
  const t60 = 0.05 * Math.pow(120, P.string);
  const w0 = TWO_PI * f / SR;
  const lpMag = v.lpc / Math.sqrt(1 + (1 - v.lpc) * (1 - v.lpc) - 2 * (1 - v.lpc) * Math.cos(w0));
  const fr0 = Math.ceil(D) - D;
  const intMag = Math.sqrt((1 - fr0) * (1 - fr0) + fr0 * fr0 + 2 * fr0 * (1 - fr0) * Math.cos(w0));
  v.g = Math.min(0.99995, Math.pow(0.001, D / (SR * t60)) / (lpMag * intMag));
  v.gRel = Math.pow(0.001, D / (SR * 0.025));
  v.line.fill(0);
  const n = Math.ceil(D) + 2;
  const c = 0.2 + 0.8 * velN;
  const amp = 0.3 + 0.7 * velN;
  let lpn = 0, mean = 0;
  for (let i = 0; i < n; i++) { lpn += c * (frand() - lpn); v.line[i] = lpn; mean += lpn; }
  mean /= n;
  for (let i = 0; i < n; i++) v.line[i] = (v.line[i] - mean) * amp * 2.5;
  v.wpos = n; v.lp = 0; v.peak = 1; v.clickEnv = velN; v.samples = 0;
}

function startOrgan(v, note) {
  v.inc = noteFreq(note) / 2 / SR;
  v.ph = 0; v.env = 0;
  v.kclk = 1;
  let others = 0;
  for (const o of voices) if (o !== v && o.part === v.part && o.stage === ON && o.eng === ENG.organ) others++;
  v.perc = (v.pp.perc > 0.5 && others === 0) ? 1 : 0;
}

function startRhodes(v, note, velN) {
  const P = v.pp;
  const f = noteFreq(note);
  v.inc = f / SR;
  v.ph = 0; v.env = 1; v.envM = 1; v.envT = 1;
  const t60 = Math.pow(8, P.rdecay) * Math.pow(2, -(note - 60) / 24);
  v.coefA = Math.pow(0.001, 1 / (t60 * SR));
  v.coefM = Math.pow(v.coefA, 2.5);
  v.coefT = Math.pow(0.001, 1 / (0.18 * SR));
  v.i1 = 0.25 + (0.4 + 3.0 * P.bark) * velN * velN;
  v.i2 = 0.6 + 2.4 * velN * velN;
  const lim = 0.42 * SR / (14 * f);
  v.tineAmp = P.tine * 0.35 * Math.min(1, Math.max(0, (lim - 1) * 2));
}

function startMoog(v, note, retrigger) {
  v.ft = noteFreq(note);
  if (!retrigger) { v.f = v.ft; v.p1 = 0; v.p2 = 0.37; v.p3 = 0; v.y1 = v.y2 = v.y3 = v.y4 = 0; }
  v.env = retrigger ? v.env : 0;
  v.es = 0; v.fe = 1;
}

function voiceOn(pi, note, vel) {
  const part = parts[pi], P = part.P, engine = part.engine;
  const velN = Math.pow(vel / 127, Math.pow(2, (0.5 - G.vcurve) * 2));
  if (engine === ENG.moog && P.mono > 0.5) {
    const h = held[pi];
    const i0 = h.indexOf(note);
    if (i0 >= 0) h.splice(i0, 1);
    h.push(note);
    for (const v of voices) {
      if (v.part === pi && v.eng === ENG.moog && v.stage === ON) {
        v.note = note; v.vel = velN; v.age = ++ageCounter;
        startMoog(v, note, true);
        return;
      }
    }
  }
  const v = voices[allocVoice()];
  v.part = pi; v.pp = P; v.eng = engine; v.note = note; v.vel = velN; v.age = ++ageCounter;
  v.stage = ON; v.held = false;
  if (engine === ENG.clav) startClav(v, note, velN);
  else if (engine === ENG.organ) startOrgan(v, note);
  else if (engine === ENG.rhodes) startRhodes(v, note, velN);
  else startMoog(v, note, false);
}

function releaseVoice(v) { v.stage = REL; v.held = false; }

function voiceOff(note) {
  for (const h of held) { const i = h.indexOf(note); if (i >= 0) h.splice(i, 1); }
  for (const v of voices) {
    if (v.stage !== ON || v.note !== note) continue;
    if (v.eng === ENG.moog && v.pp.mono > 0.5) {
      const h = held[v.part];
      if (h.length) {
        const n = h[h.length - 1];
        v.note = n; startMoog(v, n, true);
        return;
      }
    }
    if (v.eng === ENG.rhodes && sustainDown) { v.held = true; continue; }
    releaseVoice(v);
  }
}

function setSustain(down) {
  sustainDown = down;
  if (!down) for (const v of voices) if (v.stage === ON && v.held) releaseVoice(v);
}

function routeOn(n, vel, key) {
  if (mode === 0) voiceOn(0, n, vel);
  else if (mode === 1) { voiceOn(0, n, vel); voiceOn(1, n, vel); }
  else voiceOn(key < splitNote ? 0 : 1, n, vel);
}

// ---------- constantes por bloco ----------
function derive(part) {
  const P = part.P, D = part.D;
  D.cutBase = 40 * Math.pow(300, P.cutoff);
  D.res4 = P.res * 3.8;
  D.atkRate = 1 / (0.002 * Math.pow(1000, P.attack) * SR);
  D.decC = 1 - expCoef(0.01 * Math.pow(300, P.decay));
  D.relC = 1 - expCoef(0.01 * Math.pow(500, P.release));
  D.sus = P.sustain;
  D.fdc = expCoef(0.01 * Math.pow(300, P.fdecay));
  D.fenvAmt = P.fenv;
  D.fsus = P.fsus;
  D.det = Math.pow(2, P.osc2 * 25 / 1200);
  D.osc2mix = P.osc2mix;
  D.subMix = P.sub;
  const glideT = P.glide < 0.01 ? 0 : 0.003 + 0.6 * P.glide * P.glide;
  D.glideC = glideT === 0 ? 1 : 1 - expCoef(glideT);
  D.clickAmt = P.click * 0.4;
  D.pickupK = 0.05 + 0.4 * P.pickup;
  for (let i = 0; i < 9; i++) {
    const lv = Math.round(P['d' + i] * 8);
    D.wts[i] = lv <= 0 ? 0 : Math.pow(10, -3 * (8 - lv) / 20);
  }
  D.percK = P.percharm > 0.5 ? 6 : 4;
  D.percCoef = expCoef(P.percfast > 0.5 ? 0.12 : 0.5);
  D.kclick = P.kclick * 0.5;
}

const clkCoef = expCoef(0.004);
const orgAtk = 1 / (0.004 * SR), orgRel = 1 / (0.025 * SR);

function renderVoice(v, mix, D, N, pb) {
  const rel = v.stage === REL;

  if (v.eng === ENG.clav) {
    const gcur = rel ? v.gRel : v.g;
    for (let n = 0; n < N; n++) {
      let pos = v.wpos - v.D;
      let i0 = Math.floor(pos), fr = pos - i0;
      const x1 = v.line[i0 & LINE_MASK] * (1 - fr) + v.line[(i0 + 1) & LINE_MASK] * fr;
      pos = v.wpos - Math.min(v.D * (1 + D.pickupK), LINE_LEN - 4);
      i0 = Math.floor(pos); fr = pos - i0;
      const x2 = v.line[i0 & LINE_MASK] * (1 - fr) + v.line[(i0 + 1) & LINE_MASK] * fr;
      v.lp += v.lpc * (x1 - v.lp);
      v.line[v.wpos & LINE_MASK] = v.lp * gcur;
      v.wpos++;
      let o = (x1 - x2) * 0.6;
      if (v.clickEnv > 0.001) { o += frand() * v.clickEnv * D.clickAmt; v.clickEnv *= 0.97; }
      mix[n] += o;
      const a = Math.abs(o);
      v.peak = a > v.peak ? a : v.peak * 0.9995;
      v.samples++;
    }
    if (v.samples > 256 && v.peak < 0.0003) v.stage = IDLE;
    return;
  }

  if (v.eng === ENG.organ) {
    const oinc = v.inc * pb;
    for (let n = 0; n < N; n++) {
      v.ph += oinc;
      if (v.ph >= 1) v.ph -= 1;
      let s = 0;
      for (let i = 0; i < 9; i++) {
        const w = D.wts[i];
        if (w > 0 && ORG_K[i] * oinc < 0.42) s += w * sinTurns(v.ph * ORG_K[i]);
      }
      if (v.perc > 0.0005) {
        if (D.percK * oinc * 2 < 0.42) s += v.perc * 0.9 * sinTurns(v.ph * D.percK);
        v.perc *= D.percCoef;
      }
      if (v.kclk > 0.001) { s += frand() * v.kclk * D.kclick; v.kclk *= clkCoef; }
      if (rel) { v.env -= orgRel; if (v.env <= 0) { v.env = 0; v.stage = IDLE; break; } }
      else if (v.env < 1) { v.env += orgAtk; if (v.env > 1) v.env = 1; }
      mix[n] += s * v.env * 0.14;
    }
    return;
  }

  if (v.eng === ENG.rhodes) {
    const dampC = rel ? Math.pow(0.001, 1 / (0.14 * SR)) : 1;
    const amp = 0.35 + 0.65 * v.vel;
    const rinc = v.inc * pb;
    for (let n = 0; n < N; n++) {
      v.ph += rinc;
      if (v.ph >= 1) v.ph -= 1;
      const body = sinTurns(v.ph + v.i1 * v.envM * sinTurns(v.ph));
      const tine = v.tineAmp > 0 ? v.tineAmp * v.envT * sinTurns(v.ph + v.i2 * v.envT * sinTurns(v.ph * 14)) : 0;
      mix[n] += (body + tine) * v.env * amp * 0.5;
      v.env *= v.coefA * dampC; v.envM *= v.coefM; v.envT *= v.coefT;
    }
    if (v.env < 0.0003) v.stage = IDLE;
    return;
  }

  // moog
  const kt = Math.pow(v.ft / 261.63, 0.4);
  for (let n = 0; n < N; n++) {
    v.f += (v.ft - v.f) * D.glideC;
    const inc1 = v.f * pb / SR, inc2 = inc1 * D.det, inc3 = inc1 * 0.5;
    v.p1 += inc1; if (v.p1 >= 1) v.p1 -= 1;
    v.p2 += inc2; if (v.p2 >= 1) v.p2 -= 1;
    v.p3 += inc3; if (v.p3 >= 1) v.p3 -= 1;
    const o1 = 2 * v.p1 - 1 - polyBlep(v.p1, inc1);
    const o2 = 2 * v.p2 - 1 - polyBlep(v.p2, inc2);
    let t3b = v.p3 + 0.5; if (t3b >= 1) t3b -= 1;
    const o3 = (v.p3 < 0.5 ? 1 : -1) + polyBlep(v.p3, inc3) - polyBlep(t3b, inc3);
    const s = 0.42 * o1 + 0.42 * D.osc2mix * o2 + 0.42 * D.subMix * o3;

    v.fe = D.fsus + (v.fe - D.fsus) * D.fdc;
    let fc = D.cutBase * kt * Math.pow(2, D.fenvAmt * 5 * v.fe);
    const fmax = 0.17 * SR;
    if (fc > fmax) fc = fmax;
    const g = 1 - Math.exp(-TWO_PI * fc / SR);
    const xin = fastTanh(s - D.res4 * v.y4);
    v.y1 += g * (xin - v.y1);
    v.y2 += g * (v.y1 - v.y2);
    v.y3 += g * (v.y2 - v.y3);
    v.y4 += g * (v.y3 - v.y4);

    if (rel) { v.env -= v.env * D.relC; if (v.env < 0.0005) { v.stage = IDLE; v.env = 0; break; } }
    else if (v.es === 0) { v.env += D.atkRate; if (v.env >= 1) { v.env = 1; v.es = 1; } }
    else if (v.es === 1) { v.env += (D.sus - v.env) * D.decC; if (Math.abs(v.env - D.sus) < 0.001) v.es = 2; }
    else v.env = D.sus;
    mix[n] += v.y4 * v.env * (0.5 + 0.5 * v.vel) * 1.3;
  }
}

// ---------- efeitos de uma parte (sem reverb) ----------
function fxPart(part, N) {
  const P = part.P, f = part.fx, mix = part.bus, outL = part.L, outR = part.R;
  const clavMode = part.engine === ENG.clav;
  const wahMix = P.wahmix;
  const drive = P.drive;
  const dg = 1 + 40 * drive * drive;
  const bias = 0.18 * drive;
  const comp = 1 / (1 + 1.6 * drive);
  const cabFc = 6500 - 3800 * drive;
  const cabC = 1 - Math.exp(-TWO_PI * cabFc / SR);
  const dcC = 1 - Math.exp(-TWO_PI * 25 / SR);

  const lw = P.leslie, les = f.les;
  const hornTarget = P.lfast > 0.5 ? 6.7 : 0.8;
  const drumTarget = P.lfast > 0.5 ? 6.0 : 0.7;
  const dt = N / SR;
  les.hornSp += (hornTarget - les.hornSp) * (1 - Math.exp(-dt / 0.9));
  les.drumSp += (drumTarget - les.drumSp) * (1 - Math.exp(-dt / 3.0));
  const xoC = 1 - Math.exp(-TWO_PI * 800 / SR);
  const h0 = 0.0009 * SR, hd = 0.00035 * SR, dd = 0.00015 * SR;

  const tremOn = part.engine === ENG.rhodes && P.trem > 0.01;
  const tremInc = (0.5 + 6.5 * P.trate) / SR;

  const phMix = P.phaser, phs = f.phs;
  phs.ph += (0.1 + 5 * P.phrate) * N / SR; if (phs.ph >= 1) phs.ph -= 1;
  const phTan = Math.tan(Math.PI * 300 * Math.pow(10, 0.5 + 0.5 * Math.sin(TWO_PI * phs.ph)) / SR);
  const phA = (1 - phTan) / (1 + phTan);

  const chMix = P.chorus, cho = f.cho;
  const chInc = 0.7 / SR;
  const chBase = 0.014 * SR, chDepth = 0.004 * SR;

  const ech = f.ech;
  const ecMix = P.delay * 0.7;
  const ecFb = P.dfb * 0.85;
  const ecTarget = 0.08 * Math.pow(8.75, P.dtime) * SR;
  const ecLpC = 1 - Math.exp(-TWO_PI * 3500 / SR);
  const ecLen = ech.l.length;

  const wah = f.wah, drv = f.drv, trm = f.trm;

  for (let n = 0; n < N; n++) {
    let m = mix[n];

    if (clavMode && wahMix > 0.001) {
      const a = Math.abs(m);
      wah.env += (a > wah.env ? 0.01 : 0.0005) * (a - wah.env);
      let pos = P.wah + P.wahamt * wah.env * 6;
      if (pos > 1) pos = 1;
      const fc = 250 + 2500 * pos * pos;
      const ff = TWO_PI * fc / SR;
      const wq = 0.35;
      const high = m - wah.low - wq * wah.band;
      wah.band += ff * high;
      wah.low += ff * wah.band;
      m = m * (1 - wahMix) + wah.band * wq * 1.6 * wahMix;
    }

    if (drive > 0.001) {
      const mid = 0.5 * (drv.xprev + m);
      let y = 0.5 * (shaper(mid, dg, bias) + shaper(m, dg, bias));
      drv.xprev = m;
      drv.dc += dcC * (y - drv.dc);
      y -= drv.dc;
      drv.lp1 += cabC * (y - drv.lp1);
      drv.lp2 += cabC * (drv.lp1 - drv.lp2);
      m = drv.lp2 * comp;
    }

    if (phMix > 0.01) {
      let x = m + 0.55 * phs.y;
      for (let i = 0; i < 4; i++) {
        const y = phA * x + phs.z[i];
        phs.z[i] = x - phA * y;
        x = y;
      }
      phs.y = x;
      m = m * (1 - 0.5 * phMix) + x * 0.5 * phMix;
    }

    let L = m, R = m;

    if (lw > 0.01) {
      les.lo1 += xoC * (m - les.lo1);
      les.lo2 += xoC * (les.lo1 - les.lo2);
      const lo = les.lo2, hi = m - lo;
      les.hornBuf[les.wi] = hi;
      les.drumBuf[les.wi] = lo;
      les.hornPh += les.hornSp / SR; if (les.hornPh >= 1) les.hornPh -= 1;
      les.drumPh += les.drumSp / SR; if (les.drumPh >= 1) les.drumPh -= 1;
      const hs = Math.sin(TWO_PI * les.hornPh), hc = Math.cos(TWO_PI * les.hornPh);
      const ds = Math.sin(TWO_PI * les.drumPh), dc = Math.cos(TWO_PI * les.drumPh);
      const hL = readBuf(les.hornBuf, les.wi, h0 + hd * lw * hs) * (1 - 0.5 * lw * (0.5 + 0.5 * hc));
      const hR = readBuf(les.hornBuf, les.wi, h0 + hd * lw * hc) * (1 - 0.5 * lw * (0.5 + 0.5 * hs));
      const dL = readBuf(les.drumBuf, les.wi, h0 + dd * lw * ds) * (1 - 0.3 * lw * (0.5 + 0.5 * dc));
      const dR = readBuf(les.drumBuf, les.wi, h0 + dd * lw * dc) * (1 - 0.3 * lw * (0.5 + 0.5 * ds));
      L = hL + dL; R = hR + dR;
      les.wi = (les.wi + 1) & 2047;
    }

    if (tremOn) {
      trm.ph += tremInc; if (trm.ph >= 1) trm.ph -= 1;
      const s = Math.sin(TWO_PI * trm.ph);
      L *= 1 - P.trem * 0.5 * (1 + s);
      R *= 1 - P.trem * 0.5 * (1 - s);
    }

    if (chMix > 0.01) {
      cho.ph += chInc; if (cho.ph >= 1) cho.ph -= 1;
      cho.l[cho.i] = L; cho.r[cho.i] = R;
      const dl = readBuf(cho.l, cho.i, chBase + chDepth * Math.sin(TWO_PI * cho.ph));
      const dr = readBuf(cho.r, cho.i, chBase + chDepth * Math.sin(TWO_PI * cho.ph + 2.4));
      cho.i = (cho.i + 1) & 4095;
      L = L * (1 - 0.35 * chMix) + 0.75 * chMix * dl;
      R = R * (1 - 0.35 * chMix) + 0.75 * chMix * dr;
    }

    if (ecMix > 0.005) {
      ech.dt += (ecTarget - ech.dt) * 0.0004;
      ech.ph += 0.6 / SR; if (ech.ph >= 1) ech.ph -= 1;
      const wow = 1 + 0.0025 * Math.sin(TWO_PI * ech.ph);
      const inp = 0.5 * (L + R);
      const eL = readBuf(ech.l, ech.i, ech.dt * wow);
      const eR = readBuf(ech.r, ech.i, ech.dt * wow * 1.12);
      ech.lpl += ecLpC * (eL - ech.lpl);
      ech.lpr += ecLpC * (eR - ech.lpr);
      ech.l[ech.i] = inp + ech.lpr * ecFb;
      ech.r[ech.i] = inp + ech.lpl * ecFb;
      ech.i = (ech.i + 1) % ecLen;
      L += eL * ecMix; R += eR * ecMix;
    }

    outL[n] = L; outR[n] = R;
  }
}

// ---------- renderização ----------
function renderBlock(outL, outR, N) {
  vphase += 5.5 * N / SR; if (vphase >= 1) vphase -= 1;
  const pb = Math.pow(2, ((G.bend - 0.5) * 4 + G.mod * 0.35 * Math.sin(TWO_PI * vphase)) / 12);

  for (const p of parts) { p.bus.fill(0, 0, N); derive(p); }
  const act = [false, false];
  for (const v of voices) {
    if (v.stage === IDLE) continue;
    act[v.part] = true;
    renderVoice(v, parts[v.part].bus, parts[v.part].D, N, pb);
  }

  const enabled = [true, mode !== 0];
  let rvMax = 0;
  for (let i = 0; i < 2; i++) {
    const p = parts[i];
    if (enabled[i] || act[i]) p.tail = 5 * SR; else p.tail -= N;
    if (p.tail > 0) { fxPart(p, N); if (p.P.reverb > rvMax) rvMax = p.P.reverb; }
  }
  const live = [parts[0].tail > 0, parts[1].tail > 0];
  const rvFb = 0.74 + 0.1 * rvMax;

  let sumL = 0, sumR = 0;
  for (let n = 0; n < N; n++) {
    let L = 0, R = 0, rvIn = 0;
    for (let i = 0; i < 2; i++) {
      if (!live[i]) continue;
      const p = parts[i];
      p.gain += (p.P.volume * p.P.trim - p.gain) * 0.002;
      const l = p.L[n] * p.gain, r = p.R[n] * p.gain;
      L += l; R += r;
      rvIn += p.P.reverb * 0.5 * (l + r);
    }
    if (rvMax > 0.001) {
      const w = reverbTick(rvIn, rvFb, 0.4) * 0.5;
      L += w; R += w;
    }
    const oL = Math.tanh(L * 1.4), oR = Math.tanh(R * 1.4);
    outL[n] = oL; outR[n] = oR;
    sumL += oL * oL; sumR += oR * oR;
  }
  return [Math.sqrt(sumL / N), Math.sqrt(sumR / N)];
}

class SynthProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.port.onmessage = (e) => {
      const m = e.data;
      if (m.t === 'p') {
        if (GLOBAL_NAMES.has(m.name)) G[m.name] = Math.min(1, Math.max(0, m.v));
        else {
          const P = parts[editPart].P;
          if (m.name === 'trim') P.trim = m.v;
          else if (m.name in P) P[m.name] = Math.min(1, Math.max(0, m.v));
        }
      } else if (m.t === 'e') {
        if (m.name in ENG) parts[editPart].engine = ENG[m.name];
      } else if (m.t === 'part') editPart = m.v ? 1 : 0;
      else if (m.t === 'mode') mode = Math.max(0, Math.min(2, m.v | 0));
      else if (m.t === 'split') splitNote = Math.max(0, Math.min(127, m.v | 0));
      else if (m.t === 'on') {
        const n = Math.min(127, Math.max(0, m.n + transpose));
        tmap.set(m.n, n);
        routeOn(n, m.v, m.n);
      } else if (m.t === 'off') {
        const n = tmap.has(m.n) ? tmap.get(m.n) : m.n;
        tmap.delete(m.n);
        voiceOff(n);
      } else if (m.t === 'sus') setSustain(!!m.v);
      else if (m.t === 'tr') transpose = Math.max(-24, Math.min(24, m.v | 0));
      else if (m.t === 'panic') {
        for (const v of voices) { v.stage = IDLE; v.held = false; }
        held[0].length = 0; held[1].length = 0; tmap.clear(); setSustain(false);
      }
    };
    this.blocks = 0;
  }
  process(inputs, outputs) {
    const ch = outputs[0];
    const N = ch[0].length;
    const lv = renderBlock(ch[0], ch[1] || ch[0], N);
    if (++this.blocks % 4 === 0) this.port.postMessage({ t: 'lv', l: lv[0], r: lv[1] });
    return true;
  }
}
registerProcessor('synth', SynthProcessor);
