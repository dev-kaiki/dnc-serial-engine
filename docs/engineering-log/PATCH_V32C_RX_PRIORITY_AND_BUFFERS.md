# PATCH V32C - Prioridade de RX e buffers reais

## Objetivo
Reduzir gargalo no meio do programa priorizando leitura de RX antes de novos writes e limitando os buffers do stack serial em modos manuais (XON/XOFF e CTS/RTS).

## Alterações
- `DncEngine` agora drena RX imediatamente (`drainSerialRxImmediate`) tanto no callback `readyRead` quanto antes de decidir novos writes.
- Inclusão de guarda curta de silêncio após RX para que `XOFF`, `XON` e mudanças de CTS sejam processados antes do próximo burst.
- Janela local agora considera não só `bytesToWrite()` do Qt, mas também backlog efetivo (`bytesQueued - bytesAcceptedByPort`).
- `SerialPortFacade` passou a limitar `readBufferSize` e `writeBufferSize` de forma mais conservadora em fluxo manual.
- Valores padrão de `readBufferLimit` / `writeBufferLimit` reduzidos para 256 bytes.
- `SettingsPage` recalcula buffers menores quando o perfil está em XON/XOFF ou CTS/RTS.

## Resultado esperado
- resposta mais rápida ao `XOFF`
- menos overshoot depois de `CTS LOW`
- menos crescimento de backlog invisível no driver
- taxa menos bonita no pico, mas mais estável no meio e no fim
