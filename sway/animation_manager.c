#include <assert.h>
#include "sway/animation_manager.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/root.h"

struct animation_manager {
	float tick_time;
	struct wl_event_source *tick;
	struct wl_list animations;
} animation_manager;

struct animation init_animation(void *data) {
	return (struct animation){
		.data = data,
		.progress = 0.0f,
		.progress_delta = 0.0f,
		.multiplier = 0.0f,
		.initialized = false,
		.type = ANIM_WINDOW_OPEN,
		.update = NULL,
		.complete = NULL,
	};
}

static float ease_out_cubic(float p) {
	return pow(p - 1, 3) + 1;
}

static int animation_timer() {
	struct animation *animation, *tmp;
	wl_list_for_each_reverse_safe(animation, tmp, &animation_manager.animations, link) {
		animation->progress = MIN(animation->progress + animation->progress_delta, 1.0f);
		animation->multiplier = ease_out_cubic(animation->progress);

		if (animation->update) {
			animation->update(animation->data);
		}

		if (animation->progress == 1.0f) {
			finish_animation(animation);
			if (animation->complete) {
				animation->complete(animation->data);
			}
		}
	}

	if (!wl_list_empty(&animation_manager.animations)) {
		wl_event_source_timer_update(animation_manager.tick,
				animation_manager.tick_time);
	}
	return 0;
}

void add_animation(struct animation *animation, enum sway_animation_type type,
		void (*update_callback)(void *), void (*complete_callback)(void *)) {
	float duration_ms = animation_manager_duration_ms(type);
	if (duration_ms <= 0) {
		if (complete_callback) {
			complete_callback(animation->data);
		}
		return;
	}
	// remove previous instances of this animation
	if (animation->initialized) {
		wl_list_remove(&animation->link);
	}

	animation->progress = 0.0f;
	animation->progress_delta = animation_manager.tick_time / duration_ms;
	animation->multiplier = 0.0f;
	animation->initialized = true;
	animation->type = type;
	animation->update = update_callback;
	animation->complete = complete_callback;
	wl_list_insert(&animation_manager.animations, &animation->link);
}

void finish_animation(struct animation *animation) {
	if (animation->initialized) {
		wl_list_remove(&animation->link);
		animation->initialized = false;
	}
	animation->progress = 1.0f;
	animation->multiplier = 1.0f;
}

void start_animations() {
	if (wl_list_empty(&animation_manager.animations)) {
		return;
	}
	assert(animation_manager.tick);

	wl_event_source_timer_update(animation_manager.tick, 1);
}

static float get_fastest_output_refresh_ms() {
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

	// rescale in-flight animations to their type's current duration
	struct animation *animation, *tmp;
	wl_list_for_each_safe(animation, tmp, &animation_manager.animations, link) {
		float duration_ms = animation_manager_duration_ms(animation->type);
		if (duration_ms <= 0) {
			finish_animation(animation);
			if (animation->complete) {
				animation->complete(animation->data);
			}
			continue;
		}
		animation->progress_delta = animation_manager.tick_time / duration_ms;
	}
}

void animation_manager_init(struct sway_server *server) {
	animation_manager.tick = wl_event_loop_add_timer(server->wl_event_loop,
			animation_timer, NULL);
	wl_list_init(&animation_manager.animations);
	refresh_animation_manager_timing();
}

static float lerp(float a, float b, float t) {
	return a * (1.0 - t) + b * t;
}

float get_animated_value(float from, float to, const struct animation *animation) {
	if (!animation->initialized) {
		return to;
	}
	return lerp(from, to, animation->multiplier);
}

// falls back to the shared default when this type has no override
float animation_manager_duration_ms(enum sway_animation_type type) {
	if (config->animation_duration_ms_set[type]) {
		return config->animation_duration_ms_by_type[type];
	}
	return config->animation_duration_ms;
}

