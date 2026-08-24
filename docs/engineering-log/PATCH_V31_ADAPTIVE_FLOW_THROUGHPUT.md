# PATCH V31 — throughput maior sem voltar ao overflow bruto

## Causa raiz atacada
A V30 melhorou o overflow, mas impôs um teto artificial de throughput por dois motivos:
1. o caminho de fluxo manual continuava tratado como pacing estrito;
2. antes do primeiro XOFF o chunk ficava implicitamente limitado demais, o que serrilhava o envio e prendia a taxa em ~1400 B/s.

## Alterações
- separação entre:
  - **fluxo manual adaptativo** (XON/XOFF ou CTS/RTS)
  - **pacing estrito por taxa manual B/s**
- em fluxo manual adaptativo:
  - chunk inicial curto, mas crescimento mais rápido
  - chunk máximo maior
  - janela local maior e derivada do baud/framing
  - write buffer do Qt/driver maior que na V30, mas ainda curto o suficiente para não explodir overshoot
  - recuperação pós-XON/CTS-high curta e controlada, sem prender o throughput antes do primeiro XOFF
- em taxa manual B/s:
  - permanece o pacing estrito da V30

## Efeito esperado
- manter a contenção de overflow melhor que a V28/V29
- eliminar o teto artificial de ~1400 B/s quando houver fluxo manual e a máquina estiver consumindo
- ainda reagir rapidamente a XOFF e CTS baixo
