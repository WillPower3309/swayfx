#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "util.h"

struct cmd_results *cmd_gap_click_redirect(int argc, char **argv) {
	struct cmd_results *error = checkarg(argc, "gap_click_redirect", EXPECTED_AT_LEAST, 1);

	if (error) {
		return error;
	}

	config->gap_click_redirect = parse_boolean(argv[0], config->gap_click_redirect);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
