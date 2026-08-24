# PATCH V9 - Correção do erro 58 / Timeout no envio

## Causa corrigida
O motor de envio estava chamando `waitForBytesWritten(20)` dentro do loop assíncrono de drenagem.
Em `QSerialPort`, essa API é bloqueante e pode retornar `TimeoutError`, disparando `errorOccurred`
mesmo quando o cenário correto seria apenas continuar aguardando `bytesWritten()`.

## Alterações aplicadas
- Removido `waitForBytesWritten(20)` do estado `Draining` em `core/dnc/DncEngine.cpp`
- Removido `waitForBytesWritten(50)` do fechamento por sucesso em `core/dnc/DncEngine.cpp`
- `SerialPortFacade` agora ignora `QSerialPort::TimeoutError` em `errorOccurred`

## Efeito esperado
- não deve mais ocorrer `Sending -> Draining -> Fault | O tempo limite de espera foi atingido.`
- o envio deve permanecer em `Draining` até receber `bytesWritten()` suficientes e estabilizar
- `Completed` só deve acontecer quando `bytesAcceptedByPort >= fileBytesTotal` e `bytesToWrite == 0`
