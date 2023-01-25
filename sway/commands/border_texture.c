#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <drm_fourcc.h>
#include "cairo.h"
#include "stringop.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/border_textures.h"
#include "log.h"

const char *bevel_txtr = "contrib/borders/bevel.png";

struct cmd_results *get_texture_path(char **path, int argc, char **argv) {
	struct cmd_results *error = NULL;

	// default
	if (argc == 0) {
		*path = strdup(bevel_txtr);
		return NULL;
	}
	*path = strdup(argv[0]);

	// shorthands
	if (strcmp(*path, "bevel") == 0) {
		*path = strdup(bevel_txtr);
		return NULL;
	}

	// validation
	if (!expand_path(path)) {
		error = cmd_results_new(CMD_INVALID, "Invalid syntax (%s)", *path);
		free(path);
		return error;
	}
	if (!path) {
		sway_log(SWAY_ERROR, "Failed to allocate expanded path");
		return cmd_results_new(CMD_FAILURE, "Unable to allocate resource");
	}

	bool can_access = access(*path, F_OK | R_OK) != -1;
	if (!can_access) {
		sway_log(SWAY_ERROR, "Unable to access border textures file '%s'", *path);
		error = cmd_results_new(CMD_FAILURE, "Unable to access border textures file '%s'", *path);
		free(path);
		return error;

	}

	return NULL;
}

struct cmd_results *cmd_border_texture(int argc, char **argv) {
	// Command needs to be defered for texture files to become accessible.
	// Wish I knew why.
	if (config->reading || !config->active) {
		return cmd_results_new(CMD_DEFER, NULL);
	}

	struct cmd_results *error = NULL;

	char *path;
	error = get_texture_path(&path, argc, argv);
	if (error) {
		return error;
	}

	cairo_surface_t *combined_surface = cairo_image_surface_create_from_png(path);
	config->border_textures_manager = create_border_textures_manager(combined_surface);
	if (!config->border_textures_manager) {
		error = cmd_results_new(CMD_FAILURE,
				"Unable to initialize border texture (%s). Is the texture valid?", path);
		free(path);
		return error;
	}
	free(path);
	cairo_surface_destroy(combined_surface);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
