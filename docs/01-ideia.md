# A ideia do projeto

## Objetivo

Um sintetizador de acordes baseado em ESP32: você aperta uma nota e ele toca
o acorde correspondente ao tipo selecionado, repetindo/arpejando
continuamente — "fazer arranjos como um sintetizador", nas palavras do
Rafael, o autor do projeto.

## Briefing original (transcrito e organizado)

O projeto nasceu de duas mensagens de voz descrevendo a ideia e depois o
inventário de peças já compradas. Aqui vai o que foi pedido, organizado:

**Funcionalidade musical:**
- Tipos de acorde: **Maior, Menor, Menor com sétima (m7), Aumentado**
  ("todos os movimentos" de acorde relevantes — sem sustenido como tipo à
  parte, sustenido/bemol é só a nota em si)
- O som deve **ficar repetindo**, fazendo arranjos "como um sintetizador" —
  ou seja, um **arpejador**, não apenas um acorde estático em bloco
- Deve ser **versátil**: múltiplos timbres, não só uma onda fixa
- Deve mostrar **mensagens no LCD** (nome da nota, acorde, etc.)

**Inventário de peças (o que já foi comprado):**
- ESP32 (modelo genérico)
- Módulo GY-PCM5102 (DAC PCM5102A) — áudio via I2S
- Matriz de botões 4x4 (8 saídas: R1-R4, C1-C4)
- Display LCD 16x2 com backpack I2C (4 pinos: GND, VCC, SDA, SCL)
- Joystick analógico (VRx, VRy, botão)
- Potenciômetros de 10K
- LM358 (amplificador operacional duplo)
- Capacitores e transistores diversos
- Jacks P10 (saída de áudio, e possivelmente entrada futura pra plugar
  instrumento)

## Decisões tomadas para resolver ambiguidades

O briefing veio por transcrição de voz, com alguns termos técnicos
distorcidos. Aqui está como cada ambiguidade foi resolvida, e por quê:

| Ambiguidade | Interpretação adotada | Motivo |
|---|---|---|
| "i dois c" | Podia ser I2C ou I2S. Acabou sendo **os dois**: I2S pro DAC PCM5102A, I2C pro LCD | O inventário de peças confirmou o LCD com backpack I2C (4 pinos) |
| "entrada p dez" | Inicialmente lido como pinos numéricos; depois esclarecido como **matriz 4x4** de botões | O usuário confirmou diretamente ao ser perguntado |
| Quantidade de tipos de acorde | Hoje são 11 qualidades (ver tabela abaixo) | Começou com 4, mas o joystick tem 10 posições úteis — usar só 4 desperdiçaria o controle |
| Layout da matriz 4x4 (16 teclas) | **As 16 teclas são notas** (C4 a D#5) | O tipo de acorde migrou pro joystick, o que liberou as 4 teclas que antes eram MAJ/MIN/MIN7/AUG |
| "queria que ficasse repetindo" | Arpejo é um dos dois modos de toque, não o único | Ver a seção "Como o toque funciona" abaixo |
| "mais timbres... bem versátil" | 4 formas de onda com **morph contínuo** entre elas (não troca em degrau) + glide entre notas | O usuário pediu explicitamente transições fluidas em tudo |

## Como o toque funciona (o desenho central)

Esta é a decisão mais importante do projeto, e chegou depois de algumas
voltas: **segurar e tocar rápido não mudam o acorde, mudam o jeito de
tocar.**

| | Joystick no centro | Joystick numa direção |
|---|---|---|
| **Segurando a tecla** | nota pura, sustentada | aquele acorde **em bloco**, sustentado |
| **Toque rápido** | acorde Maior **arpejado** | aquele acorde **arpejado** |

O joystick escolhe *qual* acorde nos dois modos, ao vivo — dá pra mexer
nele com a nota já soando. As 10 posições:

| Posição | Acorde | Posição | Acorde |
|---|---|---|---|
| centro | Maior | baixo (S) | Menor7 |
| cima (N) | Maior7 | baixo-esquerda (SW) | Menor |
| cima-direita (NE) | Dominante7 | esquerda (W) | Diminuto |
| direita (E) | Aumentado | cima-esquerda (NW) | Sus2 |
| baixo-direita (SE) | Sus4 | clique | Sexta |

Tocar de novo a tecla que está arpejando desliga o som.

Essas decisões estão implementadas no firmware atual, mas **não são
definitivas** — qualquer uma pode ser revista se não bater com o que o
Rafael imaginou na prática. Ver [`06-boas-praticas.md`](06-boas-praticas.md)
para como propor mudanças de forma organizada.

## Escopo consciente deixado de fora (por enquanto)

- Entrada de áudio externa pelo 2º jack P10 (para plugar um instrumento e
  processar o sinal) — mencionado na ideia original, mas exigiria um
  estágio de condicionamento de sinal (atenuação + bias) que não foi
  detalhado ainda
- Salvar configurações (último timbre, BPM) entre reinicializações
- Polifonia entre teclas diferentes — o acorde sustentado já toca várias
  notas juntas, mas só de uma tecla por vez
