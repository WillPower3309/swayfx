#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"

struct cmd_results *cmd_titlebar_width(int argc, char **argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "titlebar_width", EXPECTED_AT_LEAST, 1))) {
        return error;
    } else if(strcmp(argv[0], "stretch") == 0) {
        config->titlebar_width = T_WIDTH_STRETCH;
    } else if(strcmp(argv[0], "text") == 0) {
        config->titlebar_width = T_WIDTH_TEXT;
    } else if(strcmp(argv[0], "uniform") == 0) {
        config->titlebar_width = T_WIDTH_UNIFORM;
    } else {
        return cmd_results_new(CMD_FAILURE,
                "Expected 'titlebar_width stretch|text|uniform [<px>]'");
    }

    if (argc > 1) {
        config->titlebar_uniform_width = atoi(argv[1]);
    }

    for (int i = 0; i < root->outputs->length; ++i) {
        struct sway_output *output = root->outputs->items[i];
        arrange_workspace(output_get_active_workspace(output));
    }

    return cmd_results_new(CMD_SUCCESS, NULL);
}
