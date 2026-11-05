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

// The ids in the given order, MIT for the first motor and POS_VEL for the second.
static bool run(const char* label, canid_t first, canid_t second) {
    std::printf("%s: ids 0x%02x, 0x%02x with MIT, POS_VEL\n", label, first, second);
    OpenArm openarm("vcan0", false);
    openarm.init_arm_motors({MotorType::DM4310, MotorType::DM4310}, {first, second},
                            {first + 0x10, second + 0x10}, {ControlMode::MIT, ControlMode::POS_VEL});
    auto& arm = openarm.get_arm();
    const auto motors = arm.get_motors();
    bool ok = check(motors[0].get_send_can_id() == first && motors[1].get_send_can_id() == second,
                    "get_motors() is in the order the ids were passed");

    // Each motor must have the mode the caller gave it, looked up by id: the
    // first motor passed (MIT) accepts an MIT command, the second (POS_VEL) a
    // POS_VEL command. #162: upstream gives MIT to 0x01 instead.
    auto index_of = [&](canid_t id) {
        for (size_t i = 0; i < motors.size(); ++i) {
            if (motors[i].get_send_can_id() == id) return static_cast<int>(i);
        }
        return -1;
    };
    const int i_first = index_of(first);
    const int i_second = index_of(second);

    const MotorLinkStats a0 = arm.get_link_stats(i_first);
    arm.mit_control_one(i_first, MITParam{0.0, 0.0, 0.0, 0.0, 0.0});
    const MotorLinkStats a1 = arm.get_link_stats(i_first);
    ok &= check(a1.commands_sent == a0.commands_sent + 1 && a1.rejected_commands == a0.rejected_commands,
                "the motor passed first (given MIT) accepts an MIT command");

    const MotorLinkStats b0 = arm.get_link_stats(i_second);
    arm.posvel_control_one(i_second, PosVelParam{0.0, 0.0});
    const MotorLinkStats b1 = arm.get_link_stats(i_second);
    ok &= check(b1.commands_sent == b0.commands_sent + 1 && b1.rejected_commands == b0.rejected_commands,
                "the motor passed second (given POS_VEL) accepts a POS_VEL command");
    return ok;
}

int main() {
    bool ok = run("ascending", 0x01, 0x02);
    ok &= run("descending (the #162 example)", 0x02, 0x01);
    std::printf("%s\n", ok ? "ALL PASS" : "SOME FAILED");
    return ok ? 0 : 1;
}
