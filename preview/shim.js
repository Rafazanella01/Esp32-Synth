// Troca o WebSocket do ESP32 por um motor de áudio local (AudioWorklet), para ouvir o som no PC.
// Fala o mesmo protocolo que o firmware vai falar:
//   página -> motor:  part:<0|1>  e:<engine>  p:<param>:<0..1>  mode:<0|1|2>  split:<nota>
//                     on:<nota>:<vel>  off:<nota>  tr:<semitons>  panic
//                     (e: e p: valem para a parte escolhida por part:)
//   motor -> página:  v:<nivelL>:<nivelR>  (vúmetro)   a:1 (atividade MIDI)   s:<0|1> (pedal)
// Se o navegador tiver Web MIDI (Chrome) e o CDP-160S estiver ligado por USB, o piano toca o motor direto.
(function () {
  let ctx = null, node = null, starting = null, sock = null;
  const queue = [];

  function toPage(msg) { if (sock && sock.onmessage) sock.onmessage({ data: msg }); }

  function post(m) {
    if (node) node.port.postMessage(m);
    else if (['p', 'e', 'tr', 'part', 'mode', 'split'].includes(m.t)) queue.push(m);   // notas antes de ligar o áudio são descartadas
  }

  function start() {
    if (starting) return starting;
    starting = (async () => {
      ctx = new AudioContext({ sampleRate: 44100 });
      await ctx.audioWorklet.addModule('engine.js');
      node = new AudioWorkletNode(ctx, 'synth', { outputChannelCount: [2] });
      node.port.onmessage = e => {
        if (e.data.t === 'lv') toPage('v:' + e.data.l.toFixed(4) + ':' + e.data.r.toFixed(4));
      };
      node.connect(ctx.destination);
      await ctx.resume();
      queue.splice(0).forEach(m => node.port.postMessage(m));
      toPage('i:audio');
    })();
    return starting;
  }
  document.addEventListener('pointerdown', start);
  document.addEventListener('keydown', start);

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
    }
  };

  // ----- piano por USB-MIDI (Web MIDI) -----
  function onMidi(e) {
    const [s, a, b] = e.data;
    const c = s & 0xF0;
    if (!node) { start(); return; }
    if (c === 0x90 && b > 0) { post({ t: 'on', n: a, v: b }); toPage('a:1'); }
    else if (c === 0x80 || c === 0x90) post({ t: 'off', n: a });
    else if (c === 0xB0 && a === 64) { post({ t: 'sus', v: b >= 64 }); toPage('s:' + (b >= 64 ? 1 : 0)); }
  }
  if (navigator.requestMIDIAccess) {
    navigator.requestMIDIAccess().then(acc => {
      const hook = () => {
        const names = [];
        acc.inputs.forEach(i => { i.onmidimessage = onMidi; names.push(i.name); });
        toPage('i:midi:' + names.join(', '));
      };
      hook();
      acc.onstatechange = hook;
    }).catch(() => {});
  }
})();
