#ifndef _SWAY_ANIMATION_MANAGER_H
#define _SWAY_ANIMATION_MANAGER_H

#include <stdbool.h>
#include <wayland-util.h>

struct sway_container;
struct sway_server;

// TODO: make animation just a pointer to progress
// keep multiplier and callback private
struct animation {
	struct wl_list link;
	float progress;
	struct sway_container *con;
	float multiplier;
	bool initialized;
	void (*complete)(struct sway_container *);
};

void animation_manager_init(struct sway_server *server);

struct animation init_animation(struct sway_container *con);

void refresh_animation_manager_timing();

void add_animation(struct animation *animation, void (*complete_callback)(struct sway_container *));

void start_animations(void (update_callback)(void));

float get_animated_value(float from, float to, struct animation animation);

#endif
