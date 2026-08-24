# PATCH V29 — controle manual de B/s + leitura de sinais mais rápida

## Alterações
- adicionada configuração `manualSendRateLimitBps` no `MachineConfig`
- adicionada configuração `signalPollIntervalMs` no `MachineConfig`
- UI de Settings agora expõe:
  - **Taxa envio manual (B/s)**: `0 = Auto`
  - **Leitura sinais** em milissegundos
- `DncEngine` agora aplica limitador manual por token bucket no lado da aplicação
- `HandshakeMonitor` agora usa timer preciso e também força amostragem imediata em `readyRead` e `bytesWritten`
- início do monitor de handshake respeita `signalPollIntervalMs`
- janela local manual ficou mais curta para reduzir overshoot

## Objetivo
Dar ao operador um freio manual reproduzível para conter overflow em CNC mais sensível, sem depender de um drip feed separado, e melhorar a rapidez com que CTS/DSR e demais sinais são percebidos pelo motor.

## Observação
A limitação manual de B/s atua no lado do software. Ela reduz agressividade de envio, mas não substitui XON/XOFF ou RTS/CTS quando a máquina os oferece.
