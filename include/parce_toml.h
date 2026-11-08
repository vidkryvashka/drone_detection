#ifndef PARCE_TOML_H
#define PARCE_TOML_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

enum toml_type_t {
	TOML_UNKNOWN = 0,
	TOML_STRING,
	TOML_INT64,
	TOML_FP64,
	TOML_BOOLEAN,
	TOML_TABLE
};
typedef enum toml_type_t toml_type_t;

typedef struct toml_datum_t {
	toml_type_t type;
	const char *source;
	union {
		const char *s;
		int64_t int64;
		double fp64;
		bool boolean;
	} u;
	void *__internal;
} toml_datum_t;

typedef struct toml_result_t {
	bool ok;
	toml_datum_t toptab;
	char errmsg[200];
	void *__internal;
} toml_result_t;

toml_result_t toml_parse(const char *src, int len);
toml_result_t toml_parse_file(FILE *fp);
toml_result_t toml_parse_file_ex(const char *fname);
void toml_free(toml_result_t result);
toml_datum_t toml_get(toml_datum_t table, const char *key);
toml_datum_t toml_seek(toml_datum_t table, const char *keyseq);

#endif
