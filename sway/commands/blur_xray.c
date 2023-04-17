#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "util.h"

struct cmd_results *cmd_blur_xray(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "blur_xray", EXPECTED_AT_LEAST, 1);

	if (error) {
		return error;
	}

	bool result = parse_boolean(argv[0], config->blur_xray);
	config->blur_xray = result;

	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		if (output->renderer) {
			output->renderer->blur_buffer_dirty = true;
			output_damage_whole(output);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
