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
| Quantidade de tipos de acorde | 4 tipos: Maior, Menor, Menor7, Aumentado | O usuário corrigiu explicitamente removendo "sustenido" como tipo |
| Layout da matriz 4x4 (16 teclas) | 12 teclas = nota fundamental (cromática, uma oitava) + 4 teclas = tipo de acorde | Encaixa exatamente nos 16 botões disponíveis e cobre os 4 tipos pedidos |
| "queria que ficasse repetindo" | Modo principal de toque é **arpejo contínuo** (não acorde em bloco estático) | É a leitura mais direta de "fazer arranjos como sintetizador" — o recurso clássico de arpejador |
| "mais timbres... bem versátil" | 4 formas de onda selecionáveis (seno, triangular, dente-de-serra, quadrada) + joystick pra pitch-bend/navegação + potenciômetros pra tempo/timbre | Sem instrução exata de quais controles fazem o quê, essa foi a distribuição que melhor aproveita as peças que o usuário já tem |

Essas decisões estão implementadas no firmware atual, mas **não são
definitivas** — qualquer uma pode ser revista se não bater com o que o
Rafael imaginou na prática. Ver [`06-boas-praticas.md`](06-boas-praticas.md)
para como propor mudanças de forma organizada.

## Escopo consciente deixado de fora (por enquanto)

- Acorde em bloco (todas as notas soando juntas, sem arpejo) como modo
  alternativo — hoje só existe o modo arpejado
- Entrada de áudio externa pelo 2º jack P10 (para plugar um instrumento e
  processar o sinal) — mencionado na ideia original, mas exigiria um
  estágio de condicionamento de sinal (atenuação + bias) que não foi
  detalhado ainda
- Salvar configurações (último timbre, BPM) entre reinicializações
