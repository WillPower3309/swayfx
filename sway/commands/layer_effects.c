#include <ctype.h>
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/layer_criteria.h"
#include "sway/output.h"
#include "util.h"

struct cmd_results *cmd_layer_effects(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "layer_effects", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	struct layer_criteria *criteria = malloc(sizeof(struct layer_criteria));
	criteria->namespace = malloc(strlen(argv[0]) + 1);
	strcpy(criteria->namespace, argv[0]);
	criteria->cmdlist = join_args(argv + 1, argc - 1);

	// Check if the rule already exists
	if (layer_criteria_already_exists(criteria)) {
		sway_log(SWAY_DEBUG, "layer_effect already exists: '%s' '%s'",
				criteria->namespace, criteria->cmdlist);
		layer_criteria_destroy(criteria);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	list_add(config->layer_criteria, criteria);
	sway_log(SWAY_DEBUG, "layer_effect: '%s' '%s' added", criteria->namespace, criteria->cmdlist);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
