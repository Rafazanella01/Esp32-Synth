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

## Peças usadas (o que você já tem)

| Peça | Função no projeto |
|---|---|
| ESP32 DevKit | cérebro do sintetizador |
| Módulo GY-PCM5102 (PCM5102A) | DAC de áudio, via **I2S** |
| Matriz de botões 4x4 (8 pinos: R1-R4, C1-C4) | seleção de nota + tipo de acorde |
| Display LCD 16x2 com backpack I2C (4 pinos) | mostra nota/acorde/timbre/BPM |
| Joystick analógico (VRx, VRy, SW) | pitch-bend, navegação de página, pânico |
| Potenciômetros 10K | andamento (BPM), timbre, e volume analógico |
| LM358 (dual op-amp) | buffer de saída de áudio antes do jack P10 |
| Capacitores | acoplamento AC / desacoplamento de alimentação |
| Transistor NPN (genérico, ex. BC547/2N2222) | *opcional*: estágio de corrente extra se quiser tocar direto num alto-falante pequeno |
| Jack P10 | saída de áudio para caixa/amplificador/fone |

> Não tenho como saber o valor exato dos seus resistores/capacitores porque
> você não especificou os modelos — os valores abaixo são os padrões de
> qualquer kit de eletrônica básico. Se algo não bater com o que você tem,
> me fala que eu ajusto.

## Mapa de pinos (ESP32 DevKit genérico)

### I2S — PCM5102A (áudio)
| PCM5102A | ESP32 |
|---|---|
| BCK | GPIO27 |
| LRCK/WS | GPIO25 |
| DIN | GPIO26 |
| SCK | GND |
| FMT | GND |
| **XSMT** | **3.3V** (crítico: sem isso o módulo fica mudo) |
| VIN | 3.3V |
| GND | GND |

### I2C — Display LCD 16x2
| LCD backpack | ESP32 |
|---|---|
| SDA | GPIO21 |
| SCL | GPIO22 |
| VCC | 5V (a maioria dos backpacks I2C de LCD pede 5V pro contraste/backlight funcionar direito) |
| GND | GND |

Endereço I2C padrão assumido: `0x27`. Se o seu backpack for `0x3F`, troque em
`include/Config.h` (`LCD_I2C_ADDR`). Se não souber, rode um "I2C scanner" (tem
vários exemplos prontos) pra descobrir.

### Matriz de botões 4x4
| Matriz | ESP32 |
|---|---|
| R1 | GPIO13 |
| R2 | GPIO14 |
| R3 | GPIO16 |
| R4 | GPIO17 |
| C1 | GPIO18 |
| C2 | GPIO19 |
| C3 | GPIO23 |
| C4 | GPIO5 |

Layout físico sugerido (anote/serigrafe nas teclas):

```
[ C  ] [ C# ] [ D  ] [ D# ]
[ E  ] [ F  ] [ F# ] [ G  ]
[ G# ] [ A  ] [ A# ] [ B  ]
[MAJ ] [MIN ] [MIN7] [AUG ]
```

As 12 primeiras teclas tocam/param o acorde com aquela nota como fundamental
(aperta de novo na mesma nota = para). A última linha troca o tipo de acorde
a qualquer momento, mesmo com o arpejo já tocando.

> **Nota sobre GPIO5**: é um pino de "strapping" do ESP32 (usado na
> inicialização do boot). Como ele fica como entrada com pull-up e só é
> puxado pra GND se alguém apertar aquele botão específico, na prática só dá
> problema se você estiver segurando exatamente essa tecla no instante em que
> liga a placa. Se algum dia isso causar boot estranho, troque C4 para outro
> GPIO livre (ex: GPIO15).

### Joystick analógico
| Joystick | ESP32 |
|---|---|
| VRx | GPIO34 |
| VRy | GPIO35 |
| SW | GPIO4 |
| VCC | 3.3V |
| GND | GND |

### Potenciômetros
| Função | ESP32 |
|---|---|
| Andamento (BPM) | GPIO32 |
| Timbre | GPIO33 |
| *(livres p/ expansão futura, ex: decay)* | GPIO36 (VP), GPIO39 (VN) |

O **terceiro potenciômetro** (volume) eu sugiro deixar **fora do firmware**:
ligue ele como divisor de tensão passivo direto no caminho de áudio, depois
do estágio de buffer (ver seção abaixo), com o wiper indo pro jack P10. É mais
simples, não consome pino nenhum e funciona mesmo com o ESP32 desligado do
áudio.

## Estágio de saída de áudio (LM358 + jack P10)

O PCM5102A já entrega um sinal de linha, mas um buffer com o LM358 ajuda a
tocar em fone/caixa sem carregar a saída do DAC, e permite meter um
potenciômetro de volume sem perder qualidade. Circuito de buffer unitário
(um dos dois amp-ops do LM358 — o outro fica livre para uma segunda saída
estéreo, se quiser expandir depois):

```
      3V3
       |
      R1 (10k)
       |
       +------------------+--------- IN+ (pino 3 do LM358)
       |                  |
      R2 (10k)          C_bias (10uF p/ GND)
       |                  |
      GND                GND

  PCM5102A (LOUT) --[C_in 1uF]--> (mesmo nó do IN+, soma o sinal AC sobre o bias)

  IN- (pino 2) ------ ligado direto no OUT (pino 1)   <- buffer unitário (voltage follower)

  OUT (pino 1) --[C_out 10uF]--> potenciômetro de volume (10K) --> jack P10 (tip)
                                                                 jack P10 (sleeve) -> GND

  V+ (pino 8) -> 3V3 (ou 5V, se preferir mais headroom)
  V- (pino 4) -> GND
  C_desacoplamento 100nF entre pino 8 e GND, perto do LM358
```

- `R1`/`R2` só precisam ser parecidos entre si (qualquer valor entre 4.7k e
  47k funciona) — eles criam o "meio da alimentação" pro op-amp trabalhar em
  fonte única.
- O potenciômetro de volume: ponta e ponta ligadas no sinal e no GND, o
  **wiper** vai pro jack.
- Se quiser tocar direto num alto-falante pequeno (sem amplificador
  externo), aí sim vale usar o transistor NPN como estágio seguidor de
  emissor depois do buffer — mas para fone de ouvido ou caixa amplificada
  isso não é necessário.
- O segundo jack P10 que você tem pode ficar de reserva (2ª saída, usando o
  outro op-amp do LM358) ou para uma futura entrada de áudio externa — isso
  não está implementado no firmware ainda porque exigiria um estágio de
  atenuação/bias separado no ADC do ESP32. Se quiser isso no futuro, me avisa.

## Build

Projeto em [PlatformIO](https://platformio.org/) (instale a extensão no
VSCode, ou o CLI). Não precisa de Arduino IDE.

```bash
pio run              # compila
pio run -t upload    # grava no ESP32 (conecte via USB)
pio device monitor    # monitor serial, se precisar debugar
```

Se der erro de biblioteca faltando, confirme que `platformio.ini` baixou a
`LiquidCrystal_I2C`. Se o `board = esp32dev` não for a sua placa exata (ex:
você tem uma S3), troque essa linha e recompile.

## Estrutura do código

```
include/
  Config.h     - todos os pinos e constantes de tuning num lugar só
  Chords.h     - tabela de intervalos dos acordes
  Waveforms.h  - wavetables dos timbres
  Keypad.h     - scanner da matriz 4x4
  Input.h      - joystick + potenciômetros
  Display.h    - LCD I2C
  Synth.h      - motor de áudio I2S + arpejador
src/
  *.cpp        - implementação de cada módulo acima
  main.cpp     - loop principal, liga tudo
```

## Próximos passos possíveis

- Envelope ADSR completo (hoje é só attack/release curtos por passo do arpejo)
- Salvar o último timbre/BPM na EEPROM/NVS
- Usar o 2º op-amp do LM358 pra uma saída estéreo de verdade (hoje é mono duplicado)
- Entrada de áudio externa pelo 2º jack P10 (efeito tipo looper/delay)
- Segundo modo de toque: acorde em bloco (todas as notas juntas) em vez de arpejo
