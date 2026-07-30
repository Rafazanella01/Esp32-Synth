# ESP32 Synth

Sintetizador de acordes com arpejador, baseado em ESP32 + saída de áudio I2S
(PCM5102A) + display LCD + matriz de botões + joystick + potenciômetros.

Você aperta uma tecla de **nota** (fundamental) e o acorde correspondente ao
**tipo** selecionado (Maior / Menor / Menor7 / Aumentado) começa a tocar em
arpejo, repetindo em loop até você apertar de novo (toggle) ou trocar de nota.

## O que o firmware faz

- 4 tipos de acorde: Maior, Menor, Menor com sétima (m7), Aumentado
- Arpejador contínuo (fica repetindo/"arranjando" o acorde, como um sintetizador de verdade)
- 4 timbres selecionáveis por potenciômetro: Seno, Triangular, Dente-de-serra, Quadrada
- Pitch-bend ao vivo pelo eixo X do joystick
- Controle de andamento (BPM) por potenciômetro
- Display LCD 16x2 mostrando nota/acorde/timbre/BPM (página 1) e uma
  legenda de ajuda (página 2), navegável pelo eixo Y do joystick
- Botão do joystick = "pânico" (silencia tudo na hora)

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

## Próximos passos possíveis

- Envelope ADSR completo (hoje é só attack/release curtos por passo do arpejo)
- Salvar o último timbre/BPM na EEPROM/NVS
- Usar o 2º op-amp do LM358 pra uma saída estéreo de verdade (hoje é mono duplicado)
- Entrada de áudio externa pelo 2º jack P10 (efeito tipo looper/delay)
- Segundo modo de toque: acorde em bloco (todas as notas juntas) em vez de arpejo
