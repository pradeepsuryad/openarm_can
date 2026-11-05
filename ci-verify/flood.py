#!/usr/bin/env python3
"""Fill the bus with frames for an id that nothing here registers.

Usage: flood.py IFACE CAN_ID

Sends classic frames on CAN_ID as fast as it can, until it is killed.
"""

import socket
import struct
import sys


def main():
    iface, can_id = sys.argv[1], int(sys.argv[2], 0)
    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FILTER, b"")
    s.bind((iface,))
    frame = struct.pack("=IB3x8s", can_id, 8, bytes(8))
    print(f"flooding {can_id:#05x} on {iface}", flush=True)
    while True:
        try:
            s.send(frame)
        except OSError:
            pass


if __name__ == "__main__":
    main()
