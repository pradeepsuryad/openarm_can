#!/usr/bin/env python3
"""Stand in for another process's control loop on the same bus.

Usage: control_loop.py IFACE SEND_ID [SEND_ID ...]

Every 10 ms it sends each motor a refresh (0x7FF) and an MIT command on its
SEND_ID, as CAN FD frames, until it is killed.
"""

import socket
import struct
import sys
import time


def fd_frame(can_id, data):
    return struct.pack("=IBxxx", can_id, len(data)) + data.ljust(64, b"\0")


def main():
    iface = sys.argv[1]
    ids = [int(a, 0) for a in sys.argv[2:]]
    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FD_FRAMES, 1)
    # Sends only, so receive nothing rather than let the queue fill up.
    s.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FILTER, b"")
    s.bind((iface,))
    # MIT command with position, velocity and torque at mid-scale (zero) and
    # both gains at zero.
    mit = bytes([0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x08, 0x00])
    print(f"driving {' '.join(sys.argv[2:])} on {iface}", flush=True)
    while True:
        for i in ids:
            refresh = bytes([i & 0xFF, i >> 8, 0xCC, 0, 0, 0, 0, 0])
            s.send(fd_frame(0x7FF, refresh))
            s.send(fd_frame(i, mit))
        time.sleep(0.01)


if __name__ == "__main__":
    main()
