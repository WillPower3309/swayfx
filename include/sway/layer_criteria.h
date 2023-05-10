#include <stdbool.h>
#include "sway/layers.h"

struct layer_criteria {
	char *namespace;
	char *cmdlist;
};

void layer_criteria_destroy(struct layer_criteria *criteria);

bool layer_criteria_is_equal(struct layer_criteria *a, struct layer_criteria *b);

bool layer_criteria_already_exists(struct layer_criteria *criteria);

// Gathers all of the matching criterias for a specified `sway_layer_surface`
list_t *layer_criterias_for_sway_layer_surface(struct sway_layer_surface *sway_layer);

// Parses the `layer_criteria` and applies the effects to the `sway_layer_surface`
void layer_criteria_parse(struct sway_layer_surface *sway_layer, struct layer_criteria *criteria);
