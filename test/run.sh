#!/bin/sh

cc -g -I../include os.c ../src/os.c _log.c 
exit $?;

