#pragma once


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>



/* read a configuration file from the proper directory, or return NULL */
extern char *configReadFile(char *name);
/* read a configuration file, into an array of strings representing each line*/
extern char **configReadLines(char *name);

/* the following make an attempt to read a configuration file, but will allow
 * specifying a default if it is not present or otherwise borked */

/* read a string, *including* any newlines */
extern char *configTryStringFull(char *name, char *def);
/* read a string, *trimmed* of the newline */
extern char *configTryString(char *name, char *def);
extern long configTryLong(char *name, long def);
/* read a bool */
extern bool configTryBool(char *name, bool def);

