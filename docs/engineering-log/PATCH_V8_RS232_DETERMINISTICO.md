# Patch V8 - RS232 determinístico

## Ajustes aplicados
- RTS/CTS manual determinístico: `requireCtsHighToSend` agora abre a porta com `NoFlowControl` e força RTS alto manualmente.
- Conclusão de envio exige que todos os bytes tenham sido aceitos pela porta antes da drenagem poder concluir.
- Recepção com auto-finalização por silêncio RS232 (`receiveAutoFinishOnSilence`, default 1500 ms).
- Botão `Finalizar` adicionado na tela de recepção.
- `sessionFinished` agora carrega `SessionResult` com modo e caminho do arquivo, garantindo popup correto.
- `abort()` não reclassifica mais uma sessão já concluída.
