# SMI_DNC

DNC Qt/C++ para máquina única, com foco em envio remoto RS-232 mais previsível.

## O que foi corrigido nesta entrega

Esta versão atacou os pontos que mais diferenciavam o seu motor dos DNCs públicos e da prática comum em serial industrial:

- o software volta a usar **flow control nativo do driver** quando o perfil está em `Software (XON/XOFF)` ou `Hardware (RTS/CTS)`
- o modo manual deixou de ser limitado a bursts de **1 a 4 bytes**
- `flowControl` voltou a ser salvo e carregado corretamente do JSON
- `InBufferSize` e `OutBufferSize` agora servem como limite real da porta, não como placebo sobre um scheduler já estrangulado
- o watchdog de “sem avanço” foi desativado quando o fluxo é nativo do driver, para evitar hold falso
- a configuração padrão foi simplificada para uso real: **um método de fluxo por vez**

## Arquitetura desta versão

- UI Qt Widgets
- motor serial com `QSerialPort`
- histórico e trace por sessão
- envio, recepção e diagnóstico básico
- simulador integrado
- configuração persistida em JSON

## Como configurar

### 1. Cabo sem controle usando XON/XOFF
Use quando a máquina realmente trabalha com software flow control.

- `Flow Control`: `Software (XON/XOFF)`
- `OutBufferSize`: `256`
- `InBufferSize`: `256` ou `512`
- `Taxa envio manual`: `Auto` para começar
- `Leitura sinais`: `1 ms` ou `2 ms`

### 2. Cabo com RTS/CTS
Use quando a máquina e o cabo suportam hardware flow control.

- `Flow Control`: `Hardware (RTS/CTS)`
- `OutBufferSize`: `256`
- `InBufferSize`: `256`
- `Taxa envio manual`: `Auto` para começar
- `Leitura sinais`: `2 ms`

### 3. Quando usar taxa manual
Use somente se o modo `Auto` ainda gerar overflow em uma máquina muito sensível.

Faixa inicial recomendada em `19200 baud`:

- `1200 B/s`
- `1400 B/s`
- `1600 B/s`

O valor é em **bytes por segundo úteis**, não em baud.

## Procedimento de teste recomendado

1. configure porta, baud, paridade, databits e stopbits exatamente como a CNC
2. escolha **um único** método de fluxo
3. use buffers `256/256`
4. faça um teste com programa curto
5. verifique na tela de envio:
   - `Inst.`
   - `Média`
   - `Pico`
   - `Hold`
   - `XOFF`
   - `Chunk`
6. depois teste com programa longo

## Sinais de diagnóstico

### Auto ainda dá overflow
- o perfil de fluxo pode estar errado
- a máquina pode não estar em XON/XOFF limpo
- pode ser necessário usar taxa manual real

### Manual trava perto de 900 B/s
Nesta entrega isso deixou de ser o comportamento esperado. Antes o burst era limitado a `1..4` bytes; agora o burst manual é coerente com a taxa configurada.

### Começa bem e gargala no meio
Isso normalmente aponta para um destes cenários:
- o fluxo selecionado não é o real da máquina
- o CNC segura por longos períodos e a taxa média cai legitimamente
- a máquina consome menos do que parecia no início do arquivo

## Build

### Requisitos
- Qt 5.15.2 Widgets (recomendado para Windows 7 x64)
- Qt 5.15.2 SerialPort
- CMake
- MSVC 2019 v142 ou MinGW compatível com o kit Qt 5.15.2

### Passos
1. abra a pasta no Qt Creator
2. configure um kit com Qt 5.15.2 para Windows 7 x64
3. rode `Configure Project`
4. faça `Build`
5. execute o app

## Limite honesto desta entrega

Esta versão corrige os principais erros arquiteturais encontrados na comparação com implementações públicas e com a documentação oficial do stack serial. Ela ainda precisa de validação final em máquina real, porque este ambiente não tem Qt6 nem acesso físico à CNC para ensaio de bancada.


## Ajustes da versão
- Recepção agora sugere e salva arquivos com extensão `.nc`.
- Painel de sinais RS232 com LEDs para DSR, DTR, RTS, CTS, TX, RX, DCD e RI.
- Nova aba `Sobre` criada e deixada em branco para preenchimento posterior.


## Ajustes v35 — operação simplificada

- **Envio com botões inteligentes**: apenas a ação correta aparece por estado.
  - parado: `Enviar`
  - enviando: `Pausar` e `Parar`
  - pausado: `Retomar` e `Parar`
- **Recepção com um único botão**: ao clicar em `Receber`, o operador escolhe o destino no explorador, confirma a recepção e o software abre a porta para aguardar o programa.
- **Proteção de sobrescrita**: se o arquivo já existir, o operador pode `Substituir`, `Criar novo` com nome automático ou `Cancelar`.
- **Aba Sobre**: mantida em branco para edição posterior.


## Build recomendado para Windows 7 x64

Para manter o motor de envio intacto e maximizar a compatibilidade com Windows 7 x64, esta base deve ser compilada preferencialmente com **Qt 5.15.2** e **MSVC 2019 v142**. Qt 6 não é a escolha correta para Windows 7.

Resumo:
- manter a engine de envio atual
- usar Qt 5.15.2
- usar CMake desta pasta, que agora aceita Qt5 e Qt6
- gerar o deploy com `windeployqt` do kit Qt 5.15.2
