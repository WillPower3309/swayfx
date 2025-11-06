#include <string.h>

#include "util.h"
#include "sway/commands.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"

struct cmd_results *cmd_titlebar_blur(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "titlebar_blur", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	config->titlebar_blur = parse_boolean(argv[0], config->titlebar_blur);
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		arrange_workspace(output_get_active_workspace(output));
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
