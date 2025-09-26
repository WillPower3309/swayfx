#include "sway/commands.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"

static void arrange_corner_radius_iter(struct sway_container *con, void *data) {
    container_update(con);
    output_configure_scene(NULL, &con->scene_tree->node,
        1.0f, 0, false, false, false, config->rounded_corners.window, NULL);
}

static char *unexpected_msg = "Expected 'rounded_corners (<set|add|remove> <outer|window|titlebar|all> <all|none|top|bottom|left|right|top_left|top_right|bottom_left|bottom_right>...)...'";

struct cmd_results *cmd_rounded_corners(int argc, char **argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "rounded_corners", EXPECTED_AT_LEAST, 3))) {
        return error;
    }

    enum rounded_corner_context {
        RC_OUTER,
        RC_WINDOW,
        RC_TITLEBAR,
        RC_ALL,
    };

    enum rounded_corner_operation {
        RC_ADD,
        RC_REMOVE,
        RC_SET,
    };


    // rounded_corners window set bottom top titlebar add none skip all
    int offset = 0;
    while (offset < (argc - 2)) {
        enum rounded_corner_operation op = RC_SET;
        enum rounded_corner_context context = RC_OUTER;
        enum corner_location value = CORNER_LOCATION_NONE;
        bool has_value = false;

        char* arg = argv[offset++];
        if (strcmp(arg, "set") == 0) {
            op = RC_SET;
        } else if (strcmp(arg, "add") == 0) {
            op = RC_ADD;
        } else if (strcmp(arg, "remove") == 0) {
            op = RC_REMOVE;
        } else {
            return cmd_results_new(
                CMD_INVALID,
                "%s unexpected %s in operation position only expecting <set|add|remove>",
                unexpected_msg,
                arg
            );
        }

        arg = argv[offset++];
        if (strcmp(arg, "outer") == 0) {
            context = RC_OUTER;
        } else if (strcmp(arg, "window") == 0) {
            context = RC_WINDOW;
        } else if (strcmp(arg, "titlebar") == 0) {
            context = RC_TITLEBAR;
        } else if (strcmp(arg, "all") == 0) {
            context = RC_ALL;
        } else {
            return cmd_results_new(
                CMD_INVALID,
                "%s unexpected %s in context position only expecting <outer|window|titlebar|all>",
                unexpected_msg,
                arg
            );
        }

        while (offset < argc) {
            arg = argv[offset++];
            if (strcmp(arg, "all") == 0) {
                value |= CORNER_LOCATION_ALL;
            } else if (strcmp(arg, "none") == 0) {
                value |= CORNER_LOCATION_NONE;
            } else if (strcmp(arg, "bottom") == 0) {
                value |= CORNER_LOCATION_BOTTOM;
            } else if (strcmp(arg, "top") == 0) {
                value |= CORNER_LOCATION_TOP;
            } else if (strcmp(arg, "left") == 0) {
                value |= CORNER_LOCATION_LEFT;
            } else if (strcmp(arg, "right") == 0) {
                value |= CORNER_LOCATION_RIGHT;
            } else if (strcmp(arg, "top_left") == 0) {
                value |= CORNER_LOCATION_TOP_LEFT;
            } else if (strcmp(arg, "top_right") == 0) {
                value |= CORNER_LOCATION_TOP_RIGHT;
            } else if (strcmp(arg, "bottom_left") == 0) {
                value |= CORNER_LOCATION_BOTTOM_LEFT;
            } else if (strcmp(arg, "bottom_right") == 0) {
                value |= CORNER_LOCATION_BOTTOM_RIGHT;
            } else if (has_value) {
                // we already did something with the values given
                // but the current value isn't understood
                // which (probably) just means we encountered a new operation
                offset--;
                break;
            } else {
                return cmd_results_new(
                    CMD_INVALID,
                    "%s unexpected %s in value position only expecting location (e.g. bottom_left)",
                    unexpected_msg,
                    arg
                );
            }


            has_value = true;
        }

        switch (op) {
            case RC_SET:
                switch (context) {
                case RC_OUTER:
                        config->rounded_corners.titlebar = (CORNER_LOCATION_BOTTOM & config->rounded_corners.titlebar) | (value & CORNER_LOCATION_TOP);
                        config->rounded_corners.window = value;
                        break;
                case RC_ALL:
                        config->rounded_corners.titlebar = value;
                        config->rounded_corners.window = value;
                        break;
                case RC_TITLEBAR:
                        config->rounded_corners.titlebar = value;
                        break;
                case RC_WINDOW:
                        config->rounded_corners.window = value;
                        break;
                }
                break;
            case RC_ADD:
                switch (context) {
                case RC_OUTER:
                        config->rounded_corners.titlebar |= (value & CORNER_LOCATION_TOP);
                        config->rounded_corners.window |= value;
                        break;
                case RC_ALL:
                        config->rounded_corners.titlebar |= value;
                        config->rounded_corners.window |= value;
                        break;
                case RC_TITLEBAR:
                        config->rounded_corners.titlebar |= value;
                        break;
                case RC_WINDOW:
                        config->rounded_corners.window |= value;
                        break;
                }
                break;
            case RC_REMOVE:
                switch (context) {
                case RC_OUTER:
                        config->rounded_corners.titlebar &= ~(value & CORNER_LOCATION_TOP);
                        config->rounded_corners.window &= ~value;
                        break;
                case RC_ALL:
                        config->rounded_corners.titlebar &= ~value;
                        config->rounded_corners.window &= ~value;
                        break;
                case RC_TITLEBAR:
                        config->rounded_corners.titlebar &= ~value;
                        break;
                case RC_WINDOW:
                        config->rounded_corners.window &= ~value;
                        break;
                }
                break;
        }
    }

    root_for_each_container(arrange_corner_radius_iter, NULL);
    arrange_root();

    return cmd_results_new(CMD_SUCCESS, "");
}
