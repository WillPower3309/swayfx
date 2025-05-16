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
#include "sway/tree/node.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"

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

void finish_animation(struct container_animation_state *animation_state) {
	// TODO: can I check if the link is in the list and remove the init var?
	if (animation_state->init) {
		animation_state->init = false;
		wl_list_remove(&animation_state->link);
		printf("animation complete\n");
	}
	animation_state->progress = 1.0f;

	if (animation_state->complete) {
		animation_state->complete(animation_state);
	}

	node_set_dirty(&animation_state->container->node);
	transaction_commit_dirty();
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

void start_animation(struct container_animation_state *animation_state,
		struct animation_manager *animation_manager) {
	wl_list_insert(&animation_manager->animation_states, &animation_state->link);
	animation_state->init = true;
	wl_event_source_timer_update(animation_manager->tick, 1);
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

