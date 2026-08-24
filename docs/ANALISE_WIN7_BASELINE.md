# Baseline Win7 x64 a partir do pacote funcional

Esta pasta foi preparada para preservar o motor de envio que ja funcionava e reduzir o trabalho a uma migracao de build/runtime para Windows 7 x64.

## Decisao tecnica
- manter o motor de envio existente
- nao reescrever o DncEngine
- nao trocar a logica de Drain/Flow/Watchdog
- trocar somente a camada de build para aceitar Qt5

## Mudancas feitas
1. `CMakeLists.txt` refeito para detectar `Qt5` ou `Qt6`.
2. `qt_standard_project_setup()` removido para evitar dependencia exclusiva de Qt6.
3. `qt_add_executable()` substituido por `add_executable()` com `AUTOMOC/AUTORCC/AUTOUIC`.
4. `WINVER` e `_WIN32_WINNT` fixados em `0x0601` para Windows 7.
5. `README.md` ajustado para recomendar Qt 5.15.2 no alvo Win7 x64.

## Recomendacao
Use esta base como a principal para o app final. O trabalho seguinte deve focar em:
- polimento visual
- ajustes de layout
- instalador
- validacao real em Win7 x64

Nao mexer no motor de envio sem ensaio em bancada.
