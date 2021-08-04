#include "config.h"
#include "os.h"
#include "xmem.h"
#include "fdio_full.h"
#include "log.h"
#include <stdbool.h>


#define INI_IMPLEMENTATION
#include "ini.h"
static ini_t *config_ini = NULL;

/* read file into a buffer, resizing as needed */
static bool buf_append_file(char **buf, size_t *len, size_t *pos, char *path)
{
        FILE *f;
        size_t read_count;
        if (!path)
                return false;
        if (!(f = fopen(path, "r"))) {
                return false;
        }
        while ((read_count = fread(*buf + *pos, 1, *len - *pos - 1, f))) {
                *pos += read_count;
                if (*len - *pos <= 2) {
                        *buf = xrealloc(*buf, *len *= 2);
                }
        }
        fclose(f);
        return true;
}
static char *try_read_ini(char *name)
{
	char *section_buf = NULL;
	char *prop_buf = name;
	const char *val = NULL;
	int section = INI_GLOBAL_SECTION;
	int prop;
	size_t i;

	if (!config_ini)
		return NULL;

	for (i = 0; name[i]; ++i) {
		if (name[i] == '/') {
			section_buf = xstrdup(name);
			section_buf[i] = '\0';
			prop_buf += i + 1;
		}
	}
	if (section_buf) {
		if ((section = ini_find_section(config_ini, section_buf, strlen(section_buf))) == INI_NOT_FOUND) {
			logInfo("Section %s not found in INI", section_buf);
			goto done;
		}
	}
	if ((prop = ini_find_property(config_ini, section, prop_buf, strlen(prop_buf))) == INI_NOT_FOUND) {
		logInfo("Property %s not found in INI", prop_buf);
		goto done;
	}
	if (!(val = ini_property_value(config_ini, section, prop))) {
		logInfo("Could not retrieve INI value");
		goto done;
	}
	if (val) {
		logDbg("Got value from INI: %s: %s", name, val);
	}
done:
	if (section_buf)
		free(section_buf);
	return val ? xstrdup(val) : NULL;
}

char *configReadFile(char *name)
{
	size_t len, pos;
	char *buf, *path;

	//We try the INI approach first
	if ((buf = try_read_ini(name))) {
		logDbg("Using INI value for %s: %s", name, buf);
		return buf;
	}
	//and proceed from there.
	if (!(path = osGetHomeConfigPath(name)))
		return NULL;

	len = 4096;
	buf = xmalloc(len);
	pos = 0;
	if (!buf_append_file(&buf, &len, &pos, path)) {
		free(buf);
		free(path);
		return NULL;
	}
	free(path);
	buf[pos] = 0;
	return xrealloc(buf, pos + 1);
}

char **configReadLines(char *name)
{
	char *path;
	size_t len = 24; //allocated;
	size_t pos = 0;
	size_t n; //line length -- probably don't need it
	char **line;
	FILE *f;

	if (!(path = osGetHomeConfigPath(name)))
		return NULL;
	if (!(f = fopen(path, "r"))) {
		free(path);
		return NULL;
	}
	line = xcalloc(sizeof(*line), len);
	for (pos = 0; ;++pos) {
		if (pos == len) {
			len += len;
			len /= 3;
			line = xrealloc(line, sizeof(*line) * len);
			memset(line + pos, 0, sizeof(*line) * (len - pos));
		}
		n = 0;
		if (getline(line + pos, &n, f) == -1) {
			break;
		}
	}
	line[pos] = NULL;
	free(path);
	return xrealloc(line, sizeof(*line) * (pos + 1));
}

bool configInitINI(void)
{
	char *buf;
	bool ret = true;

	if (!(buf = configReadFile("config.ini"))) {
		logWarn("Could not read INI configuration");
		return true;
	}
	if (!(config_ini = ini_load(buf, NULL))) {
		logErr("Could not load INI configuration");
		ret = false;
	}
	free(buf);
	return ret;
}

char *configTryStringFull(char *name, char *def)
{
	char *output = configReadFile(name);
	return output ? output : (def ? xstrdup(def) : NULL);
}
char *configTryString(char *name, char *def)
{
	char *output = configReadFile(name);
	if (!output)
		return xstrdup(def);
	/* remove the newline */
	for (char *c = output; *c; ++c) {
		if (*c == '\n') {
			//terminate it.
			*c = '\0';
			break;
		}
	}
	return output;
}

long configTryLong(char *name, long def)
{
	char *s;
	long out;

	if ((s = configReadFile(name))) {
		errno = 0;
		out = strtol(s, NULL, 0);
		free(s);
		if (!errno)
			return out;
	}
	return def;
}

bool configTryBool(char *name, bool def)
{
	char *s;
	bool out;

	if ((s = configReadFile(name))) {
		out = (bool)(
				strstr(s, "yes") ||
				strstr(s, "true") ||
				strstr(s, "on"));
		free(s);
		return out;
	}
	return def;
}

bool configWriteString(char *name, const char *val, bool overwrite)
{
	int fd, oflag;
	char *path;
	int ret;

	if (!(path = osGetHomeConfigPath(name))) {
		return false;
	}
	errno = 0;
	oflag = O_WRONLY | O_CREAT | (overwrite ? O_EXCL : 0);
	if ((fd = open(path, oflag, S_IRWXU)) == -1) {
		logPErr("Could not create config file");
		free(path);
		return false;
	}
	ret = write_full(fd, val, strlen(val), 0);
	close(fd);
	return ret;
}
