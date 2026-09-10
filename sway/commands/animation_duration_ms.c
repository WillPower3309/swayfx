#include <assert.h>
#include <stdlib.h>
#include <strings.h>
#include "sway/animation_manager.h"
#include "sway/commands.h"

static struct cmd_results *animation_duration_ms_command(int argc, char **argv,
		const char *cmd_name, float *duration, bool *explicit_flag) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	char *err;
	float val = strtof(argv[0], &err);
	if (*err) {
		return cmd_results_new(CMD_INVALID, "%s float invalid", cmd_name);
	}

	if (val < 0 || val > 5000) { // surely no one wants an animation longer than 5 seconds
		return cmd_results_new(CMD_FAILURE, "%s value out of bounds", cmd_name);
	}

	*duration = val;
	if (explicit_flag) {
		*explicit_flag = true;
	}
	refresh_animation_manager_timing();

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_animation_duration_ms(int argc, char **argv) {
	return animation_duration_ms_command(argc, argv, "animation_duration_ms",
			&config->animation_duration_ms, NULL);
}

struct cmd_results *cmd_animation_duration_ms_open(int argc, char **argv) {
	return animation_duration_ms_command(argc, argv, "animation_duration_ms.open",
			&config->animation_duration_ms_by_type[ANIM_WINDOW_OPEN],
			&config->animation_duration_ms_set[ANIM_WINDOW_OPEN]);
}

struct cmd_results *cmd_animation_duration_ms_close(int argc, char **argv) {
	return animation_duration_ms_command(argc, argv, "animation_duration_ms.close",
			&config->animation_duration_ms_by_type[ANIM_WINDOW_CLOSE],
			&config->animation_duration_ms_set[ANIM_WINDOW_CLOSE]);
}

struct cmd_results *cmd_animation_duration_ms_move(int argc, char **argv) {
	return animation_duration_ms_command(argc, argv, "animation_duration_ms.move",
			&config->animation_duration_ms_by_type[ANIM_WINDOW_MOVE],
			&config->animation_duration_ms_set[ANIM_WINDOW_MOVE]);
}

struct cmd_results *cmd_animation_duration_ms_workspace(int argc, char **argv) {
	return animation_duration_ms_command(argc, argv, "animation_duration_ms.workspace",
			&config->animation_duration_ms_by_type[ANIM_WORKSPACE_SWITCH],
			&config->animation_duration_ms_set[ANIM_WORKSPACE_SWITCH]);
}
