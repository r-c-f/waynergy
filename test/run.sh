#!/bin/sh

# Note: previously each `cc` invocation and its `if ./a.out` check were
# separate statements, so a failed compile (cc exiting non-zero, leaving
# a.out untouched) would silently re-run and report on the *previous*
# test's stale binary instead of failing. Removing any leftover a.out
# before each compile is enough to fix that: on failure there's then
# nothing left to run, and `./a.out` on a missing file already fails on
# its own, same as any other test failure below.

rm -f a.out

cc -D_GNU_SOURCE -DWAYNERGY_TEST -g -I../include os.c ../src/os.c ../src/log.c
if ./a.out; then
	echo "os.c: passed"
else
	echo "os.c: failed"
fi
rm -f a.out

cc -D_GNU_SOURCE -DWAYNERGY_TEST -g -I../include config.c ../src/os.c ../src/log.c ../src/config.c
if ./a.out; then
	echo "config.c: passed"
else
	echo "config.c: failed"
fi
rm -f a.out

cc -D_GNU_SOURCE -DWAYNERGY_TEST -g -I../include keymap.c ../src/xkb_util.c ../src/log.c -lxkbcommon
if ./a.out; then
	echo "keymap.c: passed"
else
	echo "keymap.c: failed"
fi
rm -f a.out
