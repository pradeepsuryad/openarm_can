#!/usr/bin/env python3
"""Answer as one or more Damiao motors on a virtual CAN FD bus.

Usage: fake_motor.py IFACE MOTOR [MOTOR ...]

MOTOR is SEND_ID, or SEND_ID:REPLY_ID for a motor that replies on another id
than SEND_ID + 0x10, such as 0x02:0x00 for one whose master id (RID 7) was
never set. Each motor replies with a state frame to every frame sent to its
SEND_ID (enable, disable, commands) and to every refresh for it (0x7FF, data
id_lo id_hi 0xCC). The socket only receives those ids, so other traffic on
the bus does not delay the replies.
"""

import socket
import struct
import sys

CANFD_MTU = 72


def main():
    iface = sys.argv[1]
    replies = {}
    for arg in sys.argv[2:]:
        send, _, reply = arg.partition(":")
        send_id = int(send, 0)
        reply_id = int(reply, 0) if reply else send_id + 0x10
        # D[0] = id | ERR << 4 with ERR 1 (enabled). Position, velocity and
        # torque at mid-scale, which is zero. MOS and rotor at 30 C.
        state = bytes([0x10 | (send_id & 0x0F), 0x80, 0x00, 0x80, 0x08, 0x00,
                       30, 30])
        replies[send_id] = (struct.pack("=IBxxx", reply_id, len(state))
                            + state.ljust(64, b"\0"))

    s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
    s.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FD_FRAMES, 1)
    wanted = [0x7FF, *replies]
    s.setsockopt(socket.SOL_CAN_RAW, socket.CAN_RAW_FILTER,
                 b"".join(struct.pack("=II", i, socket.CAN_SFF_MASK)
                          for i in wanted))
    s.bind((iface,))
    print(f"answering as {' '.join(sys.argv[2:])} on {iface}", flush=True)

    while True:
        frame = s.recv(CANFD_MTU)
        can_id, length = struct.unpack_from("=IB", frame)
        data = frame[8:8 + length]
        if can_id == 0x7FF and length >= 3 and data[2] == 0xCC:
            can_id = data[0] | data[1] << 8
        reply = replies.get(can_id)
        if reply:
            s.send(reply)


if __name__ == "__main__":
    main()
