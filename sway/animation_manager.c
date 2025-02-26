#include <stdio.h>
#include <stdlib.h>
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

double lerp(double a, double b, double t) {
	return a * (1.0 - t) + b * t;
}

double ease_out_cubic(double t) {
	double p = t - 1;
	return pow(p, 3) + 1;
}

int animation_timer(void *data) {
	struct animation_manager *animation_manager = data;

	// remove completed animations
	for (int i = 0; i < animation_manager->animated_states->length; i++) {
		struct container_animation_state *animation_state = animation_manager->animated_states->items[i];
		if (animation_state->progress == 1.0f) {
			animation_state->is_being_animated = false; // TODO: ensure this properly catches the race con (view unmapped as animation completes)
			list_del(animation_manager->animated_states, i);

			// TODO: do this better, anim complete callbacks?
			if (animation_state->to_alpha == 0.0) {
				view_cleanup(animation_state->container->view);
				transaction_commit_dirty();
			}

			continue;
		}
	}

	for (int i = 0; i < animation_manager->animated_states->length; i++) {
		struct container_animation_state *animation_state = animation_manager->animated_states->items[i];

		struct sway_container *con = animation_state->container;

		printf("from: %f, to: %f\n", animation_state->from_alpha, animation_state->to_alpha);
		printf("con alpha: %f\n", con->alpha);

		float progress_delta = get_fastest_output_refresh_ms() / config->animation_duration_ms;
		animation_state->progress = MIN(animation_state->progress + progress_delta, 1.0f);
		con->alpha = lerp(animation_state->from_alpha, animation_state->to_alpha, ease_out_cubic(animation_state->progress));
		container_update(con);
	}

	if (animation_manager->animated_states->length > 0) {
		// TODO: save this val to the animation manager
		wl_event_source_timer_update(animation_manager->tick, get_fastest_output_refresh_ms());
	}
	return 0;
}

void add_container_animation(struct container_animation_state *animation_state, struct animation_manager *animation_manager) {
	list_add(animation_manager->animated_states, animation_state);
	animation_state->is_being_animated = true;
	wl_event_source_timer_update(animation_manager->tick, 1);
}

struct animation_manager *animation_manager_create(struct sway_server *server) {
	struct animation_manager *animation_manager = calloc(1, sizeof(*animation_manager));
	if (!sway_assert(animation_manager, "Failed to allocate animation_manager")) {
		return NULL;
	}

	animation_manager->animated_states = create_list();
	animation_manager->tick = wl_event_loop_add_timer(server->wl_event_loop, animation_timer, animation_manager);
	wl_event_source_timer_update(animation_manager->tick, 1);

	return animation_manager;
}

void animation_manager_destroy(struct animation_manager *animation_manager) {
	// TODO
	return;
}

