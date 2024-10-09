#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"

struct cmd_results *cmd_animation_duration(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "animation_duration", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	char *err;
	float value = strtof(argv[0], &err);
	if (*err || value < 0.0f || value > 1.0f) {
		return cmd_results_new(CMD_FAILURE, "animation_duration value invalid");
	}

	config->animation_duration = value;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
