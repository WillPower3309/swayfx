#ifndef _SWAY_ANIMATION_MANAGER_H
#define _SWAY_ANIMATION_MANAGER_H

#include "list.h"
#include "sway/tree/container.h"

/* TODO: add an array of the below to the animation state
struct animated_var {
	float from;
	float to;
	float *current;
};
 */

struct container_animation_state {
	struct wl_list link;
	bool init;
	float from;
	float to;
	float *current;
	float progress;
	struct sway_container *container;
	void (*update)(struct sway_container *con);
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

void cancel_container_animation(struct container_animation_state *animation_state,
		struct animation_manager *animation_manager);

#endif
