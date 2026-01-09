#include <assert.h>
#include <stdlib.h>
#include <strings.h>
#include "sway/animation_manager.h"
#include "sway/commands.h"

struct cmd_results *cmd_animation_duration_ms(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "animation_duration_ms", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	char *err;
	float val = strtof(argc == 1 ? argv[0] : argv[1], &err);
	if (*err) {
		return cmd_results_new(CMD_INVALID, "animation_duration_ms float invalid");
	}

	if (val < 0 || val > 5000) { // surely no one wants an animation longer than 5 seconds
		return cmd_results_new(CMD_FAILURE, "animation_duration_ms value out of bounds");
	}

	config->animation_duration_ms = val;
	refresh_animation_manager_timing();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
