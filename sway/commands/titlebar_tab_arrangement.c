#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"

struct cmd_results *cmd_titlebar_tab_arrangement(int argc, char **argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "titlebar_tab_arrangement", EXPECTED_EQUAL_TO, 1))) {
        return error;
    } else if(strcmp(argv[0], "left") == 0) {
        config->titlebar_tab_arrangement = T_TAB_ARRANGEMENT_LEFT;
    } else if(strcmp(argv[0], "right") == 0) {
        config->titlebar_tab_arrangement = T_TAB_ARRANGEMENT_RIGHT;
    } else if(strcmp(argv[0], "even") == 0) {
        config->titlebar_tab_arrangement = T_TAB_ARRANGEMENT_EVEN;
    } else if(strcmp(argv[0], "center") == 0) {
        config->titlebar_tab_arrangement = T_TAB_ARRANGEMENT_CENTER;
    } else {
        return cmd_results_new(CMD_FAILURE,
                "Expected 'titlebar_tab_arrangement left|right|even|center'");
    }

    for (int i = 0; i < root->outputs->length; ++i) {
        struct sway_output *output = root->outputs->items[i];
        arrange_workspace(output_get_active_workspace(output));
    }

    return cmd_results_new(CMD_SUCCESS, NULL);
}
