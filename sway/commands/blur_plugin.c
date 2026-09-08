#include "render/egl.h"
#include "render/fx_renderer/fx_renderer.h"
#include "scenefx/render/fx_renderer/fx_blur.h"
#include "sway/commands.h"
#include "sway/server.h"

struct cmd_results *cmd_blur_plugin(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "blur_plugin", EXPECTED_EQUAL_TO, 2);
	if (error) {
		return error;
	}

	struct fx_blur_plugin_info *info = fx_blur_plugin_create(argv[0], argv[1]);
	fx_renderer_add_plugin(server.renderer, info);
	fx_blur_plugin_load(info);
	fx_blur_plugin_link(server.renderer, info);

	return cmd_results_new(CMD_SUCCESS, NULL);
}