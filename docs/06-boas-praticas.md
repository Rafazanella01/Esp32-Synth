# Boas práticas do projeto

Convenções seguidas neste repositório. O objetivo é manter o projeto fácil
de retomar depois de um tempo parado, e fácil de estender sem quebrar o
que já funciona.

## Estrutura de pastas

```
include/   - interface pública de cada módulo (.h), o "contrato"
src/       - implementação (.cpp) + os arquivos main de cada ambiente
docs/      - toda a documentação além do essencial do README
```

Um módulo novo sempre ganha um par `include/Nome.h` + `src/Nome.cpp`,
mesmo que pequeno — mantém a mesma forma dos módulos existentes
(`Keypad`, `Input`, `Display`, `Synth`) e deixa claro qual é a interface
pública (o `.h`) vs. detalhe de implementação (o `.cpp`).

Exceção: dados puros sem lógica (`Chords.h`, partes de `Waveforms.h`)
podem ficar só no header quando não há estado para inicializar.

## Nomenclatura

- **Pinos**: sempre `PIN_*` ou `*_PINS` em `include/Config.h`, nunca um
  número de GPIO solto em outro arquivo
- **Constantes de tuning** (BPM, sample rate, debounce, tamanhos de
  buffer): também centralizadas em `Config.h`, em MAIÚSCULAS
- **Funções públicas de módulo**: prefixadas com o nome do módulo em
  minúsculo — `synthNoteOn`, `keypadUpdate`, `displayUpdate` — pra
  ficar óbvio de qual módulo é cada função ao ler `main.cpp`
- **Getters de estado contínuo** (`getTempoBPM`, `getJoystickChordType`)
  vs. **eventos de disparo único** (`keypadJustPressed`, que zera o
  pendente ao ser lido): o `Just` no nome marca esse segundo
  comportamento, porque é fácil de usar errado — chamar duas vezes no
  mesmo ciclo esperando o mesmo resultado devolve `-1` na segunda

## Ambientes PlatformIO

Cada subsistema de hardware tem um ambiente de teste isolado
(`test_i2s`, `test_lcd`, `test_keypad`, `test_joystick`) além do ambiente
principal (`esp32dev`), usando `build_src_filter` pra escolher qual
arquivo com `setup()/loop()` entra na build — ver `platformio.ini`.

**Ao criar um módulo de hardware novo:**
1. Criar `include/NomeDoModulo.h` + `src/NomeDoModulo.cpp` com a lógica
2. Criar `src/bringup_nomedomodulo.cpp` com um teste mínimo e independente
   (Serial print do que está lendo, sem depender de outros módulos)
3. Adicionar um `[env:test_nomedomodulo]` no `platformio.ini`, excluindo
   `main.cpp` e os outros `bringup_*.cpp`
4. Documentar o passo de bring-up correspondente em
   [`05-montagem.md`](05-montagem.md)

Isso mantém o padrão de "montar e validar incrementalmente" conforme o
projeto cresce, em vez de só descobrir que algo quebrou quando tudo já
está montado.

## Commits

Mensagens curtas, em português, descrevendo o *porquê*/*o que muda* (não
uma lista de arquivos — isso o diff já mostra). Um commit por unidade de
trabalho coerente (ex: "adiciona sketches de bring-up" foi um commit
separado do firmware principal).

## Documentação

- **README.md**: só o essencial — o que é o projeto, quick start de build,
  e links pra cá. Não deve crescer indefinidamente.
- **docs/**: tudo além do essencial, um arquivo por assunto (ideia,
  arquitetura, hardware, firmware, montagem, boas práticas — esta lista).
- **CHANGELOG.md**: atualizado a cada sessão de trabalho com o que mudou,
  seguindo o formato [Keep a Changelog](https://keepachangelog.com/pt-BR/).
  Isso é o que permite retomar o projeto depois de semanas sem precisar
  reler todo o histórico do git.
- Ao mudar uma decisão registrada em [`01-ideia.md`](01-ideia.md) (ex:
  trocar layout da matriz, mudar o que cada potenciômetro faz), atualizar
  o doc junto — ele existe pra refletir o estado *atual* das decisões, não
  só o histórico.

## Ao propor uma mudança de escopo/decisão

Se uma decisão registrada em [`01-ideia.md`](01-ideia.md) precisar mudar
(por exemplo, trocar o layout da matriz ou adicionar um modo de acorde em
bloco), o fluxo é:
1. Atualizar a tabela de decisões em `01-ideia.md` com a nova escolha e o
   motivo
2. Ajustar o código afetado
3. Registrar no `CHANGELOG.md`

Isso evita a documentação ficar desalinhada com o código — problema comum
em projetos hobby que ficam meses parados entre sessões de trabalho.
