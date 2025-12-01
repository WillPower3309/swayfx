#include <stdlib.h>
#include <string.h>

#include "sway/commands.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"

struct cmd_results* read_int(char *input, int *output, const char * name);

struct cmd_results* cmd_window_button_style(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "window_button_style", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	int arg = 0;
	while ((arg+1) < argc) {
		char *name = argv[arg];
		char *value = argv[arg+1];

		if (strcmp(name, "gaps") == 0) {
			if ((error = read_int(value, &config->window_button_gaps, "gaps"))) {
				return error;
			}
		}

		if (strcmp(name, "align") == 0) {
			if (strcmp(value, "top") == 0) {
				config->window_button_vertical_align = V_ALIGN_TOP;
			}

			if (strcmp(value, "middle") == 0) {
				config->window_button_vertical_align = V_ALIGN_MIDDLE;
			}

			if (strcmp(value, "bottom") == 0) {
				config->window_button_vertical_align = V_ALIGN_BOTTOM;
			}
		}

		if (strcmp(name, "margin") == 0) {
			if ((error = read_int(value, &config->window_button_margin, "margin"))) {
				return error;
			}
		}

		if (strcmp(name, "size") == 0) {
			if ((error = read_int(value, &config->window_button_size, "size"))) {
				return error;
			}
		}

		if (strcmp(name, "radius") == 0) {
			if ((error = read_int(value, &config->window_button_radius, "radius"))) {
				return error;
			}
		}

		arg += 2;
	}

	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		arrange_workspace(output_get_active_workspace(output));
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results* read_int(char *input, int *output, const char *name) {
	char *inv;
	int v = strtol(input, &inv, 10);
	if (*inv != '\0' || v < 0) {
		return cmd_results_new(CMD_FAILURE, "invalid window_button_style %s given, invalid int", name);
	}

	*output = v;
	return NULL;
}