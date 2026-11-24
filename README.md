# Synth CDP-160S

ESP32-S3 + DAC I2S, controlado por página web, tocado pelo Casio CDP-160S.

## Estado (importante)

| Parte | Estado |
|---|---|
| `preview/engine.js` | Motor novo (Hammond, Clavinet, Rhodes, Moog, drive, Leslie, reverb). Testado só offline e no navegador. |
| `src/ui.h` | Interface nova com 15 presets. É a fonte da página. |
| `src/main.cpp` | **Desatualizado.** Tem o motor antigo (clavinet + synth simples). O motor novo ainda precisa ser portado para C++. |
| Piano por USB host | Não feito. |
| Hardware | Nada testado no ESP32 ainda. |

## Ouvir o som agora (sem o ESP32)

```bash
cd preview && python3 -m http.server 8766 --bind 127.0.0.1
```

Abra `http://localhost:8766/index.html` e clique numa tecla da tela (liga o áudio).
No Chrome, com o CDP-160S ligado por USB no computador, o piano toca o motor direto (Web MIDI,
incluindo velocity e pedal sustain no Rhodes).

`preview/index.html` é gerado de `src/ui.h` (o trecho entre `R"HTML(` e `)HTML"` mais a linha
`<script src="shim.js">`). Se mudar a interface, gere de novo.

## Instrumentos

- **Hammond**: 9 drawbars (16', 5⅓', 8', 4', 2⅔', 2', 1⅗', 1⅓', 1'), percussão 2ª/3ª, key click, drive de válvula, Leslie com rotor lento/rápido.
- **Clavinet**: corda (Karplus-Strong), captador, click, abafador, wah manual + automático.
- **Rhodes**: FM de dois pares (corpo e tine), bark pela velocity, tremolo estéreo.
- **Moog**: 2 osciladores + sub (PolyBLEP), filtro ladder de 4 polos, envelope de filtro, glide, modo mono.

Os presets são aproximações minhas do caráter de cada instrumento, não cópias de patches.

## Ligações do DAC (PCM5102A)

| ESP32-S3 | DAC |
|---|---|
| GPIO4 | BCK |
| GPIO5 | LCK (WS) |
| GPIO6 | DIN |
| 3V3 | VIN |
| GND | GND |

SCK em GND, FMT em GND, XSMT em 3V3 (confira a placa).

## Próximas etapas

1. Ouvir e ajustar os presets (os valores foram medidos, não ouvidos).
2. Portar `engine.js` para C++ no ESP32 (usar 44,1 kHz, `fastTanh`, tabela de seno).
3. USB host MIDI (`usb_host` do ESP-IDF + driver de classe MIDI).
