# Arquitetura

## Visão geral (hardware)

```
                         +----------------------+
   Matriz 4x4  --------> |                      |
   (8 GPIO)              |                      |
                         |                      | --I2S--> PCM5102A --> LM358 --> Jack P10 --> som
   Joystick   ---------> |        ESP32         |
   (2 ADC+1 digital)     |                      |
                         |                      | --I2C--> LCD 16x2
   Potenciometros ------>|                      |
   (2 ADC)                +----------------------+
```

Tudo entra no ESP32 (botões, joystick, potenciômetros), ele decide o que
tocar e o que mostrar, e sai por dois barramentos diferentes: **I2S** pro
áudio (PCM5102A) e **I2C** pro display (LCD).

## Visão geral (firmware)

```
                    +-----------+     +-----------+
   loop() ------->  |  Keypad   |     |   Input   |
                    | (matriz)  |     | (joy/pots)|
                    +-----+-----+     +-----+-----+
                          |                 |
                          v                 v
                    +----------------------------+
                    |           Synth            |  <-- estado central:
                    |  (vozes, acorde, modo de    |      raiz, acorde, modo
                    |   toque, timbre, I2S)       |      (bloco/arpejo), timbre, BPM
                    +--------------+-------------+
                                   |
                       +-----------+-----------+
                       v                       v
                 +-----------+           +-----------+
                 |  I2S out  |           |  Display  |
                 | (PCM5102A)|           | (LCD I2C) |
                 +-----------+           +-----------+
```

O `Synth` é o dono do estado (qual nota está tocando, tipo de acorde,
timbre, andamento). `main.cpp` só liga os fios: lê `Keypad`/`Input`, manda
pro `Synth`, e usa o estado atual do `Synth` pra montar o que o `Display`
mostra. Nenhum outro módulo guarda estado por conta própria — isso evita
os módulos ficarem "discordando" sobre o que está tocando.

## Por que essas escolhas

**Wavetable synthesis (em vez de calcular seno em tempo real).** As 4
formas de onda (`Waveforms.h`) são pré-calculadas uma vez no `setup()` em
tabelas de 256 amostras. Tocar é só ler da tabela com um acumulador de
fase — muito mais barato que chamar `sinf()` a cada amostra, o que
importa porque o ESP32 precisa gerar ~44100 amostras por segundo sem
travar.

**`i2s_write()` bloqueante como "metrônomo" do loop principal.** Em vez de
usar uma interrupção/timer separado pra áudio, `synthProcess()` gera um
bloco de amostras e chama `i2s_write()`, que só retorna quando o DMA do
I2S tem espaço. Isso naturalmente pausa o `loop()` pelo tempo certo entre
blocos de áudio (~5.8ms com os parâmetros atuais), o que serve de
cadência pro resto do sistema (varrer teclado, ler ADC, atualizar LCD) sem
precisar de RTOS tasks separadas. É a abordagem mais simples que ainda dá
áudio sem estalos — sacrifica um pouco de "capacidade de resposta" das
outras tarefas em troca de simplicidade de código.

**Dois modos de toque sobre o mesmo acorde.** Segurar toca o acorde em
bloco (todas as notas juntas, sustentadas); tocar rápido arpeja. O acorde
em si vem do joystick e é independente do modo — ver
[`01-ideia.md`](01-ideia.md).

**Vozes independentes (polifonia interna).** Pro acorde em bloco existir, o
motor mantém uma voz por nota do acorde (`MAX_CHORD_NOTES`), cada uma com
seu próprio acumulador de fase e sua própria frequência com glide. A soma
é dividida pelo número de vozes ativas pra não estourar o `int16`. No modo
arpejo só a voz 0 é usada — as notas do acorde se revezam nela.

**Suavização exponencial (EMA) em tudo que é contínuo.** O mesmo truque
aparece em três lugares: no glide de afinação, no morph de timbre e no
filtro das leituras de ADC. Em vez de saltar pro valor novo, o valor atual
anda uma fração do caminho a cada passo. É barato, não precisa de buffer,
e resolve dois problemas de uma vez — transição fluida e imunidade ao
tremor natural da leitura analógica.

**Módulos separados por responsabilidade física, não por camada
abstrata.** `Keypad`, `Input`, `Display`, `Synth` correspondem 1:1 a
componentes de hardware reais. Isso facilita o bring-up incremental (ver
[`05-montagem.md`](05-montagem.md)): cada módulo tem um teste isolado
correspondente.

**`Config.h` como fonte única de verdade pra pinagem.** Nenhum outro
arquivo declara um número de GPIO diretamente — tudo referencia uma
constante de `Config.h`. Trocar a fiação física = trocar uma linha nesse
arquivo, não caçar números espalhados pelo código.

## Fluxo de uma jogada de tecla

Como não dá pra saber se é toque rápido ou "segurar" no instante em que a
tecla desce, o firmware **sempre começa no modo sustentado** e só decide
quando a tecla sobe:

1. `keypadUpdate()` varre a matriz; `keypadJustPressed()` reporta a tecla que desceu (com debounce)
2. `main.cpp` chama `synthNoteOn()`, entra em `TouchMode::HOLDING` e guarda o instante do toque
3. Enquanto segura: modo `SUSTAINED`, e o acorde vem do joystick (centro = nota pura)
4. `keypadJustReleased()` reporta a tecla que subiu. `main.cpp` compara o tempo segurado com `TAP_THRESHOLD_MS` (250ms):
   - **abaixo** → foi um toque: passa pra `TouchMode::ARPEGGIO`, o som continua sozinho
   - **acima** → foi um segurar: `synthNoteOff()`, o som para com um release curto
5. `synthProcess()` gera as amostras: em `SUSTAINED` soma todas as vozes do acorde; em `ARPEGGIO` cicla uma nota por passo, no tempo do BPM
6. `main.cpp` monta um `SynthStatus` e passa pro `displayUpdate()`, que só redesenha o LCD se algo mudou desde a última vez
