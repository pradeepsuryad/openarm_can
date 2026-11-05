#!/usr/bin/env python3
"""Answer as one Damiao motor on a virtual CAN FD bus.

Usage: fake_motor.py IFACE SEND_ID

Replies with a state frame on SEND_ID + 0x10 to every frame sent to SEND_ID
(enable, disable) and to every refresh for it (0x7FF, data id_lo id_hi 0xCC).
Every other motor stays silent, like one that is not connected.
"""

import socket
import struct
import sys

CANFD_MTU = 72


def main():
    iface, send_id = sys.argv[1], int(sys.argv[2], 0)
    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FD_FRAMES, 1)
    s.bind((iface,))

    # D[0] = id | ERR << 4 with ERR 1 (enabled). Position, velocity and
    # torque at mid-scale, which is zero. MOS and rotor at 30 C.
    state = bytes([0x10 | (send_id & 0x0F), 0x80, 0x00, 0x80, 0x08, 0x00,
                   30, 30])
    reply = struct.pack("=IBxxx", send_id + 0x10, len(state))
    reply += state.ljust(64, b"\0")
    refresh = bytes([send_id & 0xFF, (send_id >> 8) & 0xFF, 0xCC])
    print(f"answering as {send_id:#04x} on {iface}", flush=True)

    while True:
        frame = s.recv(CANFD_MTU)
        can_id, length = struct.unpack_from("=IB", frame)
        data = frame[8:8 + length]
        if can_id == send_id or (can_id == 0x7FF and data[:3] == refresh):
            s.send(reply)


if __name__ == "__main__":
    main()
