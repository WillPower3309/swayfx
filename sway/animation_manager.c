#include <stdio.h>
#include <stdlib.h>
#include <wayland-util.h>
#include "log.h"
#include "sway/animation_manager.h"
#include "sway/config.h"
#include "sway/desktop/transaction.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/root.h"

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

float lerp(float a, float b, float t) {
	return a * (1.0 - t) + b * t;
}

float ease_out_cubic(float t) {
	float p = t - 1;
	return pow(p, 3) + 1;
}

void finish_animation(struct container_animation_state *animation_state, struct animation_manager *animation_manager) {
	// TODO: can I check if the link is in the list?
	if (animation_state->init) {
		wl_list_remove(&animation_state->link);
		animation_state->init = false;
	}
	animation_state->progress = 1.0f;

	if (animation_state->complete) {
		animation_state->complete(animation_state->container);
	}
}

int animation_timer(void *data) {
	struct animation_manager *animation_manager = data;
	float tick_time = get_fastest_output_refresh_ms();

	struct container_animation_state *animation_state, *tmp;
	wl_list_for_each_reverse_safe(animation_state, tmp, &animation_manager->animation_states, link) {
		float progress_delta = tick_time / config->animation_duration_ms;
		animation_state->progress = MIN(animation_state->progress + progress_delta, 1.0f);

		// TODO: memory leak, use wl_list?
		for(int i = 0; i < animation_state->animated_vars->length; i++) {
			struct animated_var *animated_var = animation_state->animated_vars->items[i];
			*animated_var->current = lerp(animated_var->from, animated_var->to, ease_out_cubic(animation_state->progress));
		}

		if (animation_state->update) {
			animation_state->update(animation_state->container);
		}

		if (animation_state->progress == 1.0f) {
			finish_animation(animation_state, animation_manager);
			continue;
		}
	}

	if (!wl_list_empty(&animation_manager->animation_states)) {
		wl_event_source_timer_update(animation_manager->tick, tick_time);
	}
	return 0;
}

// TODO: is this needed? just calls finish_animation
void cancel_container_animation(struct container_animation_state *animation_state, struct animation_manager *animation_manager) {
	finish_animation(animation_state, animation_manager);
}

void add_container_animation(struct container_animation_state *animation_state, struct animation_manager *animation_manager) {
	animation_state->init = true;
	wl_list_insert(&animation_manager->animation_states, &animation_state->link);
	wl_event_source_timer_update(animation_manager->tick, 1);
}


void fadeout_animation_complete(struct sway_container *con) {
	view_cleanup(con->view);
	transaction_commit_dirty();
}

// TODO: free memory or use wl_list
struct container_animation_state container_animation_state_create_fadein(struct sway_container *con) {
	struct container_animation_state animation_state;
	animation_state.init = false;
	animation_state.progress = 0.0f;
	animation_state.container = con;
	animation_state.update = container_update;
	animation_state.complete = NULL;

	animation_state.animated_vars = create_list();

	struct animated_var *alpha_animated_var = calloc(1, sizeof(*alpha_animated_var));
	alpha_animated_var->from = con->alpha;
	alpha_animated_var->to = con->target_alpha;
	alpha_animated_var->current = &con->alpha;
	list_add(animation_state.animated_vars, alpha_animated_var);

	return animation_state;
}

// TODO: free memory or use wl_list
struct container_animation_state container_animation_state_create_fadeout(struct sway_container *con) {
	struct container_animation_state animation_state;
	animation_state.init = false;
	animation_state.progress = 0.0f;
	animation_state.container = con;
	animation_state.update = container_update;
	animation_state.complete = fadeout_animation_complete;

	animation_state.animated_vars = create_list();

	struct animated_var *alpha_animated_var = calloc(1, sizeof(*alpha_animated_var));
	alpha_animated_var->from = con->alpha;
	alpha_animated_var->to = 0.0f;
	alpha_animated_var->current = &con->alpha;
	list_add(animation_state.animated_vars, alpha_animated_var);

	return animation_state;
}

struct animation_manager *animation_manager_create(struct sway_server *server) {
	struct animation_manager *animation_manager = calloc(1, sizeof(*animation_manager));
	if (!sway_assert(animation_manager, "Failed to allocate animation_manager")) {
		return NULL;
	}

	wl_list_init(&animation_manager->animation_states);
	animation_manager->tick = wl_event_loop_add_timer(server->wl_event_loop, animation_timer, animation_manager);
	wl_event_source_timer_update(animation_manager->tick, 1);

	return animation_manager;
}

void animation_manager_destroy(struct animation_manager *animation_manager) {
	// TODO
	return;
}

