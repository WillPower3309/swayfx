#include "sway/commands.h"

struct cmd_results *cmd_blur_passes(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "blur_passes", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	char *inv;
	int value = strtol(argv[0], &inv, 10);
	if (*inv != '\0' || value < 0 || value > 10) {
		return cmd_results_new(CMD_FAILURE, "Invalid number of passes specified");
	}

	struct wlr_scene *root_scene = root->root_scene;
	wlr_scene_set_blur_num_passes(root_scene, value);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
