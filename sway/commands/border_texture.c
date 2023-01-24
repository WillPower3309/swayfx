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

struct cmd_results *cmd_border_texture(int argc, char **argv) {
	// Command needs to be defered for texture files to become accessible.
	// Wish I knew why.
	if (config->reading || !config->active) {
		return cmd_results_new(CMD_DEFER, NULL);
	}

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "border_texture", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	char *path = strdup("contrib/borders/bevel.png");
	if (!expand_path(&path)) {
		error = cmd_results_new(CMD_INVALID, "Invalid syntax (%s)", path);
		free(path);
		return error;
	}
	if (!path) {
		sway_log(SWAY_ERROR, "Failed to allocate expanded path");
		return cmd_results_new(CMD_FAILURE, "Unable to allocate resource");
	}

	bool can_access = access(path, F_OK | R_OK) != -1;
	if (!can_access) {
		sway_log(SWAY_ERROR, "Unable to access border textures file '%s'", path);
		error = cmd_results_new(CMD_FAILURE, "Unable to access border textures file '%s'", path);
		free(path);
		return error;

	}

	cairo_surface_t *combined_surface = cairo_image_surface_create_from_png(path);
	free(path);
	config->border_texture_manager = create_border_textures_manager(combined_surface);
	if (!config->border_texture_manager) {
		return cmd_results_new(CMD_FAILURE, "Unable to initialize border textures. Is the texture valid?");
	}
	cairo_surface_destroy(combined_surface);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
