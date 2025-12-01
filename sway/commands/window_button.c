#include "log.h"
#include "util.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"

static struct window_button *find_button(list_t *buttons, char *id, int *index);

struct cmd_results* cmd_window_button(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "window_button", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	int button_index = 0;
	bool visual_changes = false;

	list_t *target = config->window_buttons;

	int arg = 0;
	if (strcmp("default", argv[arg]) == 0) {
		arg++;
	} else {
		struct sway_container *container = config->handler_context.container;
		if (!container || !container->view) {
			return cmd_results_new(CMD_INVALID, "Only views can have borders");
		}

		target = container->buttons;
	}

	struct window_button *selected = find_button(target, argv[arg+1], &button_index);
	if (strcmp("remove", argv[arg]) == 0) {
		if (selected == NULL) {
			return cmd_results_new(CMD_SUCCESS, NULL);
		}

		list_del(target, button_index);
		free(selected->id);
		free(selected->command);
		free(selected);
		visual_changes = true;
		goto cleanup;
	}

	if (strcmp("set", argv[arg]) != 0) {
		return cmd_results_new(CMD_INVALID, "Invalid window_button command: expecting remove or set");
	}

	arg += 1;

	if (selected == NULL) {
		selected = calloc(1, sizeof(*selected));
		if (!selected) {
			return cmd_results_new(CMD_FAILURE, "window_button command failed: out of memory");
		}
		selected->id = malloc(strlen(argv[arg])+1);
		if (!selected->id) {
			return cmd_results_new(CMD_FAILURE, "window_button command failed: out of memory");
		}
		strcpy(selected->id, argv[arg]);
		selected->command = NULL;
		list_add(target, selected);
		visual_changes = true;
	}

	arg+=1;

	while ((arg + 1) < argc) {
		bool is_after = false;
		char *action = argv[arg];
		char *value = argv[arg+1];
		if (strcmp("cmd", action) == 0 || strcmp("command", action) == 0) {
			if (selected->command != NULL && strcmp(selected->command, value) == 0) {
				goto next;
			}

			if (selected->command != NULL) {
				free(selected->command);
			}

			selected->command = malloc(strlen(value)+1);
			strcpy(selected->command, value);
		} else if (strcmp("color", action) == 0) {
			uint32_t rgb;
			if (parse_color(value, &rgb)) {
				color_to_rgba(selected->color, rgb);
			}
			visual_changes = true;
		} else if (strcmp("before", action) == 0 || ((is_after = (strcmp("after", action) == 0)))) {
			int other;
			find_button(target, value, &other);
			if (other == -1 || other == button_index) {
				goto next;
			}

			list_del(target, button_index);

			if (button_index < other) {
				other -= 1;
			}

			if (is_after) {
				other += 1;
			}

			list_insert(target, other, selected);
			visual_changes = true;
		}

next:
		arg += 2;
	}
cleanup:
	if (visual_changes && config->handler_context.container) {
		container_update_buttons(config->handler_context.container);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

static struct window_button *find_button(list_t *buttons, char *id, int *index) {
	for (int i = 0; i < buttons->length; i++) {
		struct window_button *item = buttons->items[i];
		if (strcmp(id, item->id) == 0) {
			*index = i;
			return item;
		}
	}

	*index = -1;
	return NULL;
}
