# Hardware

## Peças usadas

| Peça | Função no projeto |
|---|---|
| ESP32 DevKit | cérebro do sintetizador |
| Módulo GY-PCM5102 (PCM5102A) | DAC de áudio, via **I2S** |
| Matriz de botões 4x4 (8 pinos: R1-R4, C1-C4) | seleção de nota + tipo de acorde |
| Display LCD 16x2 com backpack I2C (4 pinos) | mostra nota/acorde/timbre/BPM |
| Joystick analógico (VRx, VRy, SW) | escolhe a qualidade do acorde ao vivo (10 posições) |
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
| Matriz | ESP32 | Rótulo na placa (DevKit 30 pinos) |
|---|---|---|
| R1 | GPIO13 | D13 |
| R2 | GPIO14 | D14 |
| R3 | GPIO18 | D18 |
| R4 | GPIO19 | D19 |
| C1 | GPIO23 | D23 |
| C2 | GPIO16 | RX2 |
| C3 | GPIO17 | TX2 |
| C4 | GPIO2 | D2 |

> Nessa placa específica, GPIO16/17 aparecem serigrafados como `RX2`/`TX2`
> (são os pinos default da UART2), mas como o projeto não usa UART2, estão
> livres pra usar como GPIO normal.

Layout físico sugerido (anotar/serigrafar nas teclas):

```
[ C4 ] [ C#4] [ D4 ] [ D#4]
[ E4 ] [ F4 ] [ F#4] [ G4 ]
[ G#4] [ A4 ] [ A#4] [ B4 ]
[ C5 ] [ C#5] [ D5 ] [ D#5]
```

Todas as 16 teclas são notas. **Segurar** toca o acorde em bloco
(sustentado); **tocar rápido** dispara o arpejo, que continua sozinho até
você tocar aquela mesma tecla de novo. Qual acorde soa vem do joystick —
ver [`01-ideia.md`](01-ideia.md#como-o-toque-funciona-o-desenho-central).

> **Nota sobre GPIO2**: é um pino de "strapping" do ESP32 (usado na
> inicialização do boot). Como ele fica como entrada com pull-up e só é
> puxado pra GND se alguém apertar aquele botão específico, na prática só dá
> problema se estiver segurando exatamente essa tecla no instante em que a
> placa liga. Se algum dia isso causar boot estranho, trocar C4 para outro
> GPIO livre.

### Joystick analógico
| Joystick | ESP32 | Rótulo na placa |
|---|---|---|
| VRx | GPIO34 | D34 |
| VRy | GPIO35 | D35 |
| SW | GPIO4 | D4 |
| VCC | 3.3V | 3V3 |
| GND | GND | GND |

> O centro do joystick é **medido no boot**, não assumido como o meio da
> escala — deixe o joystick em repouso quando a placa liga. Isso importa
> porque o ADC do ESP32 é bem não-linear perto de 3.3V: o repouso costuma
> ler ~3000 em vez de ~2048, o que dá cursos bem diferentes pra cada lado.
> O firmware normaliza cada lado pelo seu próprio curso pra compensar
> (ver `normalizeAxis` em `src/Input.cpp`).

### Potenciômetros
| Função | ESP32 | Rótulo na placa |
|---|---|---|
| Andamento (BPM) | GPIO32 | D32 |
| Timbre (morph entre formas de onda) | GPIO33 | D33 |
| Glide/portamento entre notas | GPIO36 | VP |
| Gate do arpejo (staccato ↔ legato) | GPIO39 | VN |

Cada potenciômetro usa as **3 pernas**: as duas pontas em 3.3V e GND, e o
**pino do meio (wiper)** no GPIO. Com só duas pernas ligadas ele não vira
divisor de tensão e a leitura fica presa num extremo da escala.

Os dois últimos (glide e gate) são **opcionais** e vêm desligados por
flag em `include/Config.h` (`POT_GLIDE_CONNECTED`,
`POT_ARP_GATE_CONNECTED`). Ligou fisicamente? Vire a flag pra `true` e
regrave. Enquanto estiverem desligados, o firmware nem lê o pino e usa um
valor padrão fixo — um pino de ADC sem nada ligado **flutua**, e isso faria
o glide e o gate variarem sozinhos (com o gate baixo, o som chega a
picotar até quase sumir).

O **potenciômetro de volume** fica **fora do firmware**: ligado como
divisor de tensão passivo direto no caminho de áudio, depois do estágio de
buffer (ver seção abaixo), com o wiper indo pro jack P10. É mais simples,
não consome pino nenhum e funciona mesmo com o ESP32 desligado do áudio.

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

| Pino | Rótulo | Uso |
|---|---|---|
| GPIO2 | D2 | Matriz C4 |
| GPIO4 | D4 | Joystick SW |
| GPIO13 | D13 | Matriz R1 |
| GPIO14 | D14 | Matriz R2 |
| GPIO16, 17 | RX2, TX2 | Matriz C2, C3 |
| GPIO18, 19 | D18, D19 | Matriz R3, R4 |
| GPIO21, 22 | D21, D22 | I2C (LCD) |
| GPIO23 | D23 | Matriz C1 |
| GPIO25, 26, 27 | D25, D26, D27 | I2S (PCM5102A) |
| GPIO32, 33 | D32, D33 | Potes de tempo e timbre |
| GPIO34, 35 | D34, D35 | Joystick VRx, VRy |
| GPIO36, 39 | VP, VN | Potes de glide e gate (opcionais) |

Nenhum pino é reutilizado — 20 GPIOs ao todo, dentro do orçamento de um
DevKit de 30/38 pinos.
