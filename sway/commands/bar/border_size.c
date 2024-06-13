#include <stdlib.h>
#include <string.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_border_size(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "border_size", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	int border_size = atoi(argv[0]);
	if (border_size < 0) {
		return cmd_results_new(CMD_INVALID,
				"Invalid border_size value: %s", argv[0]);
	}
	config->current_bar->border_size = border_size;
	sway_log(SWAY_DEBUG, "Setting bar border_size to %d on bar: %s",
			border_size, config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
