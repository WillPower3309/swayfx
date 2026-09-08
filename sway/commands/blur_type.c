#include "sway/commands.h"

struct cmd_results *cmd_blur_type(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "blur_type", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	struct wlr_scene *root_scene = root->root_scene;
	wlr_scene_set_blur_id(root_scene, argv[0]);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
