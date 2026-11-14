#!/bin/sh

set -u

LOG=build/test.log
INPUT=build/test.in
TIMEOUT=20

failed=0

fail() {
    echo "FAIL $1 (see $LOG)"
    kill $qemu 2>/dev/null
    exit 1
}

wait_for() {
    elapsed=0

    while ! grep -q "$1" "$LOG"; do
        if grep -q 'panic: ' "$LOG"; then
            fail "kernel panic"
        fi

        if [ $elapsed -ge $((TIMEOUT * 10)) ] || ! kill -0 $qemu 2>/dev/null; then
            fail "'$1' did not appear within $TIMEOUT seconds"
        fi

        sleep 0.1
        elapsed=$((elapsed + 1))
    done
}

wait_exit() {
    elapsed=0

    while kill -0 $qemu 2>/dev/null; do
        if [ $elapsed -ge $((TIMEOUT * 10)) ]; then
            fail "QEMU did not power off within $TIMEOUT seconds"
        fi

        sleep 0.1
        elapsed=$((elapsed + 1))
    done

    wait $qemu
}

check() {
    if grep -q "$2" "$LOG"; then
        echo "ok   $1"
    else
        echo "FAIL $1"
        failed=1
    fi
}

rm -f "$INPUT"
mkfifo "$INPUT"
: > "$LOG"

"$@" < "$INPUT" > "$LOG" 2>&1 &
qemu=$!

exec 3> "$INPUT"

wait_for '^> '
printf 'pages\rhello\rpages\rcount\rpages\rfault\rpages\rfoo\rpages\rpoweroff\r' >&3
wait_exit
status=$?
exec 3>&-

if grep -q 'panic: ' "$LOG"; then
    fail "kernel panic"
fi

check boot    'Hello World from Arblem'
check hello   'Hello World from U-mode!'
check count   'count: 4'
check fault   'exit: -1'
check unknown 'Unknown command'

if [ "$(grep -c '^0x' "$LOG")" -eq 5 ] && [ "$(grep '^0x' "$LOG" | sort -u | wc -l)" -eq 1 ]; then
    echo "ok   pages"
else
    echo "FAIL pages"
    failed=1
fi

if [ $status -eq 0 ]; then
    echo "ok   poweroff"
else
    echo "FAIL poweroff (exit status $status)"
    failed=1
fi

exit $failed
