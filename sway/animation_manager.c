#include <assert.h>
#include <wayland-util.h>
#include "sway/animation_manager.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/root.h"

struct animation {
	float progress;
	float multiplier;
	void (*update)(void);
	void (*complete)(void);
};

struct animation_manager {
	float tick_time;
	float progress_delta;
	struct wl_event_source *tick;
	struct animation current_animation;
} animation_manager;

float ease_out_cubic(float t) {
	float p = t - 1;
	return pow(p, 3) + 1;
}

int animation_timer(void *data) {
	struct animation *animation = data;

	animation->progress = MIN(animation->progress + animation_manager.progress_delta, 1.0f);
	animation->multiplier = ease_out_cubic(animation->progress);

	if (animation->update) {
		animation->update();
	}

	if (animation->progress < 1.0f) {
		wl_event_source_timer_update(animation_manager.tick,
				animation_manager.tick_time);
	} else if (animation->complete) {
		animation->complete();
	}
	return 0;
}

void start_animation(void (update_callback)(void), void (complete_callback)(void)) {
	if (!config->animation_duration_ms) {
		animation_manager.current_animation = (struct animation) {
			.progress = 1.0f,
			.multiplier = 1.0f,
			.update = NULL,
			.complete = NULL,
		};
		return;
	}

	assert(animation_manager.tick);

	animation_manager.current_animation = (struct animation) {
		.progress = 0.0f,
		.multiplier = 0.0f,
		.update = update_callback,
		.complete = complete_callback,
	};
	wl_event_source_timer_update(animation_manager.tick, 1);
}

float get_fastest_output_refresh_ms() {
	float fastest_output_refresh_ms = 16.6667; // fallback to 60 Hz
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		if (output->refresh_nsec > 0) {
			float output_refresh_ms = output->refresh_nsec / 1000000.0;
			fastest_output_refresh_ms = MIN(fastest_output_refresh_ms, output_refresh_ms);
		}
	}
	return fastest_output_refresh_ms;
}

void refresh_animation_manager_timing() {
	if (!config || !root) {
		return;
	}
	animation_manager.tick_time = get_fastest_output_refresh_ms();
	animation_manager.progress_delta = animation_manager.tick_time / config->animation_duration_ms;
}

void animation_manager_init(struct sway_server *server) {
	animation_manager.tick = wl_event_loop_add_timer(server->wl_event_loop,
			animation_timer, &animation_manager.current_animation);
	refresh_animation_manager_timing();
}

float lerp(float a, float b, float t) {
	return a * (1.0 - t) + b * t;
}

float get_animated_value(float from, float to) {
	return lerp(from, to, animation_manager.current_animation.multiplier);
}
