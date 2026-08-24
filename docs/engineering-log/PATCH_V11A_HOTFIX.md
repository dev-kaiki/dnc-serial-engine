Hotfix v11a
- Reverte mudanças do v11 que afetavam recepção (polling agressivo global e defaults excessivamente conservadores na UI)
- Mantém apenas as mudanças de envio imediato: TxScheduler conservador em XON/XOFF e CTS/RTS, pump imediato em XON/CTS e bloqueio de novo chunk enquanto houver bytes pendentes.
- Handshake polling em recepção volta a 20 ms; 2 ms só quando for envio com CTS.
- Settings defaults preservados do v10.
