# CDP-160S Synth Ensemble

ESP32-S3 + DAC I2S, controlado por página web (painel de equipamento dos anos 70), tocado pelo Casio CDP-160S.

## Estado (importante)

| Parte | Estado |
|---|---|
| `preview/engine.js` | Motor de áudio completo. Testado offline e no navegador. |
| `src/ui.h` | Interface (fonte da página): 17 presets, 8 memórias, vúmetros, rodas, efeitos. |
| `src/main.cpp` | **Desatualizado.** Motor antigo (clavinet + synth simples) e protocolo antigo. Precisa ser portado. |
| Piano por USB host | Não feito. |
| Hardware | Nada testado no ESP32 ainda. |

## Ouvir o som agora (sem o ESP32)

```bash
cd preview && python3 -m http.server 8766 --bind 127.0.0.1
```

Abra `http://localhost:8766/index.html` e toque numa tecla da tela (liga o áudio).
No Chrome, com o CDP-160S ligado por USB no computador, o piano toca o motor direto (Web MIDI:
velocity, pedal sustain no Rhodes).

`preview/index.html` é gerado de `src/ui.h` (trecho entre `R"HTML(` e `)HTML"` mais a linha
`<script src="shim.js">`). Se mudar a interface, gere de novo.

## O que tem

- **Instrumentos:** Hammond (9 drawbars, percussão, key click), Clavinet (corda, captador, wah), piano elétrico FM (tine, bark), Moog (ladder 4 polos, glide, mono).
- **Efeitos:** drive de válvula, Leslie (rotor lento/rápido), phaser, chorus, eco de fita, reverb, tremolo estéreo.
- **Performance** (o CDP-160S não tem): roda de pitch (±2 st, volta ao centro), roda de mod (vibrato), transposição ±12, curva de velocity, panic.
- **Memórias:** 8 posições suas, gravadas no navegador (`localStorage`). `GRAVAR` + `M1..M8` grava, `M1..M8` chama.
- **Vúmetros e lâmpadas:** nível L/R, link, áudio, MIDI, pedal.

Os presets são aproximações do caráter de cada instrumento, não cópias de patches.

## Protocolo página <-> motor (o firmware precisa falar o mesmo)

Página para o motor: `e:<instrumento>`, `p:<parâmetro>:<0..1>`, `on:<nota>:<vel>`, `off:<nota>`, `tr:<semitons>`, `panic`.
Motor para a página: `v:<nívelL>:<nívelR>` (vúmetro), `a:1` (atividade MIDI), `s:<0|1>` (pedal).

## Ligações do DAC (PCM5102A)

Os pinos em `src/main.cpp` (4, 5, 6) são um palpite. No repositório anterior, no commit `61711f5`,
a pinagem validada no seu circuito era BCK 27, WS 25, DOUT 26 (ESP32 clássico). Confira antes de ligar.

SCK em GND, FMT em GND, XSMT em 3V3 (confira a placa).

## Próximas etapas

1. Ouvir e ajustar os presets (os valores foram medidos, não ouvidos).
2. Portar `engine.js` para C++ no ESP32 (44,1 kHz, `fastTanh`, tabela de seno). Atenção à memória: o eco usa dois buffers de 1 s.
3. USB host MIDI (`usb_host` do ESP-IDF + driver de classe MIDI).
