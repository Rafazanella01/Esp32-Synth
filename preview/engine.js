// Motor de áudio do sintetizador (referência testável em JavaScript).
// Instrumentos: clav (Clavinet D6), organ (Hammond C3), rhodes (piano elétrico), moog (baixo/lead).
// Efeitos: wah (clav), drive tipo válvula, Leslie rotativo, tremolo estéreo (rhodes), reverb.
// O firmware (src/main.cpp) ainda NÃO tem este motor: precisa ser portado.

const SR = sampleRate;
const TWO_PI = Math.PI * 2;
const NV = 12;
const LINE_LEN = 2048;
const LINE_MASK = LINE_LEN - 1;

const ENG = { clav: 0, organ: 1, rhodes: 2, moog: 3 };

const P = {
  cutoff: 0.5, res: 0.2, attack: 0.02, decay: 0.4, sustain: 0.7, release: 0.3, volume: 0.7,
  pickup: 0.35, tone: 0.7, string: 0.35, click: 0.3, wah: 0.5, wahamt: 0.4, wahmix: 0,
  d0: 1, d1: 1, d2: 1, d3: 0, d4: 0, d5: 0, d6: 0, d7: 0, d8: 0,
  perc: 0, percharm: 0, percfast: 1, kclick: 0.3,
  tine: 0.5, bark: 0.5, rdecay: 0.5, trem: 0.3, trate: 0.4,
  osc2: 0.5, osc2mix: 0.8, sub: 0.5, fenv: 0.5, fdecay: 0.4, fsus: 0.2, glide: 0, mono: 1,
  drive: 0, leslie: 0, lfast: 0, reverb: 0.15, trim: 1
};
let engine = ENG.clav;
let sustainDown = false;
let rng = 22222;
let ageCounter = 0;

function frand() {
  rng = (Math.imul(rng, 1664525) + 1013904223) >>> 0;
  return (rng | 0) / 2147483648;
}

// ---------- tabelas e utilidades ----------
const TS = 4096;
const sinT = new Float32Array(TS + 1);
for (let i = 0; i <= TS; i++) sinT[i] = Math.sin(TWO_PI * i / TS);
function sinTurns(x) {            // x em voltas, qualquer valor real
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

// ---------- vozes ----------
const IDLE = 0, ON = 1, REL = 2;
const ORG_K = [1, 3, 2, 4, 6, 8, 10, 12, 16];   // parciais sobre a freq/2 (16', 5 1/3', 8', 4', 2 2/3', 2', 1 3/5', 1 1/3', 1')

function newVoice() {
  return {
    stage: IDLE, eng: 0, note: 0, age: 0, vel: 0, held: false,
    env: 0, ph: 0, inc: 0,
    // clav
    line: new Float32Array(LINE_LEN), wpos: 0, D: 100, g: 0.99, gRel: 0.5, lp: 0, lpc: 0.8,
    peak: 0, clickEnv: 0, samples: 0,
    // organ
    perc: 0, kclk: 0,
    // rhodes
    envM: 0, envT: 0, coefA: 0, coefM: 0, coefT: 0, i1: 0, i2: 0, tineAmp: 0,
    // moog
    f: 440, ft: 440, p1: 0, p2: 0, p3: 0, es: 0, fe: 1, y1: 0, y2: 0, y3: 0, y4: 0
  };
}
const voices = Array.from({ length: NV }, newVoice);
const held = [];           // pilha de notas seguradas (modo mono do moog)

function noteFreq(n) { return 440 * Math.pow(2, (n - 69) / 12); }

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
  const f = noteFreq(note);
  v.lpc = 0.35 + 0.65 * P.tone;
  let D = SR / f - (1 - v.lpc) / v.lpc;
  if (D < 2) D = 2;
  if (D > LINE_LEN - 4) D = LINE_LEN - 4;
  v.D = D;
  const t60 = 0.05 * Math.pow(120, P.string);
  // compensa a perda do filtro do laço e da interpolação no fundamental, para o decay ficar como pedido
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
  for (const o of voices) if (o !== v && o.stage === ON && o.eng === ENG.organ) others++;
  v.perc = (P.perc > 0.5 && others === 0) ? 1 : 0;   // percussão dispara só na 1ª nota (single trigger)
}

function startRhodes(v, note, velN) {
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

function startMoog(v, note, velN, retrigger) {
  v.ft = noteFreq(note);
  if (!retrigger) { v.f = v.ft; v.p1 = 0; v.p2 = 0.37; v.p3 = 0; v.y1 = v.y2 = v.y3 = v.y4 = 0; }
  v.env = retrigger ? v.env : 0;
  v.es = 0; v.fe = 1;
}

function voiceOn(note, vel) {
  const velN = vel / 127;
  if (engine === ENG.moog && P.mono > 0.5) {
    const i0 = held.indexOf(note);
    if (i0 >= 0) held.splice(i0, 1);
    held.push(note);
    for (const v of voices) {
      if (v.eng === ENG.moog && v.stage === ON) {
        v.note = note; v.vel = velN; v.age = ++ageCounter;
        startMoog(v, note, velN, true);
        return;
      }
    }
  }
  const v = voices[allocVoice()];
  v.eng = engine; v.note = note; v.vel = velN; v.age = ++ageCounter;
  v.stage = ON; v.held = false;
  if (engine === ENG.clav) startClav(v, note, velN);
  else if (engine === ENG.organ) startOrgan(v, note);
  else if (engine === ENG.rhodes) startRhodes(v, note, velN);
  else startMoog(v, note, velN, false);
}

function releaseVoice(v) { v.stage = REL; v.held = false; }

function voiceOff(note) {
  const hi = held.indexOf(note);
  if (hi >= 0) held.splice(hi, 1);
  for (const v of voices) {
    if (v.stage !== ON || v.note !== note) continue;
    if (v.eng === ENG.moog && P.mono > 0.5) {
      if (held.length) {                       // volta para a nota anterior ainda segura
        const n = held[held.length - 1];
        v.note = n; startMoog(v, n, v.vel, true);
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

// ---------- efeitos ----------
const wah = { low: 0, band: 0, env: 0 };
const drv = { xprev: 0, dc: 0, dcx: 0, lp1: 0, lp2: 0 };
const les = {
  hornPh: 0, drumPh: 0, hornSp: 0.8, drumSp: 0.7, lo1: 0, lo2: 0,
  hornBuf: new Float32Array(2048), drumBuf: new Float32Array(2048), w: 0
};
const trm = { ph: 0 };

function makeReverb() {
  const k = SR / 44100;
  const combLen = [1116, 1188, 1277, 1356].map(n => Math.round(n * k));
  const apLen = [556, 441].map(n => Math.round(n * k));
  return {
    comb: combLen.map(n => ({ buf: new Float32Array(n), i: 0, f: 0 })),
    ap: apLen.map(n => ({ buf: new Float32Array(n), i: 0 }))
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

function readBuf(buf, wi, delay) {
  let p = wi - delay;
  while (p < 0) p += buf.length;
  const i0 = p | 0;
  const f = p - i0;
  const a = buf[i0 % buf.length];
  const b = buf[(i0 + 1) % buf.length];
  return a + (b - a) * f;
}

function shaper(x, g, bias) { return fastTanh(g * x + bias) - fastTanh(bias); }

// ---------- renderização ----------
const gainS = { v: 0.7 };
let lwi = 0;   // índice de escrita dos buffers do Leslie

function renderBlock(outL, outR, N) {
  const cutBase = 40 * Math.pow(300, P.cutoff);
  const res4 = P.res * 3.8;
  const atkRate = 1 / (0.002 * Math.pow(1000, P.attack) * SR);
  const decC = 1 - expCoef(0.01 * Math.pow(300, P.decay));
  const relC = 1 - expCoef(0.01 * Math.pow(500, P.release));
  const sus = P.sustain;
  const fdc = expCoef(0.01 * Math.pow(300, P.fdecay));
  const fenvAmt = P.fenv;
  const fsus = P.fsus;
  const det = Math.pow(2, P.osc2 * 25 / 1200);
  const oscMix2 = P.osc2mix, subMix = P.sub;
  const glideT = P.glide < 0.01 ? 0 : 0.003 + 0.6 * P.glide * P.glide;
  const glideC = glideT === 0 ? 1 : 1 - expCoef(glideT);

  const clickAmt = P.click * 0.4;
  const pickupK = 0.05 + 0.4 * P.pickup;

  const wts = new Float32Array(9);
  for (let i = 0; i < 9; i++) {
    const lv = Math.round(P['d' + i] * 8);
    wts[i] = lv <= 0 ? 0 : Math.pow(10, -3 * (8 - lv) / 20);
  }
  const percK = P.percharm > 0.5 ? 6 : 4;
  const percCoef = expCoef(P.percfast > 0.5 ? 0.12 : 0.5);
  const clkCoef = expCoef(0.004);
  const orgAtk = 1 / (0.004 * SR), orgRel = 1 / (0.025 * SR);
  const kclick = P.kclick * 0.5;

  const mix = new Float32Array(N);

  for (const v of voices) {
    if (v.stage === IDLE) continue;
    const rel = v.stage === REL;

    if (v.eng === ENG.clav) {
      const gcur = rel ? v.gRel : v.g;
      for (let n = 0; n < N; n++) {
        let pos = v.wpos - v.D;
        let i0 = Math.floor(pos), fr = pos - i0;
        const x1 = v.line[i0 & LINE_MASK] * (1 - fr) + v.line[(i0 + 1) & LINE_MASK] * fr;
        pos = v.wpos - Math.min(v.D * (1 + pickupK), LINE_LEN - 4);
        i0 = Math.floor(pos); fr = pos - i0;
        const x2 = v.line[i0 & LINE_MASK] * (1 - fr) + v.line[(i0 + 1) & LINE_MASK] * fr;
        v.lp += v.lpc * (x1 - v.lp);
        v.line[v.wpos & LINE_MASK] = v.lp * gcur;
        v.wpos++;
        let o = (x1 - x2) * 0.6;
        if (v.clickEnv > 0.001) { o += frand() * v.clickEnv * clickAmt; v.clickEnv *= 0.97; }
        mix[n] += o;
        const a = Math.abs(o);
        v.peak = a > v.peak ? a : v.peak * 0.9995;
        v.samples++;
      }
      if (v.samples > 256 && v.peak < 0.0003) v.stage = IDLE;
      continue;
    }

    if (v.eng === ENG.organ) {
      for (let n = 0; n < N; n++) {
        v.ph += v.inc;
        if (v.ph >= 1) v.ph -= 1;
        let s = 0;
        for (let i = 0; i < 9; i++) {
          const w = wts[i];
          if (w > 0 && ORG_K[i] * v.inc < 0.42) s += w * sinTurns(v.ph * ORG_K[i]);
        }
        if (v.perc > 0.0005) {
          if (percK * v.inc * 2 < 0.42) s += v.perc * 0.9 * sinTurns(v.ph * percK);
          v.perc *= percCoef;
        }
        if (v.kclk > 0.001) { s += frand() * v.kclk * kclick; v.kclk *= clkCoef; }
        if (rel) { v.env -= orgRel; if (v.env <= 0) { v.env = 0; v.stage = IDLE; break; } }
        else if (v.env < 1) { v.env += orgAtk; if (v.env > 1) v.env = 1; }
        mix[n] += s * v.env * 0.14;
      }
      continue;
    }

    if (v.eng === ENG.rhodes) {
      const dampC = rel ? Math.pow(0.001, 1 / (0.14 * SR)) : 1;
      const amp = 0.35 + 0.65 * v.vel;
      for (let n = 0; n < N; n++) {
        v.ph += v.inc;
        if (v.ph >= 1) v.ph -= 1;
        const body = sinTurns(v.ph + v.i1 * v.envM * sinTurns(v.ph));
        const tine = v.tineAmp > 0 ? v.tineAmp * v.envT * sinTurns(v.ph + v.i2 * v.envT * sinTurns(v.ph * 14)) : 0;
        mix[n] += (body + tine) * v.env * amp * 0.5;
        v.env *= v.coefA * dampC; v.envM *= v.coefM; v.envT *= v.coefT;
      }
      if (v.env < 0.0003) v.stage = IDLE;
      continue;
    }

    // moog
    {
      const kt = Math.pow(v.ft / 261.63, 0.4);
      for (let n = 0; n < N; n++) {
        v.f += (v.ft - v.f) * glideC;
        const inc1 = v.f / SR, inc2 = inc1 * det, inc3 = inc1 * 0.5;
        v.p1 += inc1; if (v.p1 >= 1) v.p1 -= 1;
        v.p2 += inc2; if (v.p2 >= 1) v.p2 -= 1;
        v.p3 += inc3; if (v.p3 >= 1) v.p3 -= 1;
        const o1 = 2 * v.p1 - 1 - polyBlep(v.p1, inc1);
        const o2 = 2 * v.p2 - 1 - polyBlep(v.p2, inc2);
        let t3b = v.p3 + 0.5; if (t3b >= 1) t3b -= 1;
        const o3 = (v.p3 < 0.5 ? 1 : -1) + polyBlep(v.p3, inc3) - polyBlep(t3b, inc3);
        const s = 0.42 * o1 + 0.42 * oscMix2 * o2 + 0.42 * subMix * o3;

        v.fe = fsus + (v.fe - fsus) * fdc;
        let fc = cutBase * kt * Math.pow(2, fenvAmt * 5 * v.fe);
        const fmax = 0.17 * SR;
        if (fc > fmax) fc = fmax;
        const g = 1 - Math.exp(-TWO_PI * fc / SR);
        const xin = fastTanh(s - res4 * v.y4);
        v.y1 += g * (xin - v.y1);
        v.y2 += g * (v.y1 - v.y2);
        v.y3 += g * (v.y2 - v.y3);
        v.y4 += g * (v.y3 - v.y4);

        if (rel) { v.env -= v.env * relC; if (v.env < 0.0005) { v.stage = IDLE; v.env = 0; break; } }
        else if (v.es === 0) { v.env += atkRate; if (v.env >= 1) { v.env = 1; v.es = 1; } }
        else if (v.es === 1) { v.env += (sus - v.env) * decC; if (Math.abs(v.env - sus) < 0.001) v.es = 2; }
        else v.env = sus;
        mix[n] += v.y4 * v.env * (0.5 + 0.5 * v.vel) * 1.3;
      }
    }
  }

  // ----- efeitos de barramento -----
  const clavMode = engine === ENG.clav;
  const wahMix = P.wahmix;
  const drive = P.drive;
  const dg = 1 + 40 * drive * drive;
  const bias = 0.18 * drive;
  const comp = 1 / (1 + 1.6 * drive);
  const cabFc = 6500 - 3800 * drive;
  const cabC = 1 - Math.exp(-TWO_PI * cabFc / SR);
  const dcC = 1 - Math.exp(-TWO_PI * 25 / SR);

  const lw = P.leslie;
  const hornTarget = P.lfast > 0.5 ? 6.7 : 0.8;
  const drumTarget = P.lfast > 0.5 ? 6.0 : 0.7;
  const dt = N / SR;
  les.hornSp += (hornTarget - les.hornSp) * (1 - Math.exp(-dt / 0.9));
  les.drumSp += (drumTarget - les.drumSp) * (1 - Math.exp(-dt / 3.0));
  const xoC = 1 - Math.exp(-TWO_PI * 800 / SR);
  const h0 = 0.0009 * SR, hd = 0.00035 * SR, dd = 0.00015 * SR;

  const tremOn = engine === ENG.rhodes && P.trem > 0.01;
  const tremInc = (0.5 + 6.5 * P.trate) / SR;

  const rvMix = P.reverb * 0.5;
  const rvFb = 0.74 + 0.1 * P.reverb;
  const target = P.volume * P.trim;

  for (let n = 0; n < N; n++) {
    let m = mix[n];

    if (clavMode && wahMix > 0.001) {
      const a = Math.abs(m);
      wah.env += (a > wah.env ? 0.01 : 0.0005) * (a - wah.env);
      let pos = P.wah + P.wahamt * wah.env * 6;
      if (pos > 1) pos = 1;
      const fc = 250 + 2500 * pos * pos;
      const f = TWO_PI * fc / SR;
      const wq = 0.35;
      const high = m - wah.low - wq * wah.band;
      wah.band += f * high;
      wah.low += f * wah.band;
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

    let L = m, R = m;

    if (lw > 0.01) {
      les.lo1 += xoC * (m - les.lo1);
      les.lo2 += xoC * (les.lo1 - les.lo2);
      const lo = les.lo2, hi = m - lo;
      les.hornBuf[lwi] = hi;
      les.drumBuf[lwi] = lo;
      les.hornPh += les.hornSp / SR; if (les.hornPh >= 1) les.hornPh -= 1;
      les.drumPh += les.drumSp / SR; if (les.drumPh >= 1) les.drumPh -= 1;
      const hs = Math.sin(TWO_PI * les.hornPh), hc = Math.cos(TWO_PI * les.hornPh);
      const ds = Math.sin(TWO_PI * les.drumPh), dc = Math.cos(TWO_PI * les.drumPh);
      const hL = readBuf(les.hornBuf, lwi, h0 + hd * lw * hs) * (1 - 0.5 * lw * (0.5 + 0.5 * hc));
      const hR = readBuf(les.hornBuf, lwi, h0 + hd * lw * hc) * (1 - 0.5 * lw * (0.5 + 0.5 * hs));
      const dL = readBuf(les.drumBuf, lwi, h0 + dd * lw * ds) * (1 - 0.3 * lw * (0.5 + 0.5 * dc));
      const dR = readBuf(les.drumBuf, lwi, h0 + dd * lw * dc) * (1 - 0.3 * lw * (0.5 + 0.5 * ds));
      L = hL + dL; R = hR + dR;
      lwi = (lwi + 1) & 2047;
    }

    if (tremOn) {
      trm.ph += tremInc; if (trm.ph >= 1) trm.ph -= 1;
      const s = Math.sin(TWO_PI * trm.ph);
      L *= 1 - P.trem * 0.5 * (1 + s);
      R *= 1 - P.trem * 0.5 * (1 - s);
    }

    if (rvMix > 0.001) {
      const w = reverbTick(0.5 * (L + R), rvFb, 0.4) * rvMix;
      L += w; R += w;
    }

    gainS.v += (target - gainS.v) * 0.002;
    outL[n] = Math.tanh(L * gainS.v * 1.4);
    outR[n] = Math.tanh(R * gainS.v * 1.4);
  }
}

class SynthProcessor extends AudioWorkletProcessor {
  constructor() {
    super();
    this.port.onmessage = (e) => {
      const m = e.data;
      if (m.t === 'p') {
        if (m.name === 'trim') P.trim = m.v;
        else if (m.name in P) P[m.name] = Math.min(1, Math.max(0, m.v));
      } else if (m.t === 'e') {
        if (m.name in ENG) engine = ENG[m.name];
      } else if (m.t === 'on') voiceOn(m.n, m.v);
      else if (m.t === 'off') voiceOff(m.n);
      else if (m.t === 'sus') setSustain(!!m.v);
    };
  }
  process(inputs, outputs) {
    const ch = outputs[0];
    const N = ch[0].length;
    renderBlock(ch[0], ch[1] || ch[0], N);
    return true;
  }
}
registerProcessor('synth', SynthProcessor);
