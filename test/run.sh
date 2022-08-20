#!/bin/sh

cc -D_GNU_SOURCE -g -I../include os.c ../src/os.c _log.c
if ./a.out; then
	echo "os.c: passed"
else
	echo "os.c: failed"
fi
