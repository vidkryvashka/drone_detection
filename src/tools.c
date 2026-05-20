#include <stdio.h>
#include "defs.h"



void print_progress_bar(
	const char *prefix,
	const size_t current,
	const size_t total
) {
	if (total == 0)
		return;

	int percentage = (int)(current * 100 / total);
	int filled = (int)(current * PROGRESS_BAR_WIDTH / total);

	printf("\r%s:\t[", prefix);
	for (int j = 0; j < PROGRESS_BAR_WIDTH; j++) {
		if (j < filled) printf("#");
		else printf(" ");
	}
	printf("] %d%% (%zu/%zu)", percentage, current, total);
	fflush(stdout);
}
