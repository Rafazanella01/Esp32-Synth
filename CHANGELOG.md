# Changelog

Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/).
Atualizado a cada sessão de trabalho — ver
[`docs/06-boas-praticas.md`](docs/06-boas-praticas.md#documentação).

## [Não lançado]

### Adicionado
- Documentação completa em `docs/`: ideia original, arquitetura, hardware,
  referência de firmware, guia de montagem e boas práticas
- Este `CHANGELOG.md`
- `.gitignore` (gerado pelo PlatformIO ao abrir o projeto)

### Alterado
- `README.md` reduzido a um índice enxuto, com detalhes movidos pra `docs/`
- `bringup_i2s.cpp` toca a abertura de "Ode à Alegria" em loop em vez de um
  tom fixo de 440Hz — mais fácil de confirmar por ouvido que o áudio
  funciona de verdade

### Validado
- **Passo 1 do guia de montagem (PCM5102A / I2S) confirmado funcionando em
  hardware real** — áudio audível através do jack, testado no
  ambiente `test_i2s`
- **Passo 2 do guia de montagem (LCD I2C) confirmado funcionando em
  hardware real** — endereço `0x27` encontrado no scan (bate com o padrão
  já configurado em `Config.h`), display exibindo texto corretamente,
  testado no ambiente `test_lcd`

## [0.2.0] - 2026-07-29

### Adicionado
- 4 ambientes de bring-up no PlatformIO (`test_i2s`, `test_lcd`,
  `test_keypad`, `test_joystick`) pra validar cada subsistema de hardware
  isoladamente antes de rodar o firmware completo
- Guia de montagem incremental (passo a passo, subsistema por subsistema)

## [0.1.0] - 2026-07-29

### Adicionado
- Firmware inicial do sintetizador de acordes com arpejador
- 4 tipos de acorde (Maior, Menor, Menor7, Aumentado) via matriz de botões 4x4
- Motor de áudio via I2S (PCM5102A) com 4 timbres selecionáveis
  (seno, triangular, dente-de-serra, quadrada)
- Display LCD 16x2 (I2C) com página de status e página de ajuda
- Controle por joystick (pitch-bend, navegação de página, botão de pânico)
  e potenciômetros (andamento/BPM, timbre)
- Esquema de ligação completo e circuito de saída de áudio com buffer LM358
