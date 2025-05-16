#ifndef _SWAY_ANIMATION_MANAGER_H
#define _SWAY_ANIMATION_MANAGER_H

#include "sway/tree/container.h"

struct container_animation_state {
	struct wl_list link;
	bool init;
	float progress;
	struct sway_container *container;

	float from_alpha;
	float to_alpha;
	float from_blur_alpha;
	float to_blur_alpha;
	float from_x;
	float to_x;
	float from_y;
	float to_y;
	float from_width;
	float to_width;
	float from_height;
	float to_height;

	void (*update)(struct container_animation_state *animation_state);
	void (*complete)(struct container_animation_state *animation_state);
};

struct animation_manager {
	struct wl_event_source *tick;
	struct wl_list animation_states; // struct container_animation_state
	void (*tick_complete_callback)(void);
};

struct animation_manager *animation_manager_create();

// TODO: remove animation_manager struct?
void start_animation(struct container_animation_state *animation_state,
		struct animation_manager *animation_manager);

void finish_animation(struct container_animation_state *animation_state);

#endif
