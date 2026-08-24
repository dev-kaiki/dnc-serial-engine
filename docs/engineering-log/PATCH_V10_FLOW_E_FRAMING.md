# PATCH V10 - Fluxo RS232 dual (XON/XOFF e CTS/RTS) + framing industrial

## O que foi alterado

- Mantido o suporte aos dois modos de controle de fluxo:
  - `XON/XOFF` manual no motor
  - `CTS/RTS` manual determinístico (sem depender de `HardwareControl` do driver)
- Ajustado o perfil padrão para texto industrial:
  - `CRLF` como terminador de linha
  - remoção de BOM UTF-8
  - coerção opcional para ASCII 7-bit em `Data7`
  - `wrap` automático com `%` no início/fim quando o arquivo ainda não vier delimitado
  - garantia de quebra de linha final antes do terminador/sufixo
- Corrigido bug da tela de configurações que carregava `min/max chunk` a partir de `read/writeBufferLimit`.

## Objetivo técnico

Fazer o DNC se comportar corretamente tanto em máquinas configuradas para `XON/XOFF` quanto em máquinas configuradas para `CTS/RTS`, e reduzir o caso em que o PC diz que enviou, mas a CNC continua esperando porque não reconheceu framing de programa.

## Observação

Para máquinas muito sensíveis ao protocolo, o envio ainda depende de o cabo e os parâmetros da CNC estarem corretos. Este patch ataca o lado de software/framing e mantém o handshake de hardware e software separado de forma previsível.
