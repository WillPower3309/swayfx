#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/root.h"
#include "util.h"

static void arrange_blur_border_iter(struct sway_container *con, void *data) {
	con->blur_border = config->blur_border;
}

struct cmd_results *cmd_blur_border(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "blur_border", EXPECTED_AT_LEAST, 1);

	if (error) {
		return error;
	}

	struct sway_container *con = config->handler_context.container;

	bool result = parse_boolean(argv[0], true);
	if (con == NULL) {
		config->blur_border = result;

		// Config reload: reset all containers to config value
		root_for_each_container(arrange_blur_border_iter, NULL);
		arrange_root();
	} else {
		con->blur_border = result;
		arrange_container(con);
		output_configure_scene(NULL, &con->scene_tree->node,
		1.0f, 0, false, false, false, NULL);
	}

	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		wlr_scene_optimized_blur_mark_dirty(output->layers.blur_layer);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}