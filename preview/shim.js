// Troca o WebSocket do ESP32 por um motor de áudio local (AudioWorklet), para ouvir o som no PC.
// Fala o mesmo protocolo que o firmware vai falar:
//   página -> motor:  part:<0|1>  e:<engine>  p:<param>:<0..1>  mode:<0|1|2>  split:<nota>
//                     on:<nota>:<vel>  off:<nota>  tr:<semitons>  panic
//                     (e: e p: valem para a parte escolhida por part:)
//                     sclear (cancela agendados)
//                     son:<parte|-1>:<nota>:<vel>:<ms>  soff:<parte|-1>:<nota>:<ms>  ssus:<0|1>:<ms>   (agendados, ms depois de receber)
//   motor -> página:  v:<nivelL>:<nivelR>  (vúmetro)   a:1 (atividade MIDI)   s:<0|1> (pedal)   k:<nota>:<vel> (tecla do piano, vel 0 = solta)
// Se o navegador tiver Web MIDI (Chrome) e o CDP-160S estiver ligado por USB, o piano toca o motor direto.
(function () {
  window.__previewShim = true;
  let ctx = null, node = null, starting = null, sock = null;
  const queue = [];

  function toPage(msg) { if (sock && sock.onmessage) sock.onmessage({ data: msg }); }

  function post(m) {
    if (node) node.port.postMessage(m);
    else if (['p', 'e', 'tr', 'part', 'mode', 'split', 'arp', 'tempo', 'met'].includes(m.t)) queue.push(m);   // notas antes de ligar o áudio são descartadas
  }

  function start() {
    if (starting) return starting;
    starting = (async () => {
      ctx = new AudioContext({ latencyHint: 'interactive' });   // taxa nativa da placa: sem reamostragem
      window.__ctx = ctx;
      await ctx.audioWorklet.addModule('engine.js');
      node = new AudioWorkletNode(ctx, 'synth', { outputChannelCount: [2] });
      node.port.onmessage = e => {
        if (e.data.t === 'lv') D.lvMax = Math.max(D.lvMax, e.data.l, e.data.r);
        if (e.data.t === 'lv') toPage('v:' + e.data.l.toFixed(4) + ':' + e.data.r.toFixed(4));
      };
      node.connect(ctx.destination);
      ctx.resume();
      queue.splice(0).forEach(m => node.port.postMessage(m));
      toPage('i:audio');
      const reportLat = () => toPage('i:lat:' + (((ctx.baseLatency || 0) + (ctx.outputLatency || 0)) * 1000).toFixed(1) + ':' + ctx.sampleRate);
      reportLat();
      setTimeout(reportLat, 1500);   // o navegador só estabiliza a estimativa depois de um tempo
    })();
    return starting;
  }
  function unlock() {
    if (ctx && ctx.state !== 'running') ctx.resume();
    start();
  }
  document.addEventListener('pointerdown', unlock);
  document.addEventListener('keydown', unlock);
  const bar = document.createElement('div');
  bar.style.cssText = 'position:fixed;left:0;right:0;top:0;z-index:99999;background:#b3261e;color:#fff;font:600 15px sans-serif;text-align:center;padding:10px;cursor:pointer';
  bar.textContent = 'SOM DESLIGADO — clique aqui (ou em qualquer lugar da página) uma vez para ligar o áudio';
  function watch() {
    const on = ctx && ctx.state === 'running';
    if (on && bar.parentNode) bar.remove();
    if (!on && !bar.parentNode && document.body) document.body.appendChild(bar);
  }
  const dbg = document.createElement('div');
  dbg.style.cssText = 'position:fixed;left:6px;bottom:6px;z-index:99999;background:#000c;color:#9f9;font:12px monospace;padding:6px 8px;border-radius:4px;pointer-events:none;max-width:90vw';
  const D = { midi: 'sem MIDI', last: '-', n: 0, lv: 0, lvMax: 0 };
  setInterval(() => {
    watch();
    if (!dbg.parentNode && document.body) document.body.appendChild(dbg);
    dbg.textContent = 'audio ' + (ctx ? ctx.state : 'nao iniciado') + ' | MIDI ' + D.midi + ' | msgs ' + D.n + ' ult ' + D.last + ' | nivel ' + D.lvMax.toFixed(3);
    D.lvMax *= 0.7;
  }, 400);

  window.WebSocket = class {
    constructor() {
      sock = this;
      this.readyState = 1;
      setTimeout(() => this.onopen && this.onopen(), 0);
    }
    send(s) {
      const a = s.split(':');
      if (a[0] === 'p') post({ t: 'p', name: a[1], v: parseFloat(a[2]) });
      else if (a[0] === 'e') post({ t: 'e', name: a[1] });
      else if (a[0] === 'on') post({ t: 'on', n: parseInt(a[1]), v: parseInt(a[2]) });
      else if (a[0] === 'off') post({ t: 'off', n: parseInt(a[1]) });
      else if (a[0] === 'tr') post({ t: 'tr', v: parseInt(a[1]) });
      else if (a[0] === 'part') post({ t: 'part', v: parseInt(a[1]) });
      else if (a[0] === 'mode') post({ t: 'mode', v: parseInt(a[1]) });
      else if (a[0] === 'split') post({ t: 'split', v: parseInt(a[1]) });
      else if (a[0] === 'panic') post({ t: 'panic' });
      else if (a[0] === 'arp') post({ t: 'arp', on: parseInt(a[1]), pat: parseInt(a[2]), oct: parseInt(a[3]), div: parseInt(a[4]), gate: parseFloat(a[5]), hold: parseInt(a[6]) });
      else if (a[0] === 'tempo') post({ t: 'tempo', v: parseFloat(a[1]) });
      else if (a[0] === 'met') post({ t: 'met', on: parseInt(a[1]), vol: parseFloat(a[2]) });
      else if (a[0] === 'son') post({ t: 'sq', k: 'on', part: parseInt(a[1]), n: parseInt(a[2]), v: parseInt(a[3]), ms: parseFloat(a[4]) });
      else if (a[0] === 'soff') post({ t: 'sq', k: 'off', part: parseInt(a[1]), n: parseInt(a[2]), ms: parseFloat(a[3]) });
      else if (a[0] === 'ssus') post({ t: 'sq', k: 'sus', v: parseInt(a[1]), ms: parseFloat(a[2]) });
      else if (a[0] === 'sclear') post({ t: 'sclear' });
    }
  };

  // ----- piano por USB-MIDI (Web MIDI) -----
  function onMidi(e) {
    const [s, a, b] = e.data;
    const c = s & 0xF0;
    if (s < 0xF0) { D.n++; D.last = s.toString(16) + ' ' + a + ' ' + b; }
    if (!node) return;   // o audio so liga com clique na pagina (regra do navegador)
    if (c === 0x90 && b > 0) { post({ t: 'on', n: a, v: b }); toPage('a:1'); toPage('k:' + a + ':' + b); }
    else if (c === 0x80 || c === 0x90) { post({ t: 'off', n: a }); toPage('k:' + a + ':0'); }
    else if (c === 0xB0 && a === 64) { post({ t: 'sus', v: b >= 64 }); toPage('s:' + (b >= 64 ? 1 : 0)); }
  }
  if (navigator.requestMIDIAccess) {
    navigator.requestMIDIAccess().then(acc => {
      const hook = () => {
        const names = [];
        acc.inputs.forEach(i => { i.onmidimessage = onMidi; names.push(i.name); });
        D.midi = names.join(', ') || 'nenhum';
        toPage('i:midi:' + names.join(', '));
      };
      hook();
      acc.onstatechange = hook;
    }).catch(() => {});
  }
})();
