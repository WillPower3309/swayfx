#include <ctype.h>
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "util.h"

struct cmd_results *parse_effects(int argc, char **argv, struct layer_effects *effect) {
	char matched_delim = ';';
	char *head = join_args(argv + 1, argc - 1);
	do {
		// Trim leading whitespaces
		for (; isspace(*head); ++head) {}
		// Split command list
		char *cmd = argsep(&head, ";,", &matched_delim);
		for (; isspace(*cmd); ++cmd) {}

		if (strcmp(cmd, "") == 0) {
			sway_log(SWAY_INFO, "Ignoring empty layer effect.");
			continue;
		}
		sway_log(SWAY_INFO, "Handling layer effect '%s'", cmd);

		int argc;
		char **argv = split_args(cmd, &argc);
		// Strip all quotes from each token
		for (int i = 1; i < argc; ++i) {
			if (*argv[i] == '\"' || *argv[i] == '\'') {
				strip_quotes(argv[i]);
			}
		}
		if (strcmp(argv[0], "blur") == 0) {
			effect->deco_data.blur = cmd_blur_parse_value(argv[1]);
			continue;
		} else if (strcmp(argv[0], "shadows") == 0) {
			effect->deco_data.shadow = cmd_shadows_parse_value(argv[1]);
			continue;
		} else if (strcmp(argv[0], "corner_radius") == 0) {
			int value;
			if (cmd_corner_radius_parse_value(argv[1], &value)) {
				effect->deco_data.corner_radius = value;
				continue;
			}
			return cmd_results_new(CMD_INVALID,
					"Invalid layer_effects corner_radius size! Got \"%s\"",
					argv[1]);
		} else {
			return cmd_results_new(CMD_INVALID,
					"Invalid layer_effects effect! Got \"%s\"",
					cmd);
		}
	} while(head);
	return NULL;
}

struct cmd_results *cmd_layer_effects(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "layer_effects", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	struct layer_effects *effect = malloc(sizeof(struct layer_effects));
	size_t len = sizeof(argv[0]);
	effect->namespace = malloc(len + 1);
	memcpy(effect->namespace, argv[0], len);
	effect->deco_data = get_undecorated_decoration_data();

	// Parse the commands
	if ((error = parse_effects(argc, argv, effect))) {
		return error;
	}

	// Ignore if nothing has changed
	// TODO: Add deco_data cmp function?
	if (!effect->deco_data.blur
			&& !effect->deco_data.shadow
			&& effect->deco_data.corner_radius < 1) {
		sway_log(SWAY_ERROR,
				"Ignoring layer effect \"%s\". Nothing changed\n",
				join_args(argv + 1, argc - 1));
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	// Check if the rule already exists
	list_t *effects = config->layer_effects;
	for (int i = 0; i < effects->length; ++i) {
		struct layer_effects *existing = effects->items[i];
		// Replace the duplicate entry
		if (strcmp(existing->namespace, effect->namespace) == 0) {
			memcpy(existing, effect, sizeof(struct layer_effects));
			free(effect);
			effect = NULL;
			break;
		}
	}

	if (effect) {
		list_add(effects, effect);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
