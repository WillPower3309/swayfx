#include "sway/commands.h"

struct cmd_results *cmd_blur_noise(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "blur_noise", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	char *inv;
	float value = strtof(argv[0], &inv);
	if (*inv != '\0' || value < 0 || value > 1) {
		return cmd_results_new(CMD_FAILURE, "Invalid noise specified");
	}

	struct wlr_scene *root_scene = root->root_scene;
	wlr_scene_set_blur_noise(root_scene, value);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
