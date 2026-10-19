#!/bin/sh
# End-to-end checks for the harness + policy 1. Uses a small file in $TESTDIR (default: ./tmp_test).
set -e
D=${TESTDIR:-./tmp_test}; mkdir -p "$D"
DIRECT=${DIRECT:-1}          # DIRECT=0 for filesystems without O_DIRECT (tmpfs)
fail=0
check() { if [ "$2" = "$3" ]; then echo "ok:   $1"; else echo "FAIL: $1 (got '$2', wanted '$3')"; fail=1; fi; }

./mkfile "$D/test.bin" 64 >/dev/null
./trace_gen --mode steady --rate 5000 --duration 2 --seed 3 --file-size 67108864 --out "$D/t.csv" >/dev/null
N=$(($(wc -l < "$D/t.csv") - 1))

out=$(./run_bench --file "$D/test.bin" --trace "$D/t.csv" --direct $DIRECT --depth 16 --out "$D/r.csv")
get() { echo "$out" | awk -F': *' -v k="$1" '$1 ~ k {print $2}' | head -1 | awk '{print $1}'; }
check "all requests completed"        "$(get '^requests')" "$N"
check "no errors"                     "$(get '^errors')" "0"
check "no short reads"                "$(get '^short reads')" "0"
check "bytes verified (0 failures)"   "$(get '^verify failures')" "0"
check "one CSV row per request"       "$(($(wc -l < "$D/r.csv") - 1))" "$N"
check "every CSV row verify_ok=1"     "$(tail -n +2 "$D/r.csv" | awk -F, '$8!=1' | wc -l | tr -d ' ')" "0"
bad=$(tail -n +2 "$D/r.csv" | awk -F, '$5<$4 || $6<$5' | wc -l | tr -d ' ')
check "submit >= arrival, complete >= submit" "$bad" "0"

# Depth 1 still completes everything (limit on outstanding reads works).
out=$(./run_bench --file "$D/test.bin" --trace "$D/t.csv" --direct $DIRECT --depth 1)
check "depth 1 completes all"         "$(get '^requests')" "$N"
check "depth 1 no failures"           "$(get '^verify failures')" "0"

# Corrupt one block: the harness must notice and exit non-zero.
cp "$D/test.bin" "$D/bad.bin"
# Damage the block read by the FIRST request in the trace, so the check never depends on the seed.
OFF=$(sed -n 2p "$D/t.csv" | cut -d, -f3)
printf '\377' | dd of="$D/bad.bin" bs=1 seek="$OFF" conv=notrunc 2>/dev/null
set +e
./run_bench --file "$D/bad.bin" --trace "$D/t.csv" --direct $DIRECT --depth 16 >"$D/bad.out"
rc=$?
set -e
check "corrupted file is detected (exit code non-zero)" "$([ $rc -ne 0 ] && echo yes || echo no)" "yes"

# A missing file gives a clear error, not a crash.
set +e; ./run_bench --file "$D/nope.bin" --trace "$D/t.csv" >/dev/null 2>&1; rc=$?; set -e
check "missing file reports an error" "$([ $rc -ne 0 ] && echo yes || echo no)" "yes"

rm -rf "$D"
[ $fail -eq 0 ] && echo "HARNESS TESTS PASSED" || { echo "HARNESS TESTS FAILED"; exit 1; }
