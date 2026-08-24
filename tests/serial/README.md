# Testes seriais rápidos

## Loopback
Faça um jumper TX<->RX na porta/adaptador e execute:

```bash
python tests/serial/loopback_test.py --port COM5 --baud 9600 --bytes 512
```

## Simulador XON/XOFF
Use uma ponta da serial virtual ou um segundo adaptador:

```bash
python tests/serial/cnc_sim_xonxoff.py --port COM6 --baud 9600 --cycle-bytes 200 --pause-ms 800 --initial-xon
```
