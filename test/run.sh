#!/bin/sh

cc -D_GNU_SOURCE -DWAYNERGY_TEST -g -I../include os.c ../src/os.c ../src/log.c
if ./a.out; then
	echo "os.c: passed"
else
	echo "os.c: failed"
fi

cc -D_GNU_SOURCE -DWAYNERGY_TEST -g -I../include config.c ../src/os.c ../src/log.c ../src/config.c
if ./a.out; then
	echo "config.c: passed"
else
	echo "config.c: failed"
fi
