// Troca o WebSocket do ESP32 por um motor de áudio local (AudioWorklet), para ouvir o som no PC.
// Se o navegador tiver Web MIDI (Chrome) e o CDP-160S estiver ligado por USB, o piano toca o motor direto.
(function () {
  let ctx = null, node = null, starting = null;
  const queue = [];

  function post(m) {
    if (node) node.port.postMessage(m);
    else if (m.t === 'p' || m.t === 'e') queue.push(m);   // notas antes de ligar o áudio são descartadas
  }

  function start() {
    if (starting) return starting;
    starting = (async () => {
      ctx = new AudioContext({ sampleRate: 44100 });
      await ctx.audioWorklet.addModule('engine.js');
      node = new AudioWorkletNode(ctx, 'synth', { outputChannelCount: [2] });
      node.connect(ctx.destination);
      await ctx.resume();
      queue.splice(0).forEach(m => node.port.postMessage(m));
      const st = document.getElementById('st');
      if (st) st.textContent = 'conectado · áudio ligado' + (midiName ? ' · piano: ' + midiName : '');
    })();
    return starting;
  }
  document.addEventListener('pointerdown', start);
  document.addEventListener('keydown', start);

  window.WebSocket = class {
    constructor() {
      this.readyState = 1;
      setTimeout(() => this.onopen && this.onopen(), 0);
    }
    send(s) {
      const a = s.split(':');
      if (a[0] === 'p') post({ t: 'p', name: a[1], v: parseFloat(a[2]) });
      else if (a[0] === 'e') post({ t: 'e', name: a[1] });
      else if (a[0] === 'on') post({ t: 'on', n: parseInt(a[1]), v: parseInt(a[2]) });
      else if (a[0] === 'off') post({ t: 'off', n: parseInt(a[1]) });
    }
  };

  // ----- piano por USB-MIDI (Web MIDI) -----
  let midiName = '';
  function onMidi(e) {
    const [s, a, b] = e.data;
    const c = s & 0xF0;
    if (!node) { start(); return; }
    if (c === 0x90 && b > 0) post({ t: 'on', n: a, v: b });
    else if (c === 0x80 || c === 0x90) post({ t: 'off', n: a });
    else if (c === 0xB0 && a === 64) post({ t: 'sus', v: b >= 64 });
  }
  if (navigator.requestMIDIAccess) {
    navigator.requestMIDIAccess().then(acc => {
      const hook = () => {
        const names = [];
        acc.inputs.forEach(i => { i.onmidimessage = onMidi; names.push(i.name); });
        midiName = names.join(', ');
        const st = document.getElementById('st');
        if (st && midiName) st.textContent = 'conectado · piano: ' + midiName + (node ? ' · áudio ligado' : ' · toque numa tecla da tela para ligar o áudio');
      };
      hook();
      acc.onstatechange = hook;
    }).catch(() => {});
  }
})();
