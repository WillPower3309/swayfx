#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"

struct cmd_results *cmd_blur_noise(int argc, char **argv) {
    struct cmd_results *error = NULL;
    if ((error = checkarg(argc, "blur_noise", EXPECTED_EQUAL_TO, 1))) {
        return error;
    }

    char *inv;
    float value = strtof(argv[0], &inv);
    if (*inv != '\0' || value < 0 || value > 1) {
        return cmd_results_new(CMD_FAILURE, "Invalid noise specified");
    }

    config->blur_params.noise = value;

    struct sway_output *output;
    wl_list_for_each(output, &root->all_outputs, link) {
        if (output->renderer) {
            output->renderer->blur_buffer_dirty = true;
            output_damage_whole(output);
        }
    }

    return cmd_results_new(CMD_SUCCESS, NULL);
}
