import argparse, os, sys, time
try:
    import serial
except Exception as e:
    print('pyserial não instalado:', e)
    sys.exit(2)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', required=True)
    ap.add_argument('--baud', type=int, default=9600)
    ap.add_argument('--bytes', type=int, default=256)
    args = ap.parse_args()
    payload = bytes((i % 251 for i in range(args.bytes)))
    with serial.Serial(args.port, baudrate=args.baud, timeout=1.0, write_timeout=1.0, xonxoff=False, rtscts=False) as s:
        s.reset_input_buffer(); s.reset_output_buffer()
        n = s.write(payload)
        s.flush()
        time.sleep(0.2)
        data = s.read(len(payload))
    print(f'escritos={n} lidos={len(data)} ok={data == payload}')
    return 0 if data == payload else 1

if __name__ == '__main__':
    raise SystemExit(main())
