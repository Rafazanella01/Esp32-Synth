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
| **Sequenciador** | `Abrir .mid` carrega um arquivo; `Tocar`/`Pausar`/`Parar`; clique na barra para pular; `−`/`+` mudam o andamento (25% a 200%); `Repetir` faz laço; `Gravar` grava o que você toca (tela, teclado do computador ou piano) e `Salvar .mid` exporta. Cada faixa do arquivo pode ir para Teclado (segue o modo atual), Parte A, Parte B, A + B ou ficar desligada. |
| **Teclado e partes** | Escolha *Normal*, *Camada* ou *Dividido*. Em *Dividido*, `−` e `+` movem o ponto de divisão (a divisão vale pela tecla que você aperta, mesmo com transposição). `Parte A` e `Parte B` escolhem qual parte os programas e knobs estão editando. Ao ligar a Camada ou o Divisão pela primeira vez, a parte B começa com um programa sugerido (Piano elétrico, ou Órgão se a A já for piano). |
| **Programas** | 21 presets (Hammond, Clavinet, Piano elétrico, Moog, Cordas). Escolher um troca o instrumento da parte que está sendo editada. |
| **Memórias** | `Gravar` e depois `M1` a `M8` guarda **tudo**: modo, ponto de divisão e as duas partes. `M1` a `M8` sozinho chama. Ficam no navegador (`localStorage`), por aparelho. Memórias gravadas na versão anterior (só uma parte) não são lidas. `Exportar` salva as 8 memórias num arquivo `.json` (backup, ou para levar a outro aparelho) e `Importar` carrega esse arquivo, substituindo as memórias atuais. |
| **Arpejador e metrônomo** | `Arpejador` liga; toque um acorde e as notas saem uma a uma. Padrões: sobe, desce, sobe e desce, aleatório, ordem tocada e acorde. `Hold` mantém o arpejo ao soltar as teclas (o pedal sustain também segura). Ajuste andamento (BPM), divisão (1/4 a 1/32, com tripletos), oitavas (1 a 4) e duração das notas (gate). A primeira nota toca na hora, sem esperar a grade. O relógio roda dentro do motor de áudio, então não depende da rede. `Metrônomo` dá um clique no andamento (o primeiro tempo de cada quatro é mais agudo). |
| Painel do instrumento | Drawbars e percussão (órgão), captador e wah (clavinet), tine e bark (piano), filtro e envelopes (Moog). Arraste um knob na vertical; duplo clique volta ao valor do preset. |
| **Efeitos** | Drive, Leslie (botão troca lenta/rápida), phaser, chorus, eco de fita, reverb, volume. |
| **Performance** | Roda de pitch (±2 semitons, volta ao centro), roda de mod (vibrato), transposição ±12, curva de velocity, `Panic`. |
| Visor | Mostra o programa e, por 1,4 s, o valor do knob que você mexe. |

### Arquivos MIDI

- **Abrir:** `Abrir .mid` (formatos 0 e 1; até 3 MB). Lê notas, velocity, pedal sustain e mudanças de andamento. Ignora troca de programa, pitch bend e controladores: o instrumento de cada faixa é o que você escolher nas partes A e B, não o que o arquivo pede.
  A faixa de percussão (canal 10) vem desligada, porque o painel não tem bateria.
- **Tocar:** o andamento do arquivo é respeitado. A velocidade vai de 25% a 200%.
- **Gravar:** `Gravar`, toque, `Gravar` de novo. A gravação vira uma faixa tocável e pode ser salva.
- **Salvar:** gera um `.mid` de uma faixa só, a 120 BPM, com as notas, a velocity e o pedal. Se você salvar um arquivo que abriu, as faixas são juntas numa só (as desligadas ficam de fora).
- **Biblioteca (domínio público):** botões para Ode à Alegria (Beethoven), Parabéns pra Você, Jingle Bells (refrão), Canon em Ré (trecho, Pachelbel) e a abertura da Tocata e Fuga em Ré menor (Bach). Cada uma carrega os instrumentos nas partes A e B e já começa a tocar. `Tocar todas` toca as cinco em sequência (`Parar` cancela). Foram escritas de memória: as notas e os compassos foram conferidos por teste, mas a harmonização, o baixo e a Tocata são interpretações aproximadas.
- **Demo original:** uma peça curta que escrevi (mi menor, 150 BPM, órgão e baixo), só para testar o tocador. Não é cópia de nenhuma música.
- **Para tocar uma música sua** (por exemplo Highway Star do Deep Purple), carregue o seu próprio arquivo `.mid` da música. O repositório não traz transcrições de músicas protegidas.

O tocador roda na página e agenda cada nota com 120 ms de antecedência, com tempo exato no motor. No ESP32 isso precisa virar um sequenciador dentro do firmware: mandar nota por nota por Wi-Fi tem jitter demais.

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

- **Instrumentos:** Hammond (9 drawbars, percussão, key click), Clavinet (corda, captador, wah), piano elétrico FM (tine, bark), Moog (ladder 4 polos, glide, mono), cordas de conjunto estilo Solina (serras com detune, vibrato lento, ataque e release ajustáveis; o chorus de conjunto vem dos efeitos).
- **Arpejador e metrônomo:** seis padrões, 1 a 4 oitavas, divisões de 1/4 a 1/32 com tripletos, hold, gate e andamento de 40 a 240 BPM.
- **Efeitos:** drive de válvula, Leslie (rotor lento/rápido), phaser, chorus, eco de fita, reverb, tremolo estéreo.
- **Duas partes (A e B) e modos de teclado:** *Normal* (só A), *Camada* (A e B tocam juntas) e *Dividido* (A abaixo do ponto de divisão, B a partir dele; a marca vermelha no teclado da tela mostra o ponto). Cada parte tem instrumento, ajustes e efeitos próprios; só o reverb é compartilhado.
- **Sequenciador de arquivos MIDI:** abre e toca `.mid`, grava o que você toca e salva em `.mid`. Veja a seção abaixo.
- **Performance** (o CDP-160S não tem): roda de pitch (±2 st, volta ao centro), roda de mod (vibrato), transposição ±12, curva de velocity, panic.
- **Memórias:** 8 posições suas, gravadas no navegador (`localStorage`). `GRAVAR` + `M1..M8` grava, `M1..M8` chama. `Exportar` e `Importar` fazem backup em arquivo.
- **Vúmetros e lâmpadas:** nível L/R, link, áudio, MIDI, pedal.

Os presets são aproximações do caráter de cada instrumento, não cópias de patches.

## Protocolo página <-> motor (o firmware precisa falar o mesmo)

Página para o motor: `part:<0|1>` escolhe a parte; `e:<instrumento>` e `p:<parâmetro>:<0..1>` valem para a parte escolhida
(`bend`, `mod` e `vcurve` são globais); `mode:<0|1|2>` (normal, camada, dividido); `split:<nota>`;
`on:<nota>:<vel>`, `off:<nota>`, `tr:<semitons>`, `panic`;
arpejador e metrônomo (globais): `arp:<on>:<padrão 0-5>:<oitavas 1-4>:<divisão 1,2,3,4,6,8>:<gate 0-1>:<hold>`, `tempo:<bpm>`, `met:<on>:<volume 0-1>`;
agendados (o último número é o atraso em ms a partir do recebimento): `son:<parte|-1>:<nota>:<vel>:<ms>`, `soff:<parte|-1>:<nota>:<ms>`, `ssus:<0|1>:<ms>`; `sclear` cancela os agendados (`-1` segue o modo do teclado).
Motor para a página: `v:<nívelL>:<nívelR>` (vúmetro), `a:1` (atividade MIDI), `s:<0|1>` (pedal), `k:<nota>:<vel>` (tecla do piano; vel 0 = solta).

## Modelos dos instrumentos (o que é circuito e o que é aproximação)

O som vem de modelos dos circuitos e da física de cada instrumento, não de amostras. Cada ponto abaixo foi medido
com testes offline; nenhum foi comparado com gravação do instrumento real.

- **Órgão de tonewheels:** onda do tonewheel com 2ª e 3ª harmônicas pequenas (-34 e -41 dB), vazamento dos tonewheels vizinhos (-55 dB),
  key click, percussão de disparo único que corta o drawbar de 1', **scanner de vibrato/chorus** (6,8 Hz, até ±18 cents),
  pré-amplificador de válvula com saturação assimétrica e sag da fonte, e **Leslie** com rotores que aceleram rápido e freiam devagar
  (corneta e tambor com inércias diferentes). A afinação dos tonewheels é temperada exata; a real tem pequenos desvios das engrenagens, não modelados.
- **Piano elétrico:** três modos de vibração da tine (1 : 6,27 : 17,5, como uma viga em balanço) e **captador eletromagnético**
  que vê a velocidade da tine e muda de ganho com a distância ao ímã. Quanto mais forte a tecla, mais "mordida"
  (2ª harmônica de -19 dB a -8 dB), e o "sino" agudo some em menos de 0,6 s. Mais baque do martelo. A tine real tem
  inharmonicidade própria por nota que não está modelada.
- **Moog:** filtro ladder de 4 estágios com saturação (tanh) em cada estágio e sobreamostragem 2x (-30 dB/oitava medido, ressonância +12 dB),
  osciladores anti-aliasing com deriva lenta de até ±2,5 cents.
- **Cordas:** três serras com detune (±0 a 16 cents) mais uma serra uma oitava acima, com anti-aliasing (polyBLEP), vibrato lento de 5,3 Hz com fase própria por voz, e filtro passa-baixa de dois polos cujo corte segue a nota. É um modelo do conjunto (ensemble) de órgão de cordas dos anos 70, sem os divisores de frequência do circuito original.
- **Clavinet:** corda por Karplus-Strong com perdas do laço compensadas, captadores em fase ou fora de fase,
  baque do martelo e clique do abafador. Não modela o ruído mecânico do instrumento nem as chaves de timbre do D6.

## Ligações do DAC (PCM5102A)

Os pinos em `src/main.cpp` (4, 5, 6) são um palpite. No repositório anterior, no commit `61711f5`,
a pinagem validada no seu circuito era BCK 27, WS 25, DOUT 26 (ESP32 clássico). Confira antes de ligar.

SCK em GND, FMT em GND, XSMT em 3V3 (confira a placa).

## Próximas etapas

1. Ouvir e ajustar os presets (os valores foram medidos, não ouvidos).
2. Portar `engine.js` para C++ no ESP32 (44,1 kHz, `fastTanh`, tabela de seno). Atenção à memória: o eco usa dois buffers de 1 s.
3. USB host MIDI (`usb_host` do ESP-IDF + driver de classe MIDI).
