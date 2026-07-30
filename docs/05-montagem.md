# Guia de montagem (bring-up incremental)

Não monte tudo de uma vez. Vá subsistema por subsistema: monta, grava o
teste daquele subsistema, confirma que funciona, só então parte pro
próximo. Assim, se algo falhar, dá pra saber exatamente onde procurar.

Cada teste é um "ambiente" separado do PlatformIO (não mexe no firmware
principal — ver [`06-boas-praticas.md`](06-boas-praticas.md#ambientes-platformio)
pra como isso é organizado). No VSCode com a extensão PlatformIO: aba do
formigueiro (PIO) na lateral > `Project Tasks` > escolha o ambiente
(`test_i2s`, `test_lcd` etc.) > `Upload`. Ou pelo terminal:

```bash
pio run -e test_i2s -t upload
pio run -e test_lcd -t upload
pio run -e test_keypad -t upload
pio run -e test_joystick -t upload
pio device monitor          # abre o Serial Monitor pra ver os testes com print
```

## Passo 0 — ESP32 sozinho

Antes de ligar qualquer módulo, plugue só o ESP32 no USB e confirme no
computador que ele aparece como porta serial (`pio device list`). Se não
aparecer, é driver USB (CP2102/CH340) faltando — resolver isso antes de
continuar.

## Passo 1 — Áudio (PCM5102A)

Ligue *só* o módulo PCM5102A conforme a tabela de I2S em
[`03-hardware.md`](03-hardware.md#i2s--pcm5102a-áudio) (não esquecer o
XSMT em 3.3V!). Grave `test_i2s`. Deve sair um tom de 440Hz contínuo e
limpo no jack. Sem chiado, sem silêncio. Só depois disso funcionar direito,
prosseguir.

## Passo 2 — Display (LCD I2C)

Ligue o LCD (SDA/SCL/VCC/GND). Grave `test_lcd` e abra o Serial Monitor:
ele escaneia o barramento I2C e mostra o endereço encontrado — confirme
que bate com `LCD_I2C_ADDR` em `include/Config.h` (padrão `0x27`; se o
scan achar `0x3F`, troque lá). O LCD deve mostrar "LCD OK!" e um contador
incrementando.

## Passo 3 — Matriz de botões

Ligue as 8 linhas/colunas da matriz. Grave `test_keypad`, abra o Serial
Monitor e aperte as 16 teclas uma por uma, conferindo se o nome impresso
bate com a tecla física apertada. Se alguma tecla não responder ou
responder com o nome errado, o problema está na linha/coluna dela
especificamente — mais fácil de achar testando isolado assim.

## Passo 4 — Joystick + potenciômetros

Ligue o joystick e os dois potenciômetros (tempo/timbre). Grave
`test_joystick` e observe os números no Serial Monitor enquanto mexe em
cada controle — devem variar suavemente entre ~0 e ~4095, e o centro do
joystick deve ficar perto de ~2048.

## Passo 5 — Firmware completo

Com os 4 passos acima validados, grave o ambiente principal
(`pio run -e esp32dev -t upload`, ou apenas `pio run -t upload`). Agora sim
toque nas teclas e confira o sintetizador funcionando de ponta a ponta.

## Passo 6 — Estágio analógico de saída (LM358)

Por último, monte o buffer com o LM358 e o potenciômetro de volume
(circuito em [`03-hardware.md`](03-hardware.md#estágio-de-saída-de-áudio-lm358--jack-p10))
entre a saída do PCM5102A e o jack P10. Deixe esse por último porque é a
parte mais fácil de errar (polaridade de capacitor eletrolítico, pinagem
do LM358) — e com o Passo 1 já validado, dá pra saber que se o som sumir
depois de montar o buffer, o problema está no buffer, não no resto.

> Dica geral: tenha um multímetro por perto. Antes de plugar qualquer
> módulo novo, meça se o pino de alimentação dele está mesmo recebendo
> 3.3V (ou 5V, conforme o módulo) e não algo diferente por engano de
> fiação.

## Se algo não funcionar

Veja qual passo falhou e mande o que apareceu no Serial Monitor (ou o
sintoma físico — LCD apagado, sem som, tecla fantasma etc.) — cada passo
isola o problema a um único subsistema, então normalmente dá pra
diagnosticar rápido a partir daí.
