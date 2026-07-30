# Hardware

## Peças usadas

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

> Não há como saber o valor exato dos resistores/capacitores porque os
> modelos específicos não foram informados — os valores abaixo são os
> padrões de qualquer kit de eletrônica básico. Se algo não bater com o que
> você tem em mãos, é só avisar para ajustar.

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

Endereço I2C padrão assumido: `0x27`. Se o backpack for `0x3F`, trocar em
`include/Config.h` (`LCD_I2C_ADDR`). O sketch `test_lcd` (ver
[`05-montagem.md`](05-montagem.md)) faz um scan do barramento e informa o
endereço real encontrado.

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

Layout físico sugerido (anotar/serigrafar nas teclas):

```
[ C  ] [ C# ] [ D  ] [ D# ]
[ E  ] [ F  ] [ F# ] [ G  ]
[ G# ] [ A  ] [ A# ] [ B  ]
[MAJ ] [MIN ] [MIN7] [AUG ]
```

As 12 primeiras teclas tocam/param o acorde com aquela nota como fundamental
(apertar de novo na mesma nota = parar). A última linha troca o tipo de
acorde a qualquer momento, mesmo com o arpejo já tocando.

> **Nota sobre GPIO5**: é um pino de "strapping" do ESP32 (usado na
> inicialização do boot). Como ele fica como entrada com pull-up e só é
> puxado pra GND se alguém apertar aquele botão específico, na prática só dá
> problema se estiver segurando exatamente essa tecla no instante em que a
> placa liga. Se algum dia isso causar boot estranho, trocar C4 para outro
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

O **terceiro potenciômetro** (volume) fica **fora do firmware**: ligado
como divisor de tensão passivo direto no caminho de áudio, depois do
estágio de buffer (ver seção abaixo), com o wiper indo pro jack P10. É mais
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
- Para tocar direto num alto-falante pequeno (sem amplificador externo),
  vale usar o transistor NPN como estágio seguidor de emissor depois do
  buffer — mas para fone de ouvido ou caixa amplificada isso não é
  necessário.
- O segundo jack P10 disponível pode ficar de reserva (2ª saída, usando o
  outro op-amp do LM358) ou para uma futura entrada de áudio externa — isso
  ainda não está implementado no firmware porque exigiria um estágio de
  atenuação/bias separado no ADC do ESP32 (ver "escopo deixado de fora" em
  [`01-ideia.md`](01-ideia.md)).

## Consumo de pinos (referência rápida)

| Pino | Uso |
|---|---|
| GPIO4 | Joystick SW |
| GPIO5 | Matriz C4 |
| GPIO13, 14, 16, 17 | Matriz R1-R4 |
| GPIO18, 19, 23 | Matriz C1-C3 |
| GPIO21, 22 | I2C (LCD) |
| GPIO25, 26, 27 | I2S (PCM5102A) |
| GPIO32, 33 | Potenciômetros (tempo, timbre) |
| GPIO34, 35 | Joystick VRx, VRy |
| GPIO36, 39 | Livres (expansão futura) |

Nenhum pino é reutilizado — 20 GPIOs ao todo, dentro do orçamento de um
DevKit de 30/38 pinos.
