#include "sway/commands.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"

struct cmd_results *cmd_titlebar_margin_collapse(int argc, char **argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "titlebar_margin_collapse", EXPECTED_EQUAL_TO, 1))) {
        return error;
    } else if(strcmp(argv[0], "separate") == 0) {
        config->titlebar_margin_collapse = T_MARGIN_COLLAPSE_SEPARATE;
    } else if(strcmp(argv[0], "collapse") == 0) {
        config->titlebar_margin_collapse = T_MARGIN_COLLAPSE_COLLAPSE;
    } else if(strcmp(argv[0], "only_gaps") == 0) {
        config->titlebar_margin_collapse = T_MARGIN_COLLAPSE_ONLY_GAPS;
    } else if(strcmp(argv[0], "only_margins") == 0) {
        config->titlebar_margin_collapse = T_MARGIN_COLLAPSE_ONLY_MARGINS;
    } else {
        return cmd_results_new(CMD_FAILURE,
                "Expected 'titlebar_margin_collapse separate|collapse|only_gaps|only_margins'");
    }


    for (int i = 0; i < root->outputs->length; ++i) {
        struct sway_output *output = root->outputs->items[i];
        arrange_workspace(output_get_active_workspace(output));
    }

    return cmd_results_new(CMD_SUCCESS, NULL);
}
