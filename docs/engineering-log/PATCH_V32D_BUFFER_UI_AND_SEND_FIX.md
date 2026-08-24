Correções objetivas:
- InBufferSize e OutBufferSize agora usam widgets próprios e persistem corretamente.
- Os campos visíveis de buffer não sobrescrevem mais min/max chunk.
- Regras de fallback preservam buffers manuais informados pelo usuário.
- Buffer de escrita do Qt em fluxo manual foi relaxado para evitar estado "não envia".
- Janela local do engine foi relaxada para evitar travamento por backlog excessivamente conservador.
