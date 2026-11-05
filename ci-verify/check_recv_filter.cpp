// Checks CANSocket's receive filters (#165) on vcan0: everything until the
// first filter, nothing queued before it handed out after it, only the
// filtered ids from then on, references counted across collections that share
// the socket, and everything again once the last one is removed.

#include <cstdio>
#include <memory>
#include <openarm/canbus/can_device_collection.hpp>
#include <openarm/canbus/can_socket.hpp>
#include <set>

using openarm::canbus::CANDevice;
using openarm::canbus::CANDeviceCollection;
using openarm::canbus::CANSocket;

namespace {

struct Probe : CANDevice {
    explicit Probe(canid_t recv_id) : CANDevice(0, recv_id, CAN_SFF_MASK) {}
    void callback(const can_frame&) override {}
    void callback(const canfd_frame&) override {}
};

int failures = 0;

// Sends one frame each on 0x011, 0x012 and 0x123 from tx.
void send_three(CANSocket& tx) {
    for (canid_t id : {0x011u, 0x012u, 0x123u}) {
        can_frame f{};
        f.can_id = id;
        f.can_dlc = 8;
        tx.write_can_frame(f);
    }
}

// The ids of every frame rx has waiting.
std::set<canid_t> received(CANSocket& rx) {
    std::set<canid_t> got;
    can_frame f{};
    while (rx.is_data_available(20000) && rx.read_can_frame(f)) got.insert(f.can_id);
    return got;
}

std::set<canid_t> exchange(CANSocket& tx, CANSocket& rx) {
    send_three(tx);
    return received(rx);
}

void check(const char* what, const std::set<canid_t>& got, const std::set<canid_t>& want) {
    const bool ok = got == want;
    std::printf("  %s  %s: got", ok ? "ok  " : "FAIL", what);
    for (auto id : got) std::printf(" 0x%03X", id);
    std::printf("%s\n", got.empty() ? " nothing" : "");
    if (!ok) failures++;
}

}  // namespace

int main() {
    CANSocket tx("vcan0"), rx("vcan0");
    const std::set<canid_t> all = {0x011, 0x012, 0x123};

    check("no filter yet: everything", exchange(tx, rx), all);

    send_three(tx);
    rx.add_recv_filter(0x011, CAN_SFF_MASK);
    check("queued before the first filter: dropped", received(rx), {});

    check("filter for 0x011: only 0x011", exchange(tx, rx), {0x011});

    rx.add_recv_filter(0x011, CAN_SFF_MASK);
    rx.remove_recv_filter(0x011, CAN_SFF_MASK);
    check("added twice, removed once: still only 0x011", exchange(tx, rx), {0x011});

    rx.remove_recv_filter(0x011, CAN_SFF_MASK);
    check("last one removed: everything again", exchange(tx, rx), all);

    // Like the arm's collection and the master collection, which hold the
    // same devices.
    CANDeviceCollection arm(rx), master(rx);
    auto device = std::make_shared<Probe>(0x012);
    arm.add_device(device);
    master.add_device(device);
    check("0x012 held by two collections: only 0x012", exchange(tx, rx), {0x012});

    master.remove_device(device);
    check("removed from one: still only 0x012", exchange(tx, rx), {0x012});

    arm.add_device(std::make_shared<Probe>(0x012));
    check("replaced in the other: still only 0x012", exchange(tx, rx), {0x012});

    arm.remove_device(device);
    check("removed from both: everything again", exchange(tx, rx), all);

    std::printf(failures ? "FAIL\n" : "PASS\n");
    return failures ? 1 : 0;
}
