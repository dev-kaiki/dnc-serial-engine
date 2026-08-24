import argparse, time, sys
try:
    import serial
except Exception as e:
    print('pyserial não instalado:', e)
    sys.exit(2)
XON = b'\x11'
XOFF = b'\x13'

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', required=True)
    ap.add_argument('--baud', type=int, default=9600)
    ap.add_argument('--cycle-bytes', type=int, default=200)
    ap.add_argument('--pause-ms', type=int, default=800)
    ap.add_argument('--initial-xon', action='store_true')
    args = ap.parse_args()
    total = 0
    with serial.Serial(args.port, baudrate=args.baud, timeout=0.1, write_timeout=1.0, xonxoff=False, rtscts=False) as s:
        if args.initial_xon:
            s.write(XON)
        while True:
            data = s.read(256)
            if data:
                total += len(data)
                if total >= args.cycle_bytes:
                    s.write(XOFF)
                    time.sleep(args.pause_ms / 1000.0)
                    s.write(XON)
                    total = 0
                    print('XOFF/XON emitido')
            time.sleep(0.01)

if __name__ == '__main__':
    main()
