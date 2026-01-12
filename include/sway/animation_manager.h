#ifndef _SWAY_ANIMATION_MANAGER_H
#define _SWAY_ANIMATION_MANAGER_H

#include <stdbool.h>
#include <wayland-util.h>

struct sway_server;

struct animation {
	struct wl_list link;
	float progress;
	float multiplier;
	bool initialized;
};

void animation_manager_init(struct sway_server *server);

struct animation init_animation();

void refresh_animation_manager_timing();

void add_animation(struct animation *animation);

void start_animations(void (update_callback)(void));

float get_animated_value(float from, float to, struct animation animation);

#endif
