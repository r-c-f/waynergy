#include "config.h"
#include "xmem.h"
#include <stdbool.h>
static char *config_home(void)
{
	static char *path;
	char *env;
	if (!path) {
		if (env = getenv("XDG_CONFIG_HOME")) {
			xasprintf(&path,"%s/swaynergy",env);
		} else {
			if (!(env = getenv("HOME")))
				return NULL;
			xasprintf(&path, "%s/.config/swaynergy",env);
		}
	}
	return path;
}

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
char *configReadFile(char *name)
{
	size_t len, pos;
	char *buf, *path;

	xasprintf(&path, "%s/%s", config_home(), name);

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

	xasprintf(&path, "%s/%s", config_home(), name);
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

	



char *configTryString(char *name, char *def)
{
	char *output = configReadFile(name);
	return output ? output : (def ? xstrdup(def) : NULL);
}
long configTryLong(char *name, long def)
{
	char *s;
	long out;

	if ((s = configReadFile(name))) {
		errno = 0;
		out = strtol(s, NULL, 0);
		if (!errno)
			return out;
	}
	return def;
}



