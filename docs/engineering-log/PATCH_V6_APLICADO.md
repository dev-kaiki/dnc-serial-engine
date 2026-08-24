PATCH V6 - Motor de envio revisado

Alterações aplicadas:
- Ignorado hold prematuro antes da sessão iniciar.
- Watchdog não interfere mais na recepção nem na drenagem final.
- Drenagem final agora exige janela de estabilidade antes de concluir.
- Espera curta por bytes escritos antes de fechar a porta em sucesso.
- Popup de conclusão com opção de abrir editor de programa.
- Abertura de pasta para recepção concluída.

Arquivos alterados:
- core/dnc/DncEngine.h
- core/dnc/DncEngine.cpp
- core/dnc/DrainController.h
- core/dnc/DrainController.cpp
- core/serial/SerialPortFacade.h
- core/serial/SerialPortFacade.cpp
- ui/pages/SendPage.h
- ui/pages/SendPage.cpp
