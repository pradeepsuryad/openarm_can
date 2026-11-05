// Check for enactic/openarm_can#162: motor index i must be the i-th id passed
// to init_arm_motors(), whatever order the ids are in. Needs a CAN interface
// (vcan0); no motors are attached, so only what the library sends is checked.
#include <cstdio>
#include <openarm/can/socket/openarm.hpp>

using namespace openarm::can::socket;
using namespace openarm::damiao_motor;

static bool check(bool ok, const char* what) {
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what);
    return ok;
}

// ids in the given order, MIT for the first motor and POS_VEL for the second.
static bool run(const char* label, canid_t first, canid_t second) {
    std::printf("%s: ids 0x%02x, 0x%02x with MIT, POS_VEL\n", label, first, second);
    OpenArm openarm("vcan0", false);
    openarm.init_arm_motors({MotorType::DM4310, MotorType::DM4310}, {first, second},
                            {first + 0x10, second + 0x10}, {ControlMode::MIT, ControlMode::POS_VEL});
    auto& arm = openarm.get_arm();
    const auto motors = arm.get_motors();
    bool ok = check(motors[0].get_send_can_id() == first && motors[1].get_send_can_id() == second,
                    "get_motors() is in the order the ids were passed");

    // Index 0 was given MIT, so an MIT command to it is sent, not rejected.
    const MotorLinkStats a0 = arm.get_link_stats(0);
    arm.mit_control_one(0, MITParam{0.0, 0.0, 0.0, 0.0, 0.0});
    const MotorLinkStats a1 = arm.get_link_stats(0);
    ok &= check(a1.commands_sent == a0.commands_sent + 1 && a1.rejected_commands == a0.rejected_commands,
                "an MIT command to index 0 (given MIT) is sent");

    // Index 1 was given POS_VEL, so a POS_VEL command to it is sent.
    const MotorLinkStats b0 = arm.get_link_stats(1);
    arm.posvel_control_one(1, PosVelParam{0.0, 0.0});
    const MotorLinkStats b1 = arm.get_link_stats(1);
    ok &= check(b1.commands_sent == b0.commands_sent + 1 && b1.rejected_commands == b0.rejected_commands,
                "a POS_VEL command to index 1 (given POS_VEL) is sent");
    return ok;
}

int main() {
    bool ok = run("ascending", 0x01, 0x02);
    ok &= run("descending (the #162 example)", 0x02, 0x01);
    std::printf("%s\n", ok ? "ALL PASS" : "SOME FAILED");
    return ok ? 0 : 1;
}
