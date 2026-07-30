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
                    |  (acorde atual, arpejo,     |      raiz, tipo de acorde,
                    |   timbre, I2S)              |      timbre, BPM, pitch-bend
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

**Arpejador como modo principal (não acorde em bloco).** Ver
[`01-ideia.md`](01-ideia.md) — foi a leitura mais direta do pedido de
"ficar repetindo, fazer arranjos como sintetizador".

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

1. `keypadPoll()` varre a matriz e detecta uma tecla nova pressionada (com debounce)
2. `main.cpp` decide: tecla 0-11 = nota → `synthTriggerRoot()`; tecla 12-15 = tipo de acorde → `synthSetChordType()`
3. `synthTriggerRoot()` faz *toggle*: se já está tocando a mesma raiz, para; senão, troca o acorde e zera o passo do arpejo
4. No próximo `synthProcess()`, o motor de áudio recalcula a frequência do passo atual do arpejo e começa a gerar as amostras daquele acorde
5. `main.cpp` monta um `SynthStatus` com o estado atual (`synthIsPlaying()`, `synthCurrentRoot()` etc.) e passa pro `displayUpdate()`, que só redesenha o LCD se algo mudou desde a última vez
