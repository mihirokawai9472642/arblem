#!/bin/sh

set -u

NAME=$1
shift

LOG=build/test-$NAME.log
INPUT=build/test-$NAME.in
TIMEOUT=20

failed=0

fail() {
    echo "FAIL $NAME: $1 (see $LOG)"
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
printf 'pages\rhello\rpages\rcount\rpages\rfoo\rpages\rtest-end\r' >&3
wait_for 'test-end'

kill $qemu
wait $qemu 2>/dev/null
exec 3>&-

echo "== $NAME"

check boot    'Hello World from Arblem'
check hello   'Hello World from U-mode!'
check count   'count: 4'
check unknown 'Unknown command'

if [ "$(grep -c '^0x' "$LOG")" -eq 4 ] && [ "$(grep '^0x' "$LOG" | sort -u | wc -l)" -eq 1 ]; then
    echo "ok   pages"
else
    echo "FAIL pages"
    failed=1
fi

exit $failed
