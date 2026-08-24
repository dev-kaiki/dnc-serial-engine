# v32b anti-gag patch

Correção aplicada para reduzir gargalo progressivo no meio do programa em modo remoto/manual.

## Causa atacada
O motor estava ficando agressivo demais conforme a sessão avançava:
- crescimento de chunk rápido demais em modo manual (`XON/XOFF` / `CTS`)
- janela local de bytes pendentes larga demais
- retomada pós-`XON` ainda curta para máquinas com buffer mais sensível

Na prática, ele começava bem e depois passava a alimentar a UART/stack mais do que a CNC estava conseguindo drenar com estabilidade. Isso explica o padrão relatado: no início vai bem, no meio começa a gargalar.

## Mudanças
- `TxScheduler`
  - chunk inicial manual mais conservador
  - chunk máximo manual reduzido
  - crescimento manual só ocorre após volume estável suficiente
  - crescimento em passos menores
  - downshift mais agressivo quando o `XOFF` volta cedo
  - high-water interno do scheduler reduzido
- `DncEngine`
  - high-water local reduzido
  - janela de recuperação pós-`XON`/`CTS HIGH` alongada
  - recuperação pós-`XOFF` mais conservadora, com mais chunks pequenos antes de voltar a crescer

## Expectativa prática
- menos oscilação ao longo do programa
- menos "começa bem e depois afoga"
- taxa média mais estável, mesmo que o pico bruto fique um pouco menor

Esse patch prioriza estabilidade industrial sobre pico instantâneo.
