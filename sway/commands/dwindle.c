#include "sway/commands.h"
#include "util.h"

struct cmd_results *cmd_dwindle(int argc, char **argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "dwindle", EXPECTED_EQUAL_TO, 1))) {
        return error;
    }
    config->dwindle = parse_boolean(argv[0], config->dwindle);
    return cmd_results_new(CMD_SUCCESS, NULL);
}
