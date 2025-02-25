#ifndef _SWAY_ANIMATION_MANAGER_H
#define _SWAY_ANIMATION_MANAGER_H

#include "list.h"
#include "sway/tree/container.h"

struct animation_manager {
	struct wl_event_source *tick;
	list_t *animated_views; // struct sway_view
};

struct animation_manager *animation_manager_create();

void add_view_animation(struct sway_view *view, struct animation_manager *animation_manager);

#endif
