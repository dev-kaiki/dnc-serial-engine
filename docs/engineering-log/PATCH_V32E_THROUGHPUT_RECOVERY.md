# PATCH V32E - Throughput recovery

## Causa raiz corrigida
A taxa estava limitada artificialmente pelo próprio scheduler/manual-flow:
- chunk inicial muito baixo
- chunk máximo muito baixo
- janela dinâmica local de apenas 8..20 bytes
- write/read buffer manual sendo reencolhido no open da serial
- recuperação pós-XON/CTS excessivamente conservadora

## Correções
- scheduler manual com chunk inicial maior
- teto manual muito mais compatível com 19200 bps
- janela local aumentada
- buffers seriais agora respeitam mais o valor configurado
- recuperação pós-hold menos lenta
