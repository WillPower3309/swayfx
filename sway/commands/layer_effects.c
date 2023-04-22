#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "util.h"

struct cmd_results *cmd_layer_effects(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "layer_effects", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	struct layer_effects *effect = malloc(sizeof(struct layer_effects));
	size_t len = sizeof(argv[0]);
	effect->namespace = malloc(len + 1);
	memcpy(effect->namespace, argv[0], len);
	effect->blur = false;
	effect->shadow = false;
	effect->corner_rounding = false;

	// Parse the commands
	for (int i = 1; i < argc; i++) {
		char *arg = argv[i];
		if (strcmp(arg, "blur") == 0) {
			effect->blur = true;
			continue;
		} else if (strcmp(arg, "shadow") == 0) {
			effect->shadow = true;
			continue;
		} else if (strcmp(arg, "corner_rounding") == 0) {
			effect->corner_rounding = true;
			continue;
		}
		return cmd_results_new(CMD_INVALID, "Invalid layer_effects effect! Got \"%s\"", arg);
	}

	if (!effect->blur && !effect->shadow && !effect->corner_rounding) {
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
		list_add(config->layer_effects, effect);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
