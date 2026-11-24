# CDP-160S Synth Ensemble

ESP32-S3 + DAC I2S, controlado por página web (painel de equipamento dos anos 70), tocado pelo Casio CDP-160S.

## Estado (importante)

| Parte | Estado |
|---|---|
| `preview/engine.js` | Motor de áudio completo. Testado offline e no navegador. |
| `src/ui.h` | Interface (fonte da página): 17 presets, 8 memórias, vúmetros, rodas, efeitos. |
| `src/main.cpp` | **Desatualizado.** Motor antigo (clavinet + synth simples) e protocolo antigo. Precisa ser portado. |
| Piano por USB host | Não feito. |
| Hardware | Nada testado no ESP32 ainda. |

## Como rodar o painel

Hoje o painel roda **no computador**, com o som saindo pelo navegador. É assim que você ouve e
ajusta os instrumentos enquanto o firmware do ESP32 não recebe o motor novo.

### Requisitos

- Python 3 (ou Node, veja a alternativa abaixo).
- Chrome ou Edge para ter o piano por USB (Web MIDI). Firefox e Safari tocam o painel, mas sem
  Web MIDI; use o teclado da tela ou o do computador.
- Com Python 3 não precisa instalar nada nem ter internet.

### Passo a passo

1. Abra um terminal na pasta do projeto e suba um servidor local para a pasta `preview/`:

   ```bash
   cd preview
   python3 -m http.server 8766 --bind 127.0.0.1
   ```

   Sem Python, com Node: `npx serve -l 8766 preview` (baixa o pacote `serve` na primeira vez, então precisa de internet só nessa vez).

2. Abra no navegador: `http://localhost:8766/index.html`

   Não abra o `index.html` com duplo clique (`file://`): o navegador bloqueia o motor de áudio
   (AudioWorklet) fora de `http://` ou `https://`.

3. **Toque numa tecla da tela** (ou aperte uma tecla do computador) para ligar o áudio. O
   navegador só libera som depois de um gesto seu. A lâmpada **Áudio** acende e os vúmetros mexem.

4. Escolha um programa em **Programas** e toque.

### Tocar com o CDP-160S

1. Ligue o piano ao computador com o cabo USB-B (porta *USB to Host* do piano) e deixe-o ligado.
2. Abra o painel no **Chrome** e aceite o pedido de acesso MIDI, se aparecer.
3. A lâmpada **MIDI** pisca ao tocar, e o visor mostra "Piano MIDI conectado".
4. Velocity funciona em todos os instrumentos (a curva é ajustável no painel Performance).
   O pedal sustain funciona no piano elétrico; o órgão, o clavinet e o Moog ignoram o pedal,
   como os originais.

Se o piano não aparecer: feche outros programas que usem o MIDI (DAW, Chrome em outra aba),
desligue e ligue o cabo e recarregue a página. Em Linux, `aconnect -l` lista os dispositivos MIDI.

### Teclado do computador

`A W S E D F T G Y H U J K` tocam uma oitava a partir da nota mostrada ao lado dos botões `−` e `+`.

### Controles

| Onde | O que faz |
|---|---|
| **Teclado e partes** | Escolha *Normal*, *Camada* ou *Dividido*. Em *Dividido*, `−` e `+` movem o ponto de divisão (a divisão vale pela tecla que você aperta, mesmo com transposição). `Parte A` e `Parte B` escolhem qual parte os programas e knobs estão editando. Ao ligar a Camada ou o Divisão pela primeira vez, a parte B começa com um programa sugerido (Piano elétrico, ou Órgão se a A já for piano). |
| **Programas** | 17 presets (Hammond, Clavinet, Piano elétrico, Moog). Escolher um troca o instrumento da parte que está sendo editada. |
| **Memórias** | `Gravar` e depois `M1` a `M8` guarda **tudo**: modo, ponto de divisão e as duas partes. `M1` a `M8` sozinho chama. Ficam no navegador (`localStorage`), por aparelho. Memórias gravadas na versão anterior (só uma parte) não são lidas. |
| Painel do instrumento | Drawbars e percussão (órgão), captador e wah (clavinet), tine e bark (piano), filtro e envelopes (Moog). Arraste um knob na vertical; duplo clique volta ao valor do preset. |
| **Efeitos** | Drive, Leslie (botão troca lenta/rápida), phaser, chorus, eco de fita, reverb, volume. |
| **Performance** | Roda de pitch (±2 semitons, volta ao centro), roda de mod (vibrato), transposição ±12, curva de velocity, `Panic`. |
| Visor | Mostra o programa e, por 1,4 s, o valor do knob que você mexe. |

### Se você mudar a interface

O arquivo que se edita é `src/ui.h`. A página `preview/index.html` é gerada dele: o trecho entre
`R"HTML(` e `)HTML"`, com uma linha `<script src="shim.js"></script>` antes do primeiro `<script>`.
Para gerar de novo, na pasta do projeto:

```bash
awk '/R"HTML\(/{sub(/.*R"HTML\(/,"");f=1} /\)HTML";/{f=0} f' src/ui.h | sed 's|<script>|<script src="shim.js"></script>\n<script>|' > preview/index.html
```

Depois recarregue a página (Ctrl+F5). Se mexer em `preview/engine.js`, só recarregue.

### Problemas comuns

| Sintoma | Causa e solução |
|---|---|
| Tela aparece mas não sai som | Falta o primeiro toque. Toque numa tecla da tela. A lâmpada **Áudio** tem que acender. |
| "desconectado" / lâmpada Link apagada | O servidor da etapa 1 não está rodando. |
| Piano toca mas sem som | Clique na página uma vez (o navegador exige gesto) e toque de novo. |
| Mudou `engine.js` e nada mudou | Cache. Recarregue com Ctrl+F5. |
| Notas muito altas ou baixas demais | Troque a oitava com `−` e `+`, ou veja a transposição em **Performance**. |

### No ESP32

Ainda **não funciona com o motor novo**. O `src/main.cpp` tem o motor antigo e entende só parte do
protocolo (veja abaixo). Gravar o firmware e abrir `http://192.168.4.1` (Wi-Fi `SynthCDP`, senha
`synth1234`) mostra a mesma página, mas só `on`/`off` e alguns parâmetros fazem efeito, com o som
do motor antigo. A porta do motor novo para C++ é a próxima etapa.

## O que tem

- **Instrumentos:** Hammond (9 drawbars, percussão, key click), Clavinet (corda, captador, wah), piano elétrico FM (tine, bark), Moog (ladder 4 polos, glide, mono).
- **Efeitos:** drive de válvula, Leslie (rotor lento/rápido), phaser, chorus, eco de fita, reverb, tremolo estéreo.
- **Duas partes (A e B) e modos de teclado:** *Normal* (só A), *Camada* (A e B tocam juntas) e *Dividido* (A abaixo do ponto de divisão, B a partir dele; a marca vermelha no teclado da tela mostra o ponto). Cada parte tem instrumento, ajustes e efeitos próprios; só o reverb é compartilhado.
- **Performance** (o CDP-160S não tem): roda de pitch (±2 st, volta ao centro), roda de mod (vibrato), transposição ±12, curva de velocity, panic.
- **Memórias:** 8 posições suas, gravadas no navegador (`localStorage`). `GRAVAR` + `M1..M8` grava, `M1..M8` chama.
- **Vúmetros e lâmpadas:** nível L/R, link, áudio, MIDI, pedal.

Os presets são aproximações do caráter de cada instrumento, não cópias de patches.

## Protocolo página <-> motor (o firmware precisa falar o mesmo)

Página para o motor: `part:<0|1>` escolhe a parte; `e:<instrumento>` e `p:<parâmetro>:<0..1>` valem para a parte escolhida
(`bend`, `mod` e `vcurve` são globais); `mode:<0|1|2>` (normal, camada, dividido); `split:<nota>`;
`on:<nota>:<vel>`, `off:<nota>`, `tr:<semitons>`, `panic`.
Motor para a página: `v:<nívelL>:<nívelR>` (vúmetro), `a:1` (atividade MIDI), `s:<0|1>` (pedal).

## Ligações do DAC (PCM5102A)

Os pinos em `src/main.cpp` (4, 5, 6) são um palpite. No repositório anterior, no commit `61711f5`,
a pinagem validada no seu circuito era BCK 27, WS 25, DOUT 26 (ESP32 clássico). Confira antes de ligar.

SCK em GND, FMT em GND, XSMT em 3V3 (confira a placa).

## Próximas etapas

1. Ouvir e ajustar os presets (os valores foram medidos, não ouvidos).
2. Portar `engine.js` para C++ no ESP32 (44,1 kHz, `fastTanh`, tabela de seno). Atenção à memória: o eco usa dois buffers de 1 s.
3. USB host MIDI (`usb_host` do ESP-IDF + driver de classe MIDI).
