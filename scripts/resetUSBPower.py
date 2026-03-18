#!/usr/bin/env python3

import sys
import time
import argparse

import serial
import serial.tools.list_ports

ESPRESSIF_VID = 0x303A
BAUD = 9600
TIMEOUT = 2.0
VERSION_MARKER = "Compiled"


def candidate_ports():
    candidates = []
    for info in serial.tools.list_ports.comports():
        # On macOS skip /dev/tty.* to avoid blocking on open
        if sys.platform == "darwin" and info.device.startswith("/dev/tty."):
            continue
        if info.vid is not None:
            if info.vid == ESPRESSIF_VID:
                candidates.append(info.device)
        elif "usbmodem" in info.device.lower():
            candidates.append(info.device)
    return sorted(candidates)


def send_command(ser, cmd):
    ser.write((cmd.strip() + "\n").encode("ascii"))
    ser.flush()
    deadline = time.time() + TIMEOUT
    lines = []
    while time.time() < deadline:
        line = ser.readline().decode("ascii", errors="replace").strip()
        if line:
            lines.append(line)
            deadline = time.time() + 0.5
    return "\n".join(lines)


def find_device():
    for port in candidate_ports():
        try:
            with serial.Serial(port, BAUD, timeout=TIMEOUT) as ser:
                time.sleep(0.3)
                ser.reset_input_buffer()
                ser.write(b"VERSION\n")
                ser.flush()
                deadline = time.time() + TIMEOUT
                while time.time() < deadline:
                    line = ser.readline().decode("ascii", errors="replace").strip()
                    if VERSION_MARKER in line:
                        return port
        except (serial.SerialException, OSError):
            continue
    return None


def main(args):
    port = args.port
    if port is None:
        port = find_device()
    if port is None:
        print("USB Power Switch not found", file=sys.stderr)
        sys.exit(1)
        
    if args.list:
        print(port)
    elif args.status:
        with serial.Serial(port, BAUD, timeout=TIMEOUT) as ser:
            time.sleep(0.3)
            ser.reset_input_buffer()
            print(send_command(ser, "STATUS"))
    else:
        cmd = "RESET"
        if args.off_time is not None:
            cmd += " %d" % args.off_time
        with serial.Serial(port, BAUD, timeout=TIMEOUT) as ser:
            time.sleep(0.3)
            ser.reset_input_buffer()
            print(send_command(ser, cmd))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Reset USB power via the USB Power Switch device',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
        )
    parser.add_argument('off_time', type=int, nargs='?', default=None,
                        help='time in ms to keep power off (default 250)')
    parser.add_argument('-p', '--port', type=str, default=None,
                        help='serial port path (auto-detected if omitted)')
    parser.add_argument('-l', '--list', action='store_true',
                        help='list the device port and exit')
    parser.add_argument('-s', '--status', action='store_true',
                        help='report device status and exit')
    args = parser.parse_args()
    main(args)
