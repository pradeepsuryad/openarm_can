#!/usr/bin/env bash
# One scenario of the receive-filter checks for #165, run against a build of
# openarm_can. Needs vcan0 with mtu 72.
#
# Usage: check_recv_filter.sh BUILD_DIR SCENARIO
#
#   control-loop  Another process drives motors 0x01 and 0x02 on the same bus
#                 while diagnose and health run. Passes if neither reports
#                 that process's frames as unmatched.
#   rid7          Motor 0x02 replies on 0x00, as a motor whose master id
#                 (RID 7) was never set does. Passes if diagnose and health
#                 both report 0x00 as unmatched.
#   flood         Frames on 0x123 as fast as the bus takes them while
#                 diagnose runs against motor 0x01. Prints the report and
#                 always passes.

set -u

build=$1
scenario=$2
here=$(cd "$(dirname "$0")" && pwd)
fail=0
pids=()

# check DESCRIPTION COMMAND...: runs COMMAND and reports ok or FAIL.
check() {
    local what=$1
    shift
    if "$@"; then
        echo "  ok    $what"
    else
        echo "  FAIL  $what"
        fail=1
    fi
}

# start COMMAND...: runs COMMAND in the background until the scenario ends.
start() {
    "$@" &
    pids+=("$!")
}

# run OUT COMMAND...: runs COMMAND with its output in OUT, and prints it.
run() {
    local out=$1
    shift
    "$@" >"$out" 2>&1
    echo "=== $* (exit status $?)"
    cat "$out"
}

echo "##### $scenario, $build"
case $scenario in
control-loop)
    start python3 "$here/fake_motor.py" vcan0 0x01 0x02
    start python3 "$here/control_loop.py" vcan0 0x01 0x02
    sleep 1
    run diag.txt "$build/openarm-can-cli" -i vcan0 diagnose --id 1,2 -d 1000 -t 10 --explain
    run health.txt "$build/openarm-can-health" vcan0 2 2
    echo "--- checks"
    check "diagnose (3): no unmatched frames" test -z "$(grep "^ Unmatched :" diag.txt)"
    check "diagnose --explain: no unmatched-id warning" \
        test -z "$(grep "Something is replying on ids nothing is listening for" diag.txt)"
    check "diagnose --explain: nothing to report" grep -q "^ Nothing to report" diag.txt
    check "health: no unmatched lines" test -z "$(grep "unmatched:" health.txt)"
    ;;
rid7)
    start python3 "$here/fake_motor.py" vcan0 0x01 0x02:0x00
    sleep 1
    run diag.txt "$build/openarm-can-cli" -i vcan0 diagnose --id 1,2 -d 1000 -t 10 --explain
    run health.txt "$build/openarm-can-health" vcan0 2 2
    echo "--- checks"
    check "diagnose (3): 0x00 listed as unmatched" grep -q "^   0x00 x" diag.txt
    check "diagnose --explain: warning lists 0x00" grep -q "^     0x00 x" diag.txt
    check "health: 0x00 unmatched" grep -q "unmatched: id 0x00" health.txt
    ;;
flood)
    start python3 "$here/fake_motor.py" vcan0 0x01
    start python3 "$here/flood.py" vcan0 0x123
    sleep 1
    run diag.txt "$build/openarm-can-cli" -i vcan0 diagnose --id 1 -d 1000 -t 10
    ;;
*)
    echo "unknown scenario: $scenario"
    exit 2
    ;;
esac

kill "${pids[@]}"
wait 2>/dev/null
exit "$fail"
