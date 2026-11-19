#!/bin/sh

# Smoke test: boot the kernel in QEMU, type commands into the shell, and check
# the console output. Run by `make test`, which passes the QEMU command line
# as the arguments. See docs/build-and-test.md.

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

# Wait until the log contains $1. Stop early on a panic, a timeout, or QEMU
# exiting.
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

# Wait for QEMU to exit, and return its exit status.
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

# Report check $1 as passed if the log contains $2.
check() {
    if grep -q "$2" "$LOG"; then
        echo "ok   $1"
    else
        echo "FAIL $1"
        failed=1
    fi
}

# QEMU reads the console from a named pipe, so the test can type into it.
rm -f "$INPUT"
mkfifo "$INPUT"
: > "$LOG"

"$@" < "$INPUT" > "$LOG" 2>&1 &
qemu=$!

# Keep the pipe open for writing, so QEMU does not see end of input.
exec 3> "$INPUT"

# Type every command at once after the first prompt. The shell reads them one
# by one; \r is what a terminal sends for Enter. `pages` runs between programs
# to catch leaked pages, `orphan` runs 7 times to check that orphans are
# freed, and `regcheck` checks that gp and tp are saved per program.
wait_for '^> '
printf 'pages\rhello\rpages\r' >&3
printf 'orphan\rorphan\rorphan\rorphan\rorphan\rorphan\rorphan\r' >&3
printf 'regcheck\rpipecheck\rpages\rcount\rpages\rfault\rpages\rfoo\rpages\rpoweroff\r' >&3
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
check regs    'regs: ok'
check pipe    'pipe: ok'

# hello runs once by itself and once from each of the 7 orphans. Without
# orphan reaping, the thread table fills up after 5 orphans.
if [ "$(grep -c 'Hello World from U-mode!' "$LOG")" -eq 8 ]; then
    echo "ok   orphan"
else
    echo "FAIL orphan"
    failed=1
fi

# Six `pages` outputs, all the same: no program leaked memory.
if [ "$(grep -c '^0x' "$LOG")" -eq 6 ] && [ "$(grep '^0x' "$LOG" | sort -u | wc -l)" -eq 1 ]; then
    echo "ok   pages"
else
    echo "FAIL pages"
    failed=1
fi

# A clean shutdown through `poweroff` makes QEMU exit with status 0.
if [ $status -eq 0 ]; then
    echo "ok   poweroff"
else
    echo "FAIL poweroff (exit status $status)"
    failed=1
fi

exit $failed
