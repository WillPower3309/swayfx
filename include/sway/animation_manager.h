#ifndef _SWAY_ANIMATION_MANAGER_H
#define _SWAY_ANIMATION_MANAGER_H

#include "list.h"
#include "sway/tree/container.h"

struct container_animation_state {
	float from_alpha;
	float to_alpha;
	double progress;
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
