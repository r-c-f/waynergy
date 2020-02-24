#pragma once


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>



/* read a configuration file from the proper directory, or return NULL */
extern char *configReadFile(char *name);
/* read a configuration file, but set a default string value if it is not 
 * read */
extern char *configTryString(char *name, char *def);
extern long configTryLong(char *name, long def);
extern char **configReadLines(char *name);
