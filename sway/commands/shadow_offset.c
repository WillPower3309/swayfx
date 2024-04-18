#include <string.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *cmd_shadow_offset(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "shadow_offset", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	char *err;
	float offset_x = strtof(argv[0], &err);
	float offset_y = strtof(argv[1], &err);
	if (*err || offset_x < -99.9f || offset_x > 99.9f) {
		return cmd_results_new(CMD_INVALID, "x offset float invalid");
	}
	if (*err || offset_y < -99.9f || offset_y > 99.9f) {
		return cmd_results_new(CMD_INVALID, "y offset float invalid");
	}

	config->shadow_offset_x = offset_x;
	config->shadow_offset_y = offset_y;

	return cmd_results_new(CMD_SUCCESS, NULL);
}

