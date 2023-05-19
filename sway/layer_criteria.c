#include <ctype.h>
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/layer_criteria.h"
#include "util.h"

void layer_criteria_destroy(struct layer_criteria *criteria) {
	free(criteria->namespace);
	free(criteria->cmdlist);
	free(criteria);
}

bool layer_criteria_is_equal(struct layer_criteria *a, struct layer_criteria *b) {
	return strcmp(a->namespace, b->namespace) == 0
		&& strcmp(a->cmdlist, b->cmdlist) == 0;
}

bool layer_criteria_already_exists(struct layer_criteria *criteria) {
	list_t *criterias = config->layer_criteria;
	for (int i = 0; i < criterias->length; ++i) {
		struct layer_criteria *existing = criterias->items[i];
		if (layer_criteria_is_equal(criteria, existing)) {
			return true;
		}
	}
	return false;
}

list_t *layer_criterias_for_sway_layer_surface(struct sway_layer_surface *sway_layer) {
	list_t *criterias = config->layer_criteria;
	list_t *matches = create_list();
	for (int i = 0; i < criterias->length; ++i) {
		struct layer_criteria *criteria = criterias->items[i];
		if (strcmp(criteria->namespace, sway_layer->layer_surface->namespace) == 0) {
			list_add(matches, criteria);
		}
	}
	return matches;
}

void layer_criteria_parse(struct sway_layer_surface *sway_layer, struct layer_criteria *criteria) {
	char matched_delim = ';';
	char *head = malloc(strlen(criteria->cmdlist) + 1);
	strcpy(head, criteria->cmdlist);
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
			sway_layer->has_blur = parse_boolean(argv[1], true);
			continue;
		} if (strcmp(argv[0], "blur_ignore_transparent") == 0) {
			sway_layer->blur_ignore_transparent = parse_boolean(argv[1], true);
			continue;
		} else if (strcmp(argv[0], "shadows") == 0) {
			sway_layer->has_shadow = parse_boolean(argv[1], true);
			continue;
		} else if (strcmp(argv[0], "corner_radius") == 0) {
			int value;
			if (cmd_corner_radius_parse_value(argv[1], &value)) {
				sway_layer->corner_radius = value;
				continue;
			}
			sway_log(SWAY_ERROR,
					"Invalid layer_effects corner_radius size! Got \"%s\"",
					argv[1]);
			return;
		} else {
			sway_log(SWAY_ERROR, "Invalid layer_effects effect! Got \"%s\"", cmd);
			return;
		}
	} while(head);
}
