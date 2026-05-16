#ifndef DEFS_H
#define DEFS_H

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#define	OK 0

#define ddlogi(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#define ddlogw(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "\033[33m" "warning " "\033[0m" "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#define ddloge(_ddtag, _ddmsg, ...)	\
	printf(_ddtag "\033[31m" "error " "\033[0m" "%s: " _ddmsg "\n", __func__, ##__VA_ARGS__)

#endif
