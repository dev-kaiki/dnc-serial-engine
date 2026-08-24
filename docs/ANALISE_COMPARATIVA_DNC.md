# Análise comparativa aplicada ao SMI_DNC

## O que foi observado em projetos/documentação pública

1. Quando o driver oferece `SoftwareControl` ou `HardwareControl`, o caminho mais estável é deixar o stack serial fazer esse trabalho.
2. Implementações que exigem confirmação do receptor não usam scheduler de GUI para “adivinhar” o avanço remoto.
3. Misturar muitos mecanismos de pacing ao mesmo tempo tende a piorar o comportamento, não melhorar.

## Problemas identificados no projeto anterior

- `flowControl` salvo em JSON era descartado e convertido para lógica emulada no app
- o `QSerialPort` era forçado para `NoFlowControl` mesmo quando o usuário escolhia XON/XOFF ou RTS/CTS
- o modo manual limitava burst e janela a poucos bytes
- isso criava o padrão observado em campo:
  - `Auto` agressivo demais
  - `Manual` estrangulado demais

## Decisão de engenharia adotada nesta versão

- priorizar **flow control nativo** quando o perfil selecionado assim indicar
- manter flow emulado apenas como fallback técnico
- remover o teto absurdo de 1..4 bytes no modo manual
- tratar buffer como limite de porta, não como substituto do motor

## Resultado esperado

- menos overshoot no modo remoto
- mais throughput útil no modo manual
- persistência correta do método de fluxo
- comparação mais honesta entre o SMI_DNC e DNCs públicos/stack serial padrão
