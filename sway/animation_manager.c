#include <stdio.h>
#include <stdlib.h>
#include "log.h"
#include "sway/animation_manager.h"
#include "sway/config.h"
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

	for (int i = 0; i < animation_manager->animated_views->length; i++) {
		struct sway_view *view = animation_manager->animated_views->items[i];
		if (view->animation_progress == 1.0f) {
			list_del(animation_manager->animated_views, i);
			continue;
		}

		struct sway_container *con = view->container;
		float progress_delta = get_fastest_output_refresh_ms() / config->animation_duration_ms;
		view->animation_progress = MIN(view->animation_progress + progress_delta, 1.0f);
		con->alpha = lerp(0, con->target_alpha, ease_out_cubic(view->animation_progress));
		container_update(con);

		printf("con alpha: %f\n", con->alpha);
	}

	if (animation_manager->animated_views->length > 0) {
		// TODO: save this val to the animation manager
		wl_event_source_timer_update(animation_manager->tick, get_fastest_output_refresh_ms());
	}
	return 0;
}

void add_view_animation(struct sway_view *view, struct animation_manager *animation_manager) {
	list_add(animation_manager->animated_views, view);
	wl_event_source_timer_update(animation_manager->tick, 1);
}

struct animation_manager *animation_manager_create(struct sway_server *server) {
	struct animation_manager *animation_manager = calloc(1, sizeof(*animation_manager));
	if (!sway_assert(animation_manager, "Failed to allocate animation_manager")) {
		return NULL;
	}

	animation_manager->animated_views = create_list();
	animation_manager->tick = wl_event_loop_add_timer(server->wl_event_loop, animation_timer, animation_manager);
	wl_event_source_timer_update(animation_manager->tick, 1);

	return animation_manager;
}

