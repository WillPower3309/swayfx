#include <strings.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "util.h"

struct cmd_results *cmd_colorkey(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "colorkey", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	struct sway_container *con = config->handler_context.container;

	if (con == NULL) {
		return cmd_results_new(CMD_FAILURE, "No current container");
	}

	if (strcasecmp(argv[0], "disable") == 0 ||
			strcasecmp(argv[0], "off") == 0 ||
			strcasecmp(argv[0], "none") == 0) {
		con->colorkey_enabled = false;
		output_configure_scene(NULL, &con->scene_tree->node,
				1.0f, 0, false, false, NULL);
		container_update(con);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	if (argc < 2) {
		return cmd_results_new(CMD_INVALID,
				"Expected: colorkey <src_color> <dst_color> or colorkey disable");
	}

	uint32_t src_color, dst_color;
	if (!parse_color(argv[0], &src_color)) {
		return cmd_results_new(CMD_INVALID, "Invalid source color %s", argv[0]);
	}
	if (!parse_color(argv[1], &dst_color)) {
		return cmd_results_new(CMD_INVALID, "Invalid destination color %s", argv[1]);
	}

	con->colorkey_enabled = true;
	color_to_rgba(con->colorkey_src, src_color);
	color_to_rgba(con->colorkey_dst, dst_color);

	output_configure_scene(NULL, &con->scene_tree->node,
			1.0f, 0, false, false, NULL);
	container_update(con);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
