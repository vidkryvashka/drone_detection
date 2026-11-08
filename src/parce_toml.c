#include "parce_toml.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

typedef struct toml_entry_t {
	char *key;
	toml_datum_t val;
	struct toml_entry_t *next;
} toml_entry_t;

typedef struct toml_str_t {
	char *str;
	struct toml_str_t *next;
} toml_str_t;

typedef struct toml_doc_t {
	toml_entry_t *entries;
	toml_str_t *strings;
} toml_doc_t;

static char *doc_strdup(toml_doc_t *doc, const char *s) {
	if (!s) return NULL;
	char *copy = strdup(s);
	if (!copy) return NULL;
	toml_str_t *node = malloc(sizeof(*node));
	if (node) {
		node->str = copy;
		node->next = doc->strings;
		doc->strings = node;
	}
	return copy;
}

static char *trim(char *s) {
	while (isspace((unsigned char)*s)) s++;
	if (!*s) return s;
	char *end = s + strlen(s) - 1;
	while (end > s && isspace((unsigned char)*end)) end--;
	end[1] = '\0';
	return s;
}

static toml_datum_t parse_val(toml_doc_t *doc, const char *s) {
	toml_datum_t d = {0};
	if (!s || !*s) return d;
	size_t len = strlen(s);
	if ((s[0] == '"' && s[len - 1] == '"') || (s[0] == '\'' && s[len - 1] == '\'')) {
		char *buf = doc_strdup(doc, s + 1);
		if (buf) buf[len - 2] = '\0';
		d.type = TOML_STRING;
		d.u.s = buf;
		return d;
	}
	if (!strcmp(s, "true") || !strcmp(s, "false")) {
		d.type = TOML_BOOLEAN;
		d.u.boolean = (s[0] == 't');
		return d;
	}
	char *end = NULL;
	int64_t ival = strtoll(s, &end, 0);
	if (end && !*end) {
		d.type = TOML_INT64;
		d.u.int64 = ival;
		return d;
	}
	double fval = strtod(s, &end);
	if (end && !*end) {
		d.type = TOML_FP64;
		d.u.fp64 = fval;
		return d;
	}
	d.type = TOML_STRING;
	d.u.s = doc_strdup(doc, s);
	return d;
}

static void add_entry(toml_doc_t *doc, const char *key, toml_datum_t val) {
	for (toml_entry_t *e = doc->entries; e; e = e->next) {
		if (!strcmp(e->key, key)) { e->val = val; return; }
	}
	toml_entry_t *e = malloc(sizeof(*e));
	if (e) {
		e->key = doc_strdup(doc, key);
		e->val = val;
		e->next = doc->entries;
		doc->entries = e;
	}
}

static void parse_line(toml_doc_t *doc, char *line, char *cur_sec, size_t sec_sz) {
	bool in_q = false;
	for (int i = 0; line[i]; i++) {
		if (line[i] == '"' || line[i] == '\'') in_q = !in_q;
		else if (!in_q && line[i] == '#') { line[i] = '\0'; break; }
	}
	char *p = trim(line);
	if (!*p) return;
	if (*p == '[' && p[strlen(p) - 1] == ']') {
		p[strlen(p) - 1] = '\0';
		p = trim(p + 1);
		if (*p == '[') p = trim(p + 1);
		if (p[strlen(p) - 1] == ']') p[strlen(p) - 1] = '\0';
		strncpy(cur_sec, trim(p), sec_sz - 1);
		cur_sec[sec_sz - 1] = '\0';
		return;
	}
	char *eq = strchr(p, '=');
	if (!eq) return;
	*eq = '\0';
	char *k = trim(p), *v = trim(eq + 1);
	if (!*k) return;
	char full_key[512];
	if (cur_sec[0]) snprintf(full_key, sizeof(full_key), "%s.%s", cur_sec, k);
	else snprintf(full_key, sizeof(full_key), "%s", k);
	add_entry(doc, full_key, parse_val(doc, v));
}

toml_result_t toml_parse_file(FILE *fp) {
	toml_result_t res = {0};
	if (!fp) { snprintf(res.errmsg, sizeof(res.errmsg), "FILE pointer is NULL"); return res; }
	toml_doc_t *doc = calloc(1, sizeof(*doc));
	if (!doc) { snprintf(res.errmsg, sizeof(res.errmsg), "out of memory"); return res; }

	char line[1024], cur_sec[256] = "";
	while (fgets(line, sizeof(line), fp)) {
		parse_line(doc, line, cur_sec, sizeof(cur_sec));
	}
	res.ok = true;
	res.__internal = doc;
	res.toptab.type = TOML_TABLE;
	res.toptab.__internal = doc;
	return res;
}

toml_result_t toml_parse_file_ex(const char *fname) {
	toml_result_t res = {0};
	if (!fname) { snprintf(res.errmsg, sizeof(res.errmsg), "filename is NULL"); return res; }
	FILE *fp = fopen(fname, "r");
	if (!fp) {
		snprintf(res.errmsg, sizeof(res.errmsg), "cannot open \"%s\": %s", fname, strerror(errno));
		return res;
	}
	res = toml_parse_file(fp);
	fclose(fp);
	return res;
}

toml_result_t toml_parse(const char *src, int len) {
	toml_result_t res = {0};
	if (!src) { snprintf(res.errmsg, sizeof(res.errmsg), "source is NULL"); return res; }
	toml_doc_t *doc = calloc(1, sizeof(*doc));
	if (!doc) { snprintf(res.errmsg, sizeof(res.errmsg), "out of memory"); return res; }

	int n = (len >= 0) ? len : (int)strlen(src), pos = 0;
	char line[1024], cur_sec[256] = "";
	while (pos < n) {
		int start = pos;
		while (pos < n && src[pos] != '\n' && src[pos] != '\r') pos++;
		int l = pos - start;
		if (l >= (int)sizeof(line)) l = sizeof(line) - 1;
		memcpy(line, src + start, l);
		line[l] = '\0';
		while (pos < n && (src[pos] == '\n' || src[pos] == '\r')) pos++;
		parse_line(doc, line, cur_sec, sizeof(cur_sec));
	}
	res.ok = true;
	res.__internal = doc;
	res.toptab.type = TOML_TABLE;
	res.toptab.__internal = doc;
	return res;
}

void toml_free(toml_result_t result) {
	toml_doc_t *doc = (toml_doc_t *)result.__internal;
	if (!doc) return;
	toml_entry_t *e = doc->entries;
	while (e) {
		toml_entry_t *next = e->next;
		free(e);
		e = next;
	}
	toml_str_t *s = doc->strings;
	while (s) {
		toml_str_t *next = s->next;
		free(s->str);
		free(s);
		s = next;
	}
	free(doc);
}

toml_datum_t toml_get(toml_datum_t table, const char *key) {
	toml_datum_t res = {0};
	toml_doc_t *doc = (toml_doc_t *)table.__internal;
	if (!doc || !key) return res;

	char full[512];
	if (table.source && table.source[0]) snprintf(full, sizeof(full), "%s.%s", table.source, key);
	else snprintf(full, sizeof(full), "%s", key);

	for (toml_entry_t *e = doc->entries; e; e = e->next) {
		if (!strcmp(e->key, full)) return e->val;
	}
	size_t flen = strlen(full);
	for (toml_entry_t *e = doc->entries; e; e = e->next) {
		if (!strncmp(e->key, full, flen) && e->key[flen] == '.') {
			res.type = TOML_TABLE;
			res.__internal = doc;
			res.source = doc_strdup(doc, full);
			return res;
		}
	}
	return res;
}

toml_datum_t toml_seek(toml_datum_t table, const char *keyseq) {
	return toml_get(table, keyseq);
}
