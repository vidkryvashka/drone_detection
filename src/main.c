#include "defs.h"

#define TAG "main "


int main(int argc, char **argv) {
	config_t conf = {0};
	if (parse_conf(argc, argv, &conf)) {
		ddloge(TAG, "parse_conf failed");
		return EINVAL;
	}

	apply_io_mode(&conf);

	return OK;
}
