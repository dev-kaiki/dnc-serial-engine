# PATCH V30 — Strict pacing para conter overflow sem burst grosseiro

## Alterações principais
- `manualSendRateLimitBps` agora usa micro-bursts de 1 a 4 bytes, em vez de bucket com burst grande.
- `QSerialPort::setWriteBufferSize()` passa a ser reduzido também quando houver taxa manual configurada.
- `pumpTx()` trata taxa manual como **pacing estrito**, mesmo sem XON/XOFF ou CTS/RTS.
- `TxScheduler` em pacing estrito cresce apenas até micro-bursts curtos.
- `FlowController` volta a respeitar `requireXonToSend` quando explicitamente habilitado.

## Efeito esperado
- Menos overshoot em CNC com buffer pequeno.
- Menor chance de overflow no modo sem controle.
- Throughput mais estável, sem burst grande seguido de hold.

## Limitação real
Sem feedback da máquina, a taxa segura ainda depende de calibração por CNC. Esta versão melhora o controle do lado do software; não cria telemetria que o cabo não fornece.
