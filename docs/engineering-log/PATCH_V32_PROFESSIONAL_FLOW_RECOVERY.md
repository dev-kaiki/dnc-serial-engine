# PATCH V32 - PROFESSIONAL FLOW RECOVERY

## Objetivo
Reduzir a queda de throughput no meio do programa sem voltar ao overflow.

## Alterações principais
- retomada com guarda curta apos XON/CTS-high
- chunk adaptativo com crescimento por janela estável e redução conforme hold precoce
- métricas adicionais de sessão: taxa média, instantânea, pico, eficiência de linha, latência de retomada e faixa de chunk
- contabilização separada de RX útil e contadores de hold
- UI de envio com leitura operacional mais fiel ao comportamento industrial

## Efeito esperado
- menos serrilhado Sending -> HoldXoff -> Sending
- recuperação mais limpa após liberação da máquina
- melhor diagnóstico quando a CNC realmente limitar o fluxo
