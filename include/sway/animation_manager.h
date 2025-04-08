#ifndef _SWAY_ANIMATION_MANAGER_H
#define _SWAY_ANIMATION_MANAGER_H

#include "sway/tree/container.h"

struct container_animation_state {
	struct wl_list link;
	bool init;
	float progress;
	struct sway_container *container;

	// TODO: make this an animated vars list similar to before?
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
	void (*complete)(struct sway_container *con);
};

struct animation_manager {
	struct wl_event_source *tick;
	struct wl_list animation_states; // struct container_animation_state
};

struct animation_manager *animation_manager_create();

struct container_animation_state container_animation_state_create_fadein(struct sway_container *con);

struct container_animation_state container_animation_state_create_fadeout(struct sway_container *con);

void add_container_animation(struct container_animation_state *animation_state,
		struct animation_manager *animation_manager);

void cancel_container_animation(struct container_animation_state *animation_state);

#endif
