#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"

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

	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		if (output->renderer) {
			output->renderer->blur_buffer_dirty = true;
			output_damage_whole(output);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
