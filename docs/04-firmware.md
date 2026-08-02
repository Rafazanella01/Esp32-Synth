# Firmware — referência dos módulos

Cada módulo é um par `include/X.h` (interface) + `src/X.cpp`
(implementação). `main.cpp` só chama essas funções — a lógica de cada
subsistema fica isolada no módulo dele.

## `Config.h`

Não tem `.cpp` — é só constantes (`static const`). **Fonte única de
verdade** para todo número de GPIO, endereço I2C, tamanho de buffer e
constante de tuning (BPM, glide, gate, debounce). Ver
[`03-hardware.md`](03-hardware.md) pra tabela completa de pinos.

Ao adicionar um novo pino ou constante de tuning ao projeto, ele entra
aqui — nunca como número mágico direto em outro arquivo.

Duas flags importantes: `POT_GLIDE_CONNECTED` e `POT_ARP_GATE_CONNECTED`.
Elas existem porque um pino de ADC sem nada ligado **flutua** — vire pra
`true` só depois de ligar o potenciômetro de verdade.

## `Chords.h`

Puramente dados, header-only (sem `.cpp`).

- `enum class ChordType` — 11 valores: `NATURAL` (nota pura, sem
  empilhar nada) mais as 10 qualidades selecionáveis pelo joystick
  (`MAJOR`, `MAJOR7`, `DOMINANT7`, `AUGMENTED`, `SUS4`, `MINOR7`,
  `MINOR`, `DIMINISHED`, `SUS2`, `SIXTH`)
- `ChordDef` — nome + intervalos em semitons a partir da fundamental
- `chordDef(ChordType)` — retorna a definição
- `NOTE_NAMES[16]` — nomes das notas na ordem das teclas 0-15 (C4 a D#5)
- `noteFrequency(noteIndex, baseC4)` — calcula Hz a partir do índice
  cromático relativo a `BASE_FREQ_C4`. O índice cresce livremente, então
  valores acima de 11 caem naturalmente na oitava seguinte

## `Waveforms.h` / `Waveforms.cpp`

- `enum class Waveform { SINE, TRIANGLE, SAW, SQUARE }`
- `generateWaveTables()` — preenche as 4 wavetables de 256 amostras
  (`int16_t`, -32767..32767). Chamar **uma vez** no `setup()`.
- `waveTables[timbre][amostra]` — acesso direto às tabelas (usado pelo
  `Synth` no hot path de áudio, por isso é array global e não função)
- `waveformName(Waveform)` — nome curto pro LCD

## `Keypad.h` / `Keypad.cpp`

Scanner da matriz 4x4, sem biblioteca externa. Reporta **descida e
subida** de tecla separadamente — é isso que permite o `main.cpp`
distinguir toque rápido de segurar.

- `keypadInit()` — configura linhas como `OUTPUT` (idle HIGH) e colunas
  como `INPUT_PULLUP`
- `keypadUpdate()` — varre a matriz inteira com debounce por tecla
  (`DEBOUNCE_MS`). Chamar a cada iteração do `loop()`
- `keypadJustPressed()` — índice (0-15) da tecla que acabou de descer,
  ou `-1`. Só reporta uma vez por toque
- `keypadJustReleased()` — idem, pra tecla que acabou de subir

## `Input.h` / `Input.cpp`

Leitura de joystick e potenciômetros, com polling limitado por tempo
(`ADC_POLL_MS`) pra não gastar ciclo de CPU lendo ADC a toda hora.

- `inputInit()` — configura resolução do ADC (12 bits), atenuação dos
  eixos do joystick, pull-up do botão, e **calibra o centro do joystick**
  (o joystick precisa estar em repouso quando a placa liga)
- `inputUpdate()` — lê pots/joystick se já passou `ADC_POLL_MS` desde a
  última leitura; senão não faz nada (chamar toda iteração do `loop()`,
  o próprio módulo decide quando de fato ler o hardware)
- `getTempoBPM()` — BPM mapeado de `ARP_BPM_MIN` a `ARP_BPM_MAX`
- `getTimbrePosition()` — posição **contínua** entre timbres,
  `0..(Waveform::COUNT - 1)`. Fracionário é válido: a mistura entre as
  duas wavetables vizinhas acontece no motor de áudio
- `getGlideMs()`, `getArpGate()` — valores dos pots opcionais (ou o
  padrão fixo, se as flags de `Config.h` estiverem em `false`)
- `getJoystickChordType()` — qualidade do acorde pela posição atual do
  joystick. No centro retorna `MAJOR`
- `isJoystickCentered()` — `true` com o joystick parado e sem clique.
  Serve pra quem chama decidir o que "centro" significa: ao segurar a
  tecla vira nota pura, no arpejo vira acorde maior

### Como as 8 direções são detectadas

Duas coisas fazem isso funcionar num joystick barato ligado no ADC do
ESP32:

1. **Centro calibrado no boot.** O repouso quase nunca é o meio da escala
   — nesta placa lê ~3020 em X e ~3150 em Y, não 2048
2. **Cada lado normalizado pelo seu próprio curso** (`normalizeAxis`). O
   ADC do ESP32 comprime a leitura perto de 3.3V, então o curso "pra cima"
   tem muito menos contagens que o "pra baixo". Sem isso, uma direção
   responde bem e a oposta quase não responde

Com os dois eixos já normalizados em -1..+1, a direção sai do **ângulo**
(`atan2`) dividido em 8 setores de 45°, e não de comparar `dx` com `dy` —
é isso que faz as diagonais existirem de verdade.

## `Display.h` / `Display.cpp`

Wrapper do LCD 16x2 I2C (biblioteca `LiquidCrystal_I2C`).

- `struct SynthStatus` — snapshot do que mostrar: `playing`, `rootNote`,
  `chordType`, `timbre`, `bpm`
- `displayInit()` — inicializa `Wire` e o LCD, mostra tela de boas-vindas
- `displayUpdate(status)` — só reescreve o LCD se algo relevante mudou
  desde a última chamada (evita tráfego I2C e flicker desnecessário)

## `Synth.h` / `Synth.cpp`

O motor de áudio. Dono de todo o estado do que está tocando.

- `synthInit()` — instala o driver I2S (`driver/i2s.h`) com os pinos de
  `Config.h`
- `synthProcess()` — gera um bloco de `I2S_DMA_BUF_LEN` amostras e escreve
  no I2S (bloqueante). **Chamar toda iteração do `loop()`** — é essa
  chamada que mantém o som contínuo e também pauta o ritmo do loop
  principal (ver [`02-arquitetura.md`](02-arquitetura.md))
- `synthNoteOn(rootNote)` / `synthNoteOff()` — liga a nota (com rampa de
  ataque) e inicia o release
- `synthSetPlayMode(PlayMode)` — `SUSTAINED` (acorde em bloco) ou
  `ARPEGGIO`. Pode trocar com o som já rolando, sem cortar
- `synthSetChordType(ChordType)` — idem, troca ao vivo
- `synthSetTimbrePosition(float)`, `synthSetTempoBPM(int)`,
  `synthSetGlideMs(float)`, `synthSetArpGate(float)` — parâmetros ao vivo
- `synthIsPlaying()`, `synthCurrentRoot()`, `synthCurrentChordType()` —
  getters, usados pelo `main.cpp` pra montar o `SynthStatus` do display

### Vozes

O motor mantém `MAX_CHORD_NOTES` vozes, cada uma com seu acumulador de
fase e sua frequência própria (com glide independente):

- **`SUSTAINED`**: uma voz por nota do acorde, todas soando ao mesmo
  tempo. A soma é dividida pelo número de vozes ativas pra não estourar o
  `int16`, e as fases entram espalhadas — se todas começassem em zero, os
  picos somariam de uma vez e o acorde estouraria no primeiro ciclo
- **`ARPEGGIO`**: só a voz 0 é usada; as notas do acorde se revezam nela,
  uma por passo

`refreshVoices()` recalcula esse arranjo sempre que o acorde ou o modo
muda. Voz que acabou de entrar começa já afinada, sem glide a partir de
valor velho.

### Envelopes

Dois envelopes independentes se multiplicam:

- **`noteGain`** — envelope geral da nota: rampa de ataque ao ligar,
  release ao soltar. É ele que evita estalo no início e no fim
- **`stepGateEnvelope()`** — só no modo arpejo: dentro de cada passo a
  nota soa por uma fração (`currentArpGate`) e cala no resto, com
  ataque/release curtinhos. É o que dá o "groove" percussivo do
  arpejador, e o que o pote de gate controla entre staccato e legato

## `main.cpp`

Não tem lógica de áudio própria — decide apenas *o que* tocar, a partir
das entradas, e passa pro `Synth`. A parte não-óbvia é a máquina de
estado de toque (`TouchMode`), que resolve o problema de não dar pra
saber se é toque ou segurar no instante em que a tecla desce. Ver o fluxo
passo a passo em
[`02-arquitetura.md`](02-arquitetura.md#fluxo-de-uma-jogada-de-tecla).
