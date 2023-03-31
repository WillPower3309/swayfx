#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "util.h"

struct cmd_results *cmd_blur_radius(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "blur_radius", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	char *inv;
	int value = strtol(argv[0], &inv, 10);
	if (*inv != '\0' || value < 0 || value > 10) {
		return cmd_results_new(CMD_FAILURE, "Invalid size specified");
	}

	config->blur_params.radius = value;

	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
