#ifndef _SWAY_ANIMATION_MANAGER_H
#define _SWAY_ANIMATION_MANAGER_H

#include "list.h"
#include "sway/tree/container.h"

// TODO: have target var so we can have the same struct work w alpha and pos
// then rename from_alpha and to_alpha to from and to
struct container_animation_state {
	float from_alpha;
	float to_alpha;
	double progress;
	bool is_being_animated;
	struct sway_container *container;
};

struct animation_manager {
	struct wl_event_source *tick;
	list_t *animated_states; // struct container_animation_state
};

struct animation_manager *animation_manager_create();

void add_container_animation(struct container_animation_state *animation_state,
		struct animation_manager *animation_manager);

#endif
