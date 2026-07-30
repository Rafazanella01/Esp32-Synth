# Firmware — referência dos módulos

Cada módulo é um par `include/X.h` (interface) + `src/X.cpp`
(implementação). `main.cpp` só chama essas funções — a lógica de cada
subsistema fica isolada no módulo dele.

## `Config.h`

Não tem `.cpp` — é só constantes (`static const`). **Fonte única de
verdade** para todo número de GPIO, endereço I2C, tamanho de buffer e
constante de tuning (BPM, sample rate, debounce). Ver
[`03-hardware.md`](03-hardware.md) pra tabela completa de pinos.

Ao adicionar um novo pino ou constante de tuning ao projeto, ele entra
aqui — nunca como número mágico direto em outro arquivo.

## `Chords.h`

Puramente dados, header-only (sem `.cpp`).

- `enum class ChordType { MAJOR, MINOR, MINOR7, AUGMENTED }`
- `ChordDef` — nome + intervalos em semitons a partir da fundamental
- `chordDef(ChordType)` — retorna a definição
- `NOTE_NAMES[12]` — nomes das notas na ordem das teclas 0-11
- `noteFrequency(noteIndex, baseC4)` — calcula Hz a partir do índice
  cromático (0=C, 11=B) relativo à oitava de `BASE_FREQ_C4`

## `Waveforms.h` / `Waveforms.cpp`

- `enum class Waveform { SINE, TRIANGLE, SAW, SQUARE }`
- `generateWaveTables()` — preenche as 4 wavetables de 256 amostras
  (`int16_t`, -32767..32767). Chamar **uma vez** no `setup()`.
- `waveTables[timbre][amostra]` — acesso direto às tabelas (usado pelo
  `Synth` no hot path de áudio, por isso é array global e não função)
- `waveformName(Waveform)` — nome curto pro LCD
- `nextWaveform(Waveform)` — próximo timbre com wrap-around

## `Keypad.h` / `Keypad.cpp`

Scanner da matriz 4x4, sem biblioteca externa.

- `keypadInit()` — configura linhas como `OUTPUT` (idle HIGH) e colunas
  como `INPUT_PULLUP`
- `keypadPoll()` — varre a matriz inteira, com debounce por tecla
  (`DEBOUNCE_MS` em `Config.h`). Retorna o índice (0-15) da tecla que
  **acabou de ser pressionada** neste ciclo (borda de subida), ou `-1`

Chamar `keypadPoll()` a cada iteração do `loop()`. Só reporta 1 tecla nova
por chamada — suficiente para toque sequencial, não faz *n-key rollover*.

## `Input.h` / `Input.cpp`

Leitura de joystick e potenciômetros, com polling limitado por tempo
(`ADC_POLL_MS`) pra não gastar ciclo de CPU lendo ADC a toda hora.

- `inputInit()` — configura resolução do ADC (12 bits) e pull-up do botão
- `inputUpdate()` — lê pots/joystick se já passou `ADC_POLL_MS` desde a
  última leitura; senão não faz nada (chamar toda iteração do `loop()`,
  o próprio módulo decide quando de fato ler o hardware)
- `getTempoBPM()` — BPM mapeado de `ARP_BPM_MIN` a `ARP_BPM_MAX`
- `getSelectedTimbre()` — timbre mapeado a partir do pote de timbre
- `getPitchBendSemitones()` — -1..+1 semitom a partir do eixo X do
  joystick, com *deadzone* central (`JOY_DEADZONE`)
- `consumeDisplayPageGesture()` — gesto do eixo Y (-1/0/+1), **edge-
  triggered**: só reporta 1 vez por vez que o stick sai do centro, evita
  ficar avançando página em rajada enquanto o stick fica deslocado
- `consumePanicPress()` — `true` uma única vez no instante em que o botão
  do joystick foi pressionado (debounced)

O padrão "consume" (zera o estado pendente ao ser lido) é usado pra
eventos que devem disparar exatamente uma vez, ao contrário dos getters de
valor contínuo (`getTempoBPM` etc.) que sempre retornam o estado atual.

## `Display.h` / `Display.cpp`

Wrapper do LCD 16x2 I2C (biblioteca `LiquidCrystal_I2C`).

- `struct SynthStatus` — snapshot do que mostrar: `playing`, `rootNote`,
  `chordType`, `timbre`, `bpm`
- `displayInit()` — inicializa `Wire` e o LCD, mostra tela de boas-vindas
- `displaySetPage(delta)` — troca de página (-1/0/+1). Página 0 = status,
  página 1 = ajuda (legenda das teclas)
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
- `synthTriggerRoot(rootNote)` — inicia o arpejo naquela nota, ou para se
  já for a nota tocando (toggle)
- `synthSetChordType(type)` — troca o tipo de acorde com o som já rolando
- `synthSetTimbre(Waveform)`, `synthSetTempoBPM(bpm)`,
  `synthSetPitchBendSemitones(semis)` — parâmetros ao vivo
- `synthStop()` — silencia
- `synthIsPlaying()`, `synthCurrentRoot()`, `synthCurrentChordType()` —
  getters, usados pelo `main.cpp` pra montar o `SynthStatus` do display

### Como o arpejo funciona internamente

Cada "passo" do arpejo dura `samplesPerStep` amostras (derivado do BPM).
Dentro do passo, a nota soa por `ARP_GATE` (75%) do tempo, com uma
rampa curta de ataque/release pra evitar estalo, e fica em silêncio no
resto — isso dá o "groove" percussivo característico de arpejador. Ao
fim do passo, avança pro próximo tom do acorde (`arpStep % numNotes`) e
recalcula a frequência (aplicando o pitch-bend atual).

## `main.cpp`

Não tem lógica própria além de: inicializar tudo, e a cada `loop()`
chamar `synthProcess()`, ler as entradas, mandar as mudanças pro `Synth`,
e atualizar o `Display` com o estado atual do `Synth`. Ver o diagrama de
fluxo em [`02-arquitetura.md`](02-arquitetura.md#fluxo-de-uma-jogada-de-tecla).
