SMI_DNC V28 - baseline a partir da v23 funcional

Objetivo:
- voltar para a base que efetivamente transmitia
- remover a regressao introduzida pelo modo de janela por bloco em fluxo manual
- reduzir gargalo sem reintroduzir deadlock de envio

Alteracoes principais:
1. Mantido remoteBlockModeFor() = false em fluxo manual.
2. Scheduler manual aumentado de forma moderada e derivada do baud/framing.
3. Janela local de bytes em voo aumentada de forma curta e controlada (8-24 bytes).
4. Purge/rewind de backlog manual removido para evitar duplicacao/truncamento de bloco.
5. Drenagem final nao depende mais de sendAllowed().
6. Logs de fluxo/transmissao/recepcao ampliados.

Racional:
- a v23 ainda enviava, portanto ela e a melhor base observavel.
- a v27 reintroduziu comportamento estrutural diferente ao habilitar janela por bloco manual.
- esta v28 preserva a base v23 e ataca apenas o gargalo e a instrumentacao.
