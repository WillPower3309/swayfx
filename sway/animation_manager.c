#include <stdio.h>
#include <stdlib.h>
#include <wayland-util.h>
#include "log.h"
#include "sway/animation_manager.h"
#include "sway/config.h"
#include "sway/desktop/transaction.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"

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

void finish_animation(struct container_animation_state *animation_state) {
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

		if (animation_state->update) {
			animation_state->update(animation_state);
		}

		if (animation_state->progress == 1.0f) {
			finish_animation(animation_state);
		}
	}

	if (!wl_list_empty(&animation_manager->animation_states)) {
		wl_event_source_timer_update(animation_manager->tick, tick_time);
	}
	return 0;
}

// TODO: is this needed? just calls finish_animation
void cancel_container_animation(struct container_animation_state *animation_state) {
	finish_animation(animation_state);
}

void add_container_animation(struct container_animation_state *animation_state, struct animation_manager *animation_manager) {
	animation_state->init = true;
	wl_list_insert(&animation_manager->animation_states, &animation_state->link);
	wl_event_source_timer_update(animation_manager->tick, 1);
}

void fadeinout_animation_update(struct container_animation_state *animation_state) {
	struct sway_container *con = animation_state->container;
	con->alpha = lerp(animation_state->from_alpha, animation_state->to_alpha, ease_out_cubic(animation_state->progress));
	con->blur_alpha = lerp(animation_state->from_blur_alpha, animation_state->to_blur_alpha, ease_out_cubic(animation_state->progress));

	/*
	con->pending.x = animation_state->from_x + (animation_state->to_x - animation_state->from_x) * animation_state->progress;
	con->pending.y = animation_state->from_y + (animation_state->to_y - animation_state->from_y) * animation_state->progress;
	con->pending.width = animation_state->from_width + (animation_state->to_width - animation_state->from_width) * animation_state->progress;
	con->pending.height = animation_state->from_height + (animation_state->to_height - animation_state->from_height) * animation_state->progress;
	*/

	container_update(con);
}

void fadeout_animation_complete(struct sway_container *con) {
	con->node.destroying = true;
	node_set_dirty(&con->node);

	struct sway_container *parent = con->pending.parent;
	struct sway_workspace *ws = con->pending.workspace;

	if (parent) {
		container_detach(con);
		container_reap_empty(parent);
	} else if (ws) {
		container_detach(con);
		workspace_consider_destroy(ws);
	}

	if (root->fullscreen_global) {
		// Container may have been a child of the root fullscreen container
		arrange_root();
	} else if (ws && !ws->node.destroying) {
		arrange_workspace(ws);
		workspace_detect_urgent(ws);
	}

	transaction_commit_dirty();
}

struct container_animation_state container_animation_state_create_fadein(struct sway_container *con) {
	struct sway_container_state pending_state = con->pending;
	return (struct container_animation_state) {
		.init = false,
		.progress = 0.0f,
		.container = con,
		.from_alpha = con->alpha,
		.to_alpha = con->target_alpha,
		.from_blur_alpha = con->blur_alpha,
		.to_blur_alpha = 1.0f,
		.from_x = pending_state.x + (pending_state.width / 2.0f) / 2.0f,
		.to_x = pending_state.x,
		.from_y = pending_state.y + (pending_state.height / 2.0f) / 2.0f,
		.to_y = pending_state.y,
		.from_width = pending_state.width / 2.0f,
		.to_width = pending_state.width,
		.from_height = pending_state.height / 2.0f,
		.to_height = pending_state.height,
		.update = fadeinout_animation_update,
		.complete = NULL,
	};
}

struct container_animation_state container_animation_state_create_fadeout(struct sway_container *con) {
	struct sway_container_state pending_state = con->pending;
	return (struct container_animation_state) {
		.init = false,
		.progress = 0.0f,
		.container = con,
		.from_alpha = con->alpha,
		.to_alpha = 0.0f,
		.from_blur_alpha = con->blur_alpha,
		.to_blur_alpha = 0.0f,
		.from_x = pending_state.x,
		.to_x = pending_state.x + (pending_state.width / 2.0f) / 2.0f,
		.from_y = pending_state.y,
		.to_y = pending_state.y + (pending_state.height / 2.0f) / 2.0f,
		.from_width = pending_state.width,
		.to_width = pending_state.width / 2.0f,
		.from_height = pending_state.height,
		.to_height = pending_state.height / 2.0f,
		.update = fadeinout_animation_update,
		.complete = fadeout_animation_complete,
	};
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

