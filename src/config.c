#include "config.h"
#include "os.h"
#include "xmem.h"
#include "fdio_full.h"
#include "log.h"
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>

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
static char *read_file_dumb(char *path)
{
	size_t len, pos;
	char *buf;

	len = 4096;
	buf = xmalloc(len);
	pos = 0;
	if (!buf_append_file(&buf, &len, &pos, path)) {
		free(buf);
		return NULL;
	}
	buf[pos] = 0;
	return xrealloc(buf, pos + 1);
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
			break;
		}
	}
	if (section_buf) {
		if ((section = ini_find_section(config_ini, section_buf, strlen(section_buf))) == INI_NOT_FOUND) {
			logDbg("Section %s not found in INI", section_buf);
			goto done;
		}
	}
	if ((prop = ini_find_property(config_ini, section, prop_buf, strlen(prop_buf))) == INI_NOT_FOUND) {
		logDbg("Property %s not found in INI", prop_buf);
		goto done;
	}
	if (!(val = ini_property_value(config_ini, section, prop))) {
		logDbg("Could not retrieve INI value");
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
static int read_full_section_dir(char *name, char ***key, char ***val)
{
	int ret = 0, count = 0;
	DIR *dir = NULL;
	struct dirent *ent;
	char *dir_path = NULL;
	char *path = NULL;
	struct stat sbuf;
	dir_path = osGetHomeConfigPath(name);
	if (!(dir = opendir(dir_path))) {
		ret = -1;
		goto done;
	}
	*key = xmalloc(sizeof((*key)[0]));
	*val = xmalloc(sizeof((*val)[0]));
	while ((ent = readdir(dir))) {
		xasprintf(&path, "%s/%s", dir_path, ent->d_name);
	       	if (stat(path, &sbuf) == -1) {
			ret = -1;
			goto done;
		}
		if (!S_ISREG(sbuf.st_mode)) {
			free(path);
			path = NULL;
			continue;
		}
		*key = xreallocarray(*key, ++count + 1, sizeof((*key)[0]));
		*val = xreallocarray(*val, count + 1, sizeof((*val)[0]));
		(*key)[count - 1] = strdup(ent->d_name);
		(*val)[count - 1] = read_file_dumb(path);
		free(path);
		path = NULL;
	}
	(*key)[count] = NULL;
	(*val)[count] = NULL;
	ret = count;
	goto done;
done:
	free(dir_path);
	return ret;
}

static int read_full_section_ini(char *name, char ***key, char ***val)
{
	int count, i, section;
	if (!config_ini) {
		return -1;
	}
	if (!name) {
		section = INI_GLOBAL_SECTION;
	} else {
		section = ini_find_section(config_ini, name, strlen(name));
	}
	if (section == INI_NOT_FOUND) {
		logDbg("Could not find section %s", name);
		return -1;
	}
	count = ini_property_count(config_ini, section);
	*key = xcalloc(count + 1, sizeof((*key)[0]));
	*val = xcalloc(count + 1, sizeof((*val)[0]));
	for (i = 0; i < count; ++i) {
		(*key)[i] = strdup(ini_property_name(config_ini, section, i));
		(*val)[i] = strdup(ini_property_value(config_ini, section, i));
	}
	return count;
}

int configReadFullSection(char *name, char ***key, char ***val)
{
	int count;
	*val = NULL;
	*key = NULL;
	if ((count = read_full_section_ini(name, key, val)) != -1) {
		return count;
	}
	return read_full_section_dir(name, key, val);
}

char *configReadFile(char *name)
{
	char *buf, *path;

	//We try the INI approach first
	if ((buf = try_read_ini(name))) {
		logDbg("Using INI value for %s: %s", name, buf);
		return buf;
	}
	//and proceed from there.
	if (!(path = osGetHomeConfigPath(name)))
		return NULL;
	buf = read_file_dumb(path);
	free(path);
	return buf;
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
			len *= 3;
			len /= 2;
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

/* combine ini files, overwriting conflicting values */
static void ini_cat(ini_t *dst, ini_t *src)
{
	int sec_src, sec_dst, prop_src, prop_dst;
	const char *sec_name, *prop_name;
	for (sec_src = 0; sec_src < ini_section_count(src); ++sec_src) {
		sec_name = ini_section_name(src, sec_src);
		if ((sec_dst = ini_find_section(dst, sec_name, 0)) == INI_NOT_FOUND) {
			sec_dst = ini_section_add(dst, sec_name, 0);
		}
		for (prop_src = 0; prop_src < ini_property_count(src, sec_src); ++prop_src) {
			prop_name = ini_property_name(src, sec_src, prop_src);
			if ((prop_dst = ini_find_property(dst, sec_dst, prop_name, 0)) == INI_NOT_FOUND) {
				ini_property_add(dst, sec_dst, prop_name, 0, ini_property_value(src, sec_src, prop_src), 0);
			} else {
				ini_property_value_set(dst, sec_dst, prop_dst, ini_property_value(src, sec_src, prop_src), 0);
			}
		}
	}
}

/* read all INI files from config.ini.d, combining them into a destination
 *
 * slightly hackish because it's treating 'config.ini.d' as a section*/
static bool ini_d_load(ini_t *dst)
{
	char *suffix;
	char **name, **txt;
	int count, i;
	ini_t *src;

	if ((count = read_full_section_dir("config.ini.d", &name, &txt)) == -1) {
		return false;
	}

	for (i = 0; i < count; ++i) {
		/* we only want INI files */
		if ((suffix = strstr(name[i], ".ini")) && *(suffix + 4) == '\0') {
			if ((src = ini_load(txt[i], NULL))) {
				ini_cat(dst, src);
				ini_destroy(src);
			}
		}
	}
	return true;
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
	if (!ini_d_load(config_ini)) {
		logWarn("Could not read ini.d configurations");
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
	if (!osMakeParentDir(path, S_IRWXU)) {
		logPErr("Could not create parent directory structure");
		free(path);
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
