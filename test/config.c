#include "../include/os.h"
#include "../include/log.h"
#include "../include/config.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char **argv)
{
	int stat;

	char **lines;
	char *str;
	long l;
	bool b;

	osConfigPathOverride="./config";
	logInit(LOG_DBG, NULL);

	if (!configInitINI()) {
		logErr("Could not initialize config.ini");
		return 1;
	}
	if (!(str = configTryString("str", NULL))) {
		logErr("Could not read string");
		return 1;
	}
	if (strcmp("string", str)) {
		logErr("String value %s != string", str);
		return 1;
	}
	if ((l = configTryLong("long", 0)) != 1234) {
		logErr("Long value %d != 1234", l);
		return 1;
	}
	if (!(b = configTryBool("bool", false))) {
		logErr("Bool value incorrect");
		return 1;
	}
	if (!(lines = configReadLines("str"))) {
		logErr("Could not read lines for 'str'");
		return 1;
	}
	if (strcmp(lines[0], "string")) {
		logErr("Incorrect value for line: %s", lines[0]);
		return 1;
	}

	return 0;
}
