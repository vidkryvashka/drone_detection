#include "defs.h"

#define TAG "main "


int main(int argc, char **argv) {
	main_conf_t conf = {0};
	if (parse_conf(argc, argv, &conf)) {
		ddloge(TAG, "parse_conf failed");
		free_conf(&conf);
		return EINVAL;
	}

	errno_t err = apply_io_mode(&conf);
	free_conf(&conf);

	return err;
}
