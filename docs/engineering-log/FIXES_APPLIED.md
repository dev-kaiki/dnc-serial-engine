# Fixes applied

This package includes the following direct fixes over the previous ZIP:

- fixed broken string literals in `core/config/MachineConfig.h`
- fixed broken string literals in `core/config/MachineConfigRepository.cpp`
- fixed broken multiline string literal in `core/storage/SessionTraceRepository.cpp`
- fixed Qt macro collision in `core/serial/SerialPortFacade.cpp` by renaming local variable `signals`
- added explicit `#include <QJsonObject>` in `core/dnc/DncEngine.h`

These were the compile blockers visible from the reported errors.


## v4 - ajustes de responsividade e motor de envio
- Corrigido layout da tela Operação para evitar sobreposição dos botões ao redimensionar a janela.
- Adicionada área rolável na página principal para preservar usabilidade em janela menor.
- Ajustadas políticas de tamanho dos blocos Operação, Recepção, Status e Sinais.
- Sincronização inicial de CTS/DSR corrigida no HandshakeMonitor ao abrir a sessão.
- Migração de configurações legadas para fluxo manual do engine, evitando conflito entre QSerialPort SoftwareControl/HardwareControl e o controle interno do DNC.
- Settings agora salvam fluxo físico como NoFlowControl e deixam o engine interpretar XON/XOFF ou CTS conforme o modo selecionado.


## Revisão adicional do motor de envio (v5)
- XON/XOFF com cabo sem controle agora permanece em `NoFlowControl` no driver e o bloqueio/liberação ocorre no engine via `FlowController`, evitando conflito com buffers do driver.
- RTS/CTS com cabo com controle agora força `HardwareControl` na porta serial e também mantém a supervisão explícita de CTS no engine.
- O estado inicial do envio agora nasce bloqueado quando a configuração exige XON inicial ou CTS alto, eliminando partidas indevidas.
- A leitura e gravação da configuração RS232 na tela de settings foi corrigida para persistir os campos reais do motor.


## V22 - Refill imediato + payload transparente
- Refill imediato no `bytesWritten` em modo manual para eliminar gargalo imposto pelo timer de 5 ms.
- Timer do pump reduzido para 1 ms como fallback, sem depender exclusivamente dele.
- Scheduler manual fixado em janela curta (2-4 bytes por passo, watermark 8), evitando overshoot sem subalimentar a UART.
- Desativada a reescrita do payload no envio: sem normalizar CR/LF, sem `%` automático, sem EOF, sem força ASCII-7.
