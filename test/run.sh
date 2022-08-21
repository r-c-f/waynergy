#!/bin/sh

cc -D_GNU_SOURCE -DWAYNERGY_TEST -g -I../include os.c ../src/os.c ../src/log.c
if ./a.out; then
	echo "os.c: passed"
else
	echo "os.c: failed"
fi
