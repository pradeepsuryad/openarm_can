#!/usr/bin/env bash
# Checks that diagnose attaches each row and each piece of advice to the
# right motor whatever order --id is given in (#161). Needs vcan0 with
# mtu 72. A fake motor answers as 0x01 and nothing answers as 0x08, as
# if the chain were broken between them.
#
# Usage: check_diagnose_order.sh path/to/openarm-can-cli

set -u

cli=$1
here=$(cd "$(dirname "$0")" && pwd)
fail=0

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

# run IDS OUT [ARGS...]: half a second of diagnose, output and exit status
# in OUT.
run() {
    local ids=$1 out=$2
    shift 2
    "$cli" -i vcan0 diagnose --id "$ids" -d 500 -t 20 "$@" >"$out" 2>&1
    echo "exit status $?" >>"$out"
    echo "=== openarm-can-cli -i vcan0 diagnose --id $ids -d 500 -t 20 $*"
    cat "$out"
}

# rows SECTION OUT: number of per-axis rows in one section, e.g. "(2b)".
rows() {
    awk -v s="--- $1 " '
        index($0, s) == 1 { f = 1; next }
        /^--- / { f = 0 }
        f && $1 ~ /^0x/ { n++ }
        END { print n + 0 }' "$2"
}

# recv_of ID OUT: the recv column of ID's row in section (2b), -1 if none.
recv_of() {
    awk -v id="$1" '
        index($0, "--- (2b) ") == 1 { f = 1; next }
        /^--- / { f = 0 }
        f && $1 == id { n = $3 }
        END { print (n == "" ? -1 : n) }' "$2"
}

python3 "$here/fake_motor.py" vcan0 0x01 &
fake=$!
sleep 1

for ids in 8,1 1,8; do
    out=out_$ids.txt
    run "$ids" "$out" --explain
    echo "--- checks for --id $ids"
    check "report in ascending order" grep -qx " Motors    : 0x01 0x08" "$out"
    check "(2b): replies counted for 0x01" test "$(recv_of 0x01 "$out")" -gt 0
    check "(2b): none counted for 0x08" test "$(recv_of 0x08 "$out")" -eq 0
    check "(2b): break placed between 0x01 and 0x08" \
        grep -qF -- "-> axes up to 0x01 answer and 0x08 onward do not" "$out"
    check "--explain: break placed between 0x01 and 0x08" \
        grep -qF "Everything up to 0x01 answers and 0x08 onward does not" "$out"
    check "exit status 0" grep -qx "exit status 0" "$out"
done

out=out_1_1.txt
run 1,1 "$out"
echo "--- checks for --id 1,1"
check "one motor, 0x01" grep -qx " Motors    : 0x01" "$out"
check "(1): one row" test "$(rows "(1)" "$out")" -eq 1
check "(2b): one row" test "$(rows "(2b)" "$out")" -eq 1
check "exit status 0" grep -qx "exit status 0" "$out"

kill "$fake"

if [ "$fail" -eq 0 ]; then
    echo "PASS"
else
    echo "FAIL"
fi
exit "$fail"
