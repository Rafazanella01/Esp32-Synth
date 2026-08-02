# ESP32 Synth

Sintetizador de acordes com arpejador, baseado em ESP32 + saída de áudio I2S
(PCM5102A) + display LCD + matriz de botões + joystick + potenciômetros.

As 16 teclas da matriz são notas (C4 a D#5). **Segurar** uma tecla toca o
acorde em bloco, sustentado; **tocar rápido** dispara o arpejo, que
continua sozinho. O **joystick** escolhe qual acorde, ao vivo — dá pra
mexer nele com o som já rolando.

| | Joystick no centro | Joystick numa direção |
|---|---|---|
| **Segurando a tecla** | nota pura, sustentada | aquele acorde em bloco |
| **Toque rápido** | acorde Maior arpejado | aquele acorde arpejado |

## O que o firmware faz

- 10 qualidades de acorde nas posições do joystick — centro, as 8 direções
  (com diagonais) e o clique: Maior, Maior7, Dominante7, Aumentado, Sus4,
  Menor7, Menor, Diminuto, Sus2 e Sexta
- Dois modos de toque sobre o mesmo acorde: bloco sustentado ou arpejado
- 4 formas de onda com **morph contínuo** entre elas (seno, triangular,
  dente-de-serra, quadrada), num potenciômetro
- **Glide** (portamento) entre as notas, pra transição fluida
- Controle de andamento (BPM) por potenciômetro
- Potenciômetros opcionais pra quantidade de glide e pro gate do arpejo
  (staccato ↔ legato)
- Display LCD 16x2 mostrando nota, acorde, timbre e BPM

## Documentação

| Doc | Conteúdo |
|---|---|
| [`docs/01-ideia.md`](docs/01-ideia.md) | A ideia original do projeto e as decisões tomadas pra chegar no design atual |
| [`docs/02-arquitetura.md`](docs/02-arquitetura.md) | Como hardware e firmware se encaixam, e por quê |
| [`docs/03-hardware.md`](docs/03-hardware.md) | Lista de peças, mapa completo de pinos, circuito de saída de áudio (LM358) |
| [`docs/04-firmware.md`](docs/04-firmware.md) | Referência da API de cada módulo do firmware |
| [`docs/05-montagem.md`](docs/05-montagem.md) | Guia de montagem passo a passo (bring-up incremental) |
| [`docs/06-boas-praticas.md`](docs/06-boas-praticas.md) | Convenções do projeto — como estender sem quebrar o que já existe |
| [`CHANGELOG.md`](CHANGELOG.md) | Histórico do que foi feito, sessão a sessão |

## Build

Projeto em [PlatformIO](https://platformio.org/) (instale a extensão no
VSCode, ou o CLI). Não precisa de Arduino IDE.

```bash
pio run              # compila
pio run -t upload    # grava no ESP32 (conecte via USB)
pio device monitor   # monitor serial, se precisar debugar
```

Se der erro de biblioteca faltando, confirme que `platformio.ini` baixou a
`LiquidCrystal_I2C`. Se o `board = esp32dev` não for a sua placa exata (ex:
você tem uma S3), troque essa linha e recompile.

Antes de gravar o firmware completo pela primeira vez, veja o
[guia de montagem](docs/05-montagem.md) — ele testa cada módulo de
hardware isoladamente primeiro, o que economiza muito tempo de debug.

## Estrutura do código

```
include/   - interface pública de cada módulo (Config, Chords, Waveforms,
             Keypad, Input, Display, Synth)
src/       - implementação de cada módulo + os "mains":
             main.cpp             - firmware completo (ambiente esp32dev)
             bringup_i2s.cpp      - teste isolado do PCM5102A (test_i2s)
             bringup_lcd.cpp      - teste isolado do LCD I2C (test_lcd)
             bringup_keypad.cpp   - teste isolado da matriz 4x4 (test_keypad)
             bringup_joystick.cpp - teste isolado do joystick/pots (test_joystick)
docs/      - documentação detalhada (ver tabela acima)
```

Mais detalhes de cada módulo em [`docs/04-firmware.md`](docs/04-firmware.md).

## Estado da montagem

| Subsistema | Estado |
|---|---|
| Áudio (PCM5102A / I2S) | validado em hardware ✅ |
| Display (LCD I2C, `0x27`) | validado em hardware ✅ |
| Matriz 4x4 | 13 de 16 teclas validadas — a linha 2 estava ligada em D12, o correto é **D14** |
| Joystick | ainda não confirmado |
| Potes de tempo e timbre | leitura presa nos extremos — falta ligar as 3 pernas |
| Potes de glide e gate | não ligados (desativados por flag em `Config.h`) |
| Estágio LM358 + jack P10 | não montado — hoje o som sai pelo P2 do módulo |

## Próximos passos possíveis

- Envelope ADSR completo (hoje é só attack/release curtos por passo do arpejo)
- Salvar o último timbre/BPM na EEPROM/NVS
- Usar o 2º op-amp do LM358 pra uma saída estéreo de verdade (hoje é mono duplicado)
- Entrada de áudio externa pelo 2º jack P10 (efeito tipo looper/delay)
- Polifonia entre teclas diferentes (hoje o acorde soa junto, mas de uma tecla por vez)
