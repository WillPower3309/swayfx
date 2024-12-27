#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "util.h"

struct cmd_results *cmd_blur(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "blur", EXPECTED_AT_LEAST, 1);

	if (error) {
		return error;
	}

	struct sway_container *con = config->handler_context.container;

	bool result = parse_boolean(argv[0], true);
	if (con == NULL) {
		config->blur_enabled = result;
	} else {
		con->blur_enabled = result;
		container_update(con);
	}

    /* TODO
	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		struct fx_effect_framebuffers *effect_fbos =
			fx_effect_framebuffers_try_get(output->wlr_output);
		effect_fbos->blur_buffer_dirty = true;
		output_damage_whole(output);
	}
	*/

	return cmd_results_new(CMD_SUCCESS, NULL);
}
