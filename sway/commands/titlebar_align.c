#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"


struct cmd_results *cmd_titlebar_align(int argc, char **argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "titlebar_align", EXPECTED_AT_LEAST, 1))) {
        return error;
    }

    if (strcmp(argv[0], "left") == 0) {
        config->titlebar_align = ALIGN_LEFT;
    } else if (strcmp(argv[0], "center") == 0) {
        config->titlebar_align = ALIGN_CENTER;
    } else if (strcmp(argv[0], "right") == 0) {
        config->titlebar_align = ALIGN_RIGHT;
    } else {
        return cmd_results_new(CMD_INVALID,
                "Expected 'titlebar_align left|center|right'");
    }

    for (int i = 0; i < root->outputs->length; ++i) {
        struct sway_output *output = root->outputs->items[i];
        arrange_workspace(output_get_active_workspace(output));
    }

    return cmd_results_new(CMD_SUCCESS, NULL);
}
