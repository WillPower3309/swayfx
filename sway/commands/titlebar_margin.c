#include "sway/commands.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"

struct cmd_results *cmd_titlebar_margin(int argc, char **argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "titlebar_margin", EXPECTED_AT_LEAST, 1))) {
        return error;
    }

    char *inv;
    int bottom_value = strtol(argv[0], &inv, 10);
    int top_value = 0;

    if (*inv != '\0' || bottom_value <= 0) {
        return cmd_results_new(CMD_FAILURE, "Invalid size specified");
    }

    if (argc == 2) {
        top_value = strtol(argv[1], &inv, 10);
        if (*inv != '\0' || top_value < 0) {
            return cmd_results_new(CMD_FAILURE, "Invalid size specified");
        }
    }

    config->titlebar_bottom_margin = bottom_value;
    config->titlebar_top_margin = top_value;

    for (int i = 0; i < root->outputs->length; ++i) {
        struct sway_output *output = root->outputs->items[i];
        arrange_workspace(output_get_active_workspace(output));
    }

    return cmd_results_new(CMD_SUCCESS, NULL);
}
