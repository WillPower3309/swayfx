#include "sway/commands.h"
#include "sway/tree/root.h"

struct cmd_results *cmd_blur_brightness(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "blur_brightness", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	char *inv;
	float value = strtof(argv[0], &inv);
	if (*inv != '\0' || value < 0 || value > 2) {
		return cmd_results_new(CMD_FAILURE, "Invalid brightness specified");
	}

	struct wlr_scene *root_scene = root->root_scene;
	struct blur_data blur_data = root_scene->blur_data;
	blur_data.brightness = value;
	wlr_scene_set_blur_data(root_scene, blur_data);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
