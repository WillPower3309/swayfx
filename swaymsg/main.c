
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <getopt.h>
#include <stdint.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <ctype.h>
#include <unistd.h>
#include <json.h>
#include "stringop.h"
#include "ipc-client.h"
#include "log.h"

static bool success_object(json_object *result) {
	json_object *success;

	if (!json_object_object_get_ex(result, "success", &success)) {
		return true;
	}

	return json_object_get_boolean(success);
}

// Iterate results array and return false if any of them failed
static bool success(json_object *r, bool fallback) {
	if (!json_object_is_type(r, json_type_array)) {
		if (json_object_is_type(r, json_type_object)) {
			return success_object(r);
		}
		return fallback;
	}

	size_t results_len = json_object_array_length(r);
	if (!results_len) {
		return fallback;
	}

	for (size_t i = 0; i < results_len; ++i) {
		json_object *result = json_object_array_get_idx(r, i);

		if (!success_object(result)) {
			return false;
		}
	}

	return true;
}

static void pretty_print_cmd(json_object *r) {
	if (!success_object(r)) {
		json_object *error;
		if (!json_object_object_get_ex(r, "error", &error)) {
			printf("An unknown error occurred");
		} else {
			printf("Error: %s\n", json_object_get_string(error));
		}
	}
}

static void pretty_print_workspace(json_object *w) {
	json_object *name, *rect, *visible, *output, *urgent, *layout,
				*representation, *focused;
	json_object_object_get_ex(w, "name", &name);
	json_object_object_get_ex(w, "rect", &rect);
	json_object_object_get_ex(w, "visible", &visible);
	json_object_object_get_ex(w, "output", &output);
	json_object_object_get_ex(w, "urgent", &urgent);
	json_object_object_get_ex(w, "layout", &layout);
	json_object_object_get_ex(w, "representation", &representation);
	json_object_object_get_ex(w, "focused", &focused);
	printf(
		"Workspace %s%s%s%s\n"
		"  Output: %s\n"
		"  Layout: %s\n"
		"  Representation: %s\n\n",
		json_object_get_string(name),
		json_object_get_boolean(focused) ? " (focused)" : "",
		!json_object_get_boolean(visible) ? " (off-screen)" : "",
		json_object_get_boolean(urgent) ? " (urgent)" : "",
		json_object_get_string(output),
		json_object_get_string(layout),
		json_object_get_string(representation)
	);
}

static const char *pretty_type_name(const char *name) {
	// TODO these constants probably belong in the common lib
	struct {
		const char *a;
		const char *b;
	} type_names[] = {
		{ "keyboard", "Keyboard" },
		{ "pointer", "Pointer" },
		{ "touchpad", "Touchpad" },
		{ "tablet_pad", "Tablet pad" },
		{ "tablet_tool", "Tablet tool" },
		{ "touch", "Touch" },
		{ "switch", "Switch" },
	};

	for (size_t i = 0; i < sizeof(type_names) / sizeof(type_names[0]); ++i) {
		if (strcmp(type_names[i].a, name) == 0) {
			return type_names[i].b;
		}
	}

	return name;
}

static void pretty_print_input(json_object *i) {
	json_object *id, *name, *type, *product, *vendor, *kbdlayout, *libinput;
	json_object_object_get_ex(i, "identifier", &id);
	json_object_object_get_ex(i, "name", &name);
	json_object_object_get_ex(i, "type", &type);
	json_object_object_get_ex(i, "product", &product);
	json_object_object_get_ex(i, "vendor", &vendor);

	const char *fmt =
		"Input device: %s\n"
		"  Type: %s\n"
		"  Identifier: %s\n"
		"  Product ID: %d\n"
		"  Vendor ID: %d\n";


	printf(fmt, json_object_get_string(name),
		pretty_type_name(json_object_get_string(type)),
		json_object_get_string(id),
		json_object_get_int(product),
		json_object_get_int(vendor));

	if (json_object_object_get_ex(i, "xkb_active_layout_name", &kbdlayout)) {
		const char *layout = json_object_get_string(kbdlayout);
		printf("  Active Keyboard Layout: %s\n", layout ? layout : "(unnamed)");
	}

	if (json_object_object_get_ex(i, "libinput", &libinput)) {
		json_object *events;
		if (json_object_object_get_ex(libinput, "send_events", &events)) {
			printf("  Libinput Send Events: %s\n",
					json_object_get_string(events));
		}
	}

	printf("\n");
}

static void pretty_print_seat(json_object *i) {
	json_object *name, *capabilities, *devices;
	json_object_object_get_ex(i, "name", &name);
	json_object_object_get_ex(i, "capabilities", &capabilities);
	json_object_object_get_ex(i, "devices", &devices);

	const char *fmt =
		"Seat: %s\n"
		"  Capabilities: %d\n";

	printf(fmt, json_object_get_string(name),
		json_object_get_int(capabilities));

	size_t devices_len = json_object_array_length(devices);
	if (devices_len > 0) {
		printf("  Devices:\n");
		for (size_t i = 0; i < devices_len; ++i) {
			json_object *device = json_object_array_get_idx(devices, i);

			json_object *device_name;
			json_object_object_get_ex(device, "name", &device_name);

			printf("    %s\n", json_object_get_string(device_name));
		}
	}

	printf("\n");
}

static void pretty_print_output(json_object *o) {
	json_object *name, *rect, *focused, *active, *power, *ws, *current_mode, *non_desktop;
	json_object_object_get_ex(o, "name", &name);
	json_object_object_get_ex(o, "rect", &rect);
	json_object_object_get_ex(o, "focused", &focused);
	json_object_object_get_ex(o, "active", &active);
	json_object_object_get_ex(o, "power", &power);
	json_object_object_get_ex(o, "current_workspace", &ws);
	json_object_object_get_ex(o, "non_desktop", &non_desktop);
	json_object *make, *model, *serial, *scale, *scale_filter, *subpixel,
		*transform, *max_render_time, *adaptive_sync_status, *allow_tearing;
	json_object_object_get_ex(o, "make", &make);
	json_object_object_get_ex(o, "model", &model);
	json_object_object_get_ex(o, "serial", &serial);
	json_object_object_get_ex(o, "scale", &scale);
	json_object_object_get_ex(o, "scale_filter", &scale_filter);
	json_object_object_get_ex(o, "subpixel_hinting", &subpixel);
	json_object_object_get_ex(o, "transform", &transform);
	json_object_object_get_ex(o, "max_render_time", &max_render_time);
	json_object_object_get_ex(o, "adaptive_sync_status", &adaptive_sync_status);
	json_object_object_get_ex(o, "allow_tearing", &allow_tearing);
	json_object *x, *y;
	json_object_object_get_ex(rect, "x", &x);
	json_object_object_get_ex(rect, "y", &y);
	json_object *modes;
	json_object_object_get_ex(o, "modes", &modes);
	json_object *width, *height, *refresh;
	json_object_object_get_ex(o, "current_mode", &current_mode);
	json_object_object_get_ex(current_mode, "width", &width);
	json_object_object_get_ex(current_mode, "height", &height);
	json_object_object_get_ex(current_mode, "refresh", &refresh);

	if (json_object_get_boolean(non_desktop)) {
		printf(
			"Output %s '%s %s %s' (non-desktop)\n",
			json_object_get_string(name),
			json_object_get_string(make),
			json_object_get_string(model),
			json_object_get_string(serial)
		);
	} else if (json_object_get_boolean(active)) {
		printf(
			"Output %s '%s %s %s'%s\n"
			"  Current mode: %dx%d @ %.3f Hz\n"
			"  Power: %s\n"
			"  Position: %d,%d\n"
			"  Scale factor: %f\n"
			"  Scale filter: %s\n"
			"  Subpixel hinting: %s\n"
			"  Transform: %s\n"
			"  Workspace: %s\n",
			json_object_get_string(name),
			json_object_get_string(make),
			json_object_get_string(model),
			json_object_get_string(serial),
			json_object_get_boolean(focused) ? " (focused)" : "",
			json_object_get_int(width),
			json_object_get_int(height),
			(double)json_object_get_int(refresh) / 1000,
			json_object_get_boolean(power) ? "on" : "off",
			json_object_get_int(x), json_object_get_int(y),
			json_object_get_double(scale),
			json_object_get_string(scale_filter),
			json_object_get_string(subpixel),
			json_object_get_string(transform),
			json_object_get_string(ws)
		);

		int max_render_time_int = json_object_get_int(max_render_time);
		printf("  Max render time: ");
		printf(max_render_time_int == 0 ? "off\n" : "%d ms\n", max_render_time_int);

		printf("  Adaptive sync: %s\n",
			json_object_get_string(adaptive_sync_status));

		printf("  Allow tearing: %s\n",
			json_object_get_boolean(allow_tearing) ? "yes" : "no");
	} else {
		printf(
			"Output %s '%s %s %s' (disabled)\n",
			json_object_get_string(name),
			json_object_get_string(make),
			json_object_get_string(model),
			json_object_get_string(serial)
		);
	}

	size_t modes_len = json_object_is_type(modes, json_type_array)
		? json_object_array_length(modes) : 0;
	if (modes_len > 0) {
		printf("  Available modes:\n");
		for (size_t i = 0; i < modes_len; ++i) {
			json_object *mode = json_object_array_get_idx(modes, i);

			json_object *mode_width, *mode_height, *mode_refresh,
				*mode_picture_aspect_ratio;
			json_object_object_get_ex(mode, "width", &mode_width);
			json_object_object_get_ex(mode, "height", &mode_height);
			json_object_object_get_ex(mode, "refresh", &mode_refresh);
			json_object_object_get_ex(mode, "picture_aspect_ratio",
				&mode_picture_aspect_ratio);

			printf("    %dx%d @ %.3f Hz", json_object_get_int(mode_width),
				json_object_get_int(mode_height),
				(double)json_object_get_int(mode_refresh) / 1000);
			if (mode_picture_aspect_ratio &&
					strcmp("none", json_object_get_string(mode_picture_aspect_ratio)) != 0) {
				printf(" (%s)", json_object_get_string(mode_picture_aspect_ratio));
			}
			printf("\n");
		}
	}

	printf("\n");
}

static void pretty_print_version(json_object *v) {
	json_object *ver;
	json_object_object_get_ex(v, "human_readable", &ver);
	printf("sway version %s\n", json_object_get_string(ver));
}

static void pretty_print_config(json_object *c) {
	json_object *config;
	json_object_object_get_ex(c, "config", &config);
	printf("%s\n", json_object_get_string(config));
}

static void pretty_print_tree(json_object *obj, int indent) {
	for (int i = 0; i < indent; i++) {
		printf("  ");
	}

	int id = json_object_get_int(json_object_object_get(obj, "id"));
	const char *name = json_object_get_string(json_object_object_get(obj, "name"));
	const char *type = json_object_get_string(json_object_object_get(obj, "type"));
	const char *shell = json_object_get_string(json_object_object_get(obj, "shell"));

	printf("#%d: %s \"%s\"", id, type, name);

	if (shell != NULL) {
		int pid = json_object_get_int(json_object_object_get(obj, "pid"));
		const char *app_id = json_object_get_string(json_object_object_get(obj, "app_id"));
		json_object *window_props_obj = json_object_object_get(obj, "window_properties");
		const char *instance = json_object_get_string(json_object_object_get(window_props_obj, "instance"));
		const char *class = json_object_get_string(json_object_object_get(window_props_obj, "class"));
		int x11_id = json_object_get_int(json_object_object_get(obj, "window"));
		const char *foreign_toplevel_id = json_object_get_string(json_object_object_get(obj, "foreign_toplevel_identifier"));
		const char *sandbox_engine = json_object_get_string(json_object_object_get(obj, "sandbox_engine"));
		const char *sandbox_app_id = json_object_get_string(json_object_object_get(obj, "sandbox_app_id"));
		const char *sandbox_instance_id = json_object_get_string(json_object_object_get(obj, "sandbox_instance_id"));

		printf(" (%s, pid: %d", shell, pid);
		if (app_id != NULL) {
			printf(", app_id: \"%s\"", app_id);
		}
		if (instance != NULL) {
			printf(", instance: \"%s\"", instance);
		}
		if (class != NULL) {
			printf(", class: \"%s\"", class);
		}
		if (x11_id != 0) {
			printf(", X11 window: 0x%X", x11_id);
		}
		if (foreign_toplevel_id != NULL) {
			printf(", foreign_toplevel_id: \"%s\"", foreign_toplevel_id);
		}
		if (sandbox_engine != NULL) {
			printf(", sandbox_engine: \"%s\"", sandbox_engine);
		}
		if (sandbox_app_id != NULL) {
			printf(", sandbox_app_id: \"%s\"", sandbox_app_id);
		}
		if (sandbox_instance_id != NULL) {
			printf(", sandbox_instance_id: \"%s\"", sandbox_instance_id);
		}
		printf(")");
	}

	printf("\n");

	json_object *nodes_obj = json_object_object_get(obj, "nodes");
	size_t len = json_object_array_length(nodes_obj);
	for (size_t i = 0; i < len; i++) {
		pretty_print_tree(json_object_array_get_idx(nodes_obj, i), indent + 1);
	}

	json_object *floating_nodes_obj;
	json_bool floating_nodes = json_object_object_get_ex(obj, "floating_nodes", &floating_nodes_obj);
	if (floating_nodes) {
		size_t len = json_object_array_length(floating_nodes_obj);
		for (size_t i = 0; i < len; i++) {
			pretty_print_tree(json_object_array_get_idx(floating_nodes_obj, i), indent + 1);
		}
	}
}

static void pretty_print(int type, json_object *resp) {
	switch (type) {
	case IPC_SEND_TICK:
		return;
	case IPC_GET_VERSION:
		pretty_print_version(resp);
		return;
	case IPC_GET_CONFIG:
		pretty_print_config(resp);
		return;
	case IPC_GET_TREE:
		pretty_print_tree(resp, 0);
		return;
	case IPC_COMMAND:
	case IPC_GET_WORKSPACES:
	case IPC_GET_INPUTS:
	case IPC_GET_OUTPUTS:
	case IPC_GET_SEATS:
		break;
	default:
		printf("%s\n", json_object_to_json_string_ext(resp,
			JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
		return;
	}

	json_object *obj;
	size_t len = json_object_array_length(resp);
	for (size_t i = 0; i < len; ++i) {
		obj = json_object_array_get_idx(resp, i);
		switch (type) {
		case IPC_COMMAND:
			pretty_print_cmd(obj);
			break;
		case IPC_GET_WORKSPACES:
			pretty_print_workspace(obj);
			break;
		case IPC_GET_INPUTS:
			pretty_print_input(obj);
			break;
		case IPC_GET_OUTPUTS:
			pretty_print_output(obj);
			break;
		case IPC_GET_SEATS:
			pretty_print_seat(obj);
			break;
		}
	}
}

int main(int argc, char **argv) {
	static bool quiet = false;
	static bool raw = false;
	static bool monitor = false;
	char *socket_path = NULL;
	char *cmdtype = NULL;

	sway_log_init(SWAY_INFO, NULL);

	static const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"monitor", no_argument, NULL, 'm'},
		{"pretty", no_argument, NULL, 'p'},
		{"quiet", no_argument, NULL, 'q'},
		{"raw", no_argument, NULL, 'r'},
		{"socket", required_argument, NULL, 's'},
		{"type", required_argument, NULL, 't'},
		{"version", no_argument, NULL, 'v'},
		{0, 0, 0, 0}
	};

	const char *usage =
		"Usage: swaymsg [options] [message]\n"
		"\n"
		"  -h, --help             Show help message and quit.\n"
		"  -m, --monitor          Monitor until killed (-t SUBSCRIBE only)\n"
		"  -p, --pretty           Use pretty output even when not using a tty\n"
		"  -q, --quiet            Be quiet.\n"
		"  -r, --raw              Use raw output even if using a tty\n"
		"  -s, --socket <socket>  Use the specified socket.\n"
		"  -t, --type <type>      Specify the message type.\n"
		"  -v, --version          Show the version number and quit.\n";

	raw = !isatty(STDOUT_FILENO);

	int c;
	while (1) {
		int option_index = 0;
		c = getopt_long(argc, argv, "hmpqrs:t:v", long_options, &option_index);
		if (c == -1) {
			break;
		}
		switch (c) {
		case 'm': // Monitor
			monitor = true;
			break;
		case 'p': // Pretty
			raw = false;
			break;
		case 'q': // Quiet
			quiet = true;
			break;
		case 'r': // Raw
			raw = true;
			break;
		case 's': // Socket
			socket_path = strdup(optarg);
			break;
		case 't': // Type
			cmdtype = strdup(optarg);
			break;
		case 'v':
			printf("swaymsg version " SWAY_VERSION " (based on sway " SWAY_ORIGINAL_VERSION ")\n");
			exit(EXIT_SUCCESS);
			break;
		default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
		}
	}

	if (!cmdtype) {
		cmdtype = strdup("command");
	}
	if (!socket_path) {
		socket_path = get_socketpath();
		if (!socket_path) {
			if (quiet) {
				exit(EXIT_FAILURE);
			}
			sway_abort("Unable to retrieve socket path");
		}
	}

	uint32_t type = IPC_COMMAND;

	if (strcasecmp(cmdtype, "command") == 0) {
		type = IPC_COMMAND;
	} else if (strcasecmp(cmdtype, "get_workspaces") == 0) {
		type = IPC_GET_WORKSPACES;
	} else if (strcasecmp(cmdtype, "get_seats") == 0) {
		type = IPC_GET_SEATS;
	} else if (strcasecmp(cmdtype, "get_inputs") == 0) {
		type = IPC_GET_INPUTS;
	} else if (strcasecmp(cmdtype, "get_outputs") == 0) {
		type = IPC_GET_OUTPUTS;
	} else if (strcasecmp(cmdtype, "get_tree") == 0) {
		type = IPC_GET_TREE;
	} else if (strcasecmp(cmdtype, "get_marks") == 0) {
		type = IPC_GET_MARKS;
	} else if (strcasecmp(cmdtype, "get_bar_config") == 0) {
		type = IPC_GET_BAR_CONFIG;
	} else if (strcasecmp(cmdtype, "get_version") == 0) {
		type = IPC_GET_VERSION;
	} else if (strcasecmp(cmdtype, "get_binding_modes") == 0) {
		type = IPC_GET_BINDING_MODES;
	} else if (strcasecmp(cmdtype, "get_binding_state") == 0) {
		type = IPC_GET_BINDING_STATE;
	} else if (strcasecmp(cmdtype, "get_config") == 0) {
		type = IPC_GET_CONFIG;
	} else if (strcasecmp(cmdtype, "send_tick") == 0) {
		type = IPC_SEND_TICK;
	} else if (strcasecmp(cmdtype, "subscribe") == 0) {
		type = IPC_SUBSCRIBE;
	} else {
		if (quiet) {
			exit(EXIT_FAILURE);
		}
		sway_abort("Unknown message type %s", cmdtype);
	}

	free(cmdtype);

	if (monitor && type != IPC_SUBSCRIBE) {
		if (!quiet) {
			sway_log(SWAY_ERROR, "Monitor can only be used with -t SUBSCRIBE");
		}
		free(socket_path);
		return 1;
	}

	char *command = NULL;
	if (optind < argc) {
		command = join_args(argv + optind, argc - optind);
	} else {
		command = strdup("");
	}

	int ret = 0;
	int socketfd = ipc_open_socket(socket_path);
	struct timeval timeout = {.tv_sec = 3, .tv_usec = 0};
	ipc_set_recv_timeout(socketfd, timeout);
	uint32_t len = strlen(command);
	char *resp = ipc_single_command(socketfd, type, command, &len);

	// pretty print the json
	json_tokener *tok = json_tokener_new_ex(JSON_MAX_DEPTH);
	if (tok == NULL) {
		if (quiet) {
			exit(EXIT_FAILURE);
		}
		sway_abort("failed allocating json_tokener");
	}
	json_object *obj = json_tokener_parse_ex(tok, resp, -1);
	enum json_tokener_error err = json_tokener_get_error(tok);
	json_tokener_free(tok);
	if (obj == NULL || err != json_tokener_success) {
		if (!quiet) {
			sway_log(SWAY_ERROR, "failed to parse payload as json: %s",
				json_tokener_error_desc(err));
		}
		ret = 1;
	} else {
		if (!success(obj, true)) {
			ret = 2;
		}
		if (!quiet && (type != IPC_SUBSCRIBE  || ret != 0)) {
			if (raw) {
				printf("%s\n", json_object_to_json_string_ext(obj,
					JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
			} else {
				pretty_print(type, obj);
			}
		}
		json_object_put(obj);
	}
	free(command);
	free(resp);

	if (type == IPC_SUBSCRIBE && ret == 0) {
		// Remove the timeout for subscribed events
		timeout.tv_sec = 0;
		timeout.tv_usec = 0;
		ipc_set_recv_timeout(socketfd, timeout);

		do {
			struct ipc_response *reply = ipc_recv_response(socketfd);
			if (!reply) {
				break;
			}

			json_tokener *tok = json_tokener_new_ex(JSON_MAX_DEPTH);
			if (tok == NULL) {
				if (quiet) {
					exit(EXIT_FAILURE);
				}
				sway_abort("failed allocating json_tokener");
			}
			json_object *obj = json_tokener_parse_ex(tok, reply->payload, -1);
			enum json_tokener_error err = json_tokener_get_error(tok);
			json_tokener_free(tok);
			if (obj == NULL || err != json_tokener_success) {
				if (!quiet) {
					sway_log(SWAY_ERROR, "failed to parse payload as json: %s",
						json_tokener_error_desc(err));
				}
				ret = 1;
				break;
			} else if (quiet) {
				json_object_put(obj);
			} else {
				if (raw) {
					printf("%s\n", json_object_to_json_string(obj));
				} else {
					printf("%s\n", json_object_to_json_string_ext(obj,
						JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED));
				}
				fflush(stdout);
				json_object_put(obj);
			}

			free_ipc_response(reply);
		} while (monitor);
	}

	close(socketfd);
	free(socket_path);
	return ret;
}
