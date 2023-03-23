#define _POSIX_C_SOURCE 200809L
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include "sway/commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *stat_file(const char *path) {
	// FIXME: ~ paths don't work
	FILE *f = fopen(path, "r");
	if (!f) {
		return cmd_results_new(CMD_FAILURE, "Failed to open custom shader file: %s", path);
	}
	struct stat statbuf;
	if (fstat(fileno(f), &statbuf) < 0) {
		fclose(f);
		return cmd_results_new(CMD_FAILURE, "Failed to access custom shader file: %s", path);
	}

	fclose(f);
	return NULL;
}

struct cmd_results *set(const char *shader) {
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *load(char *label, char *vert_path, char *frag_path) {
	struct cmd_results *result = cmd_results_new(CMD_SUCCESS, NULL);

	// FIXME: erorr handling
	if (vert_path != NULL) {
		expand_path(&frag_path);
		stat_file(vert_path);
	}
	stat_file(frag_path);

	struct foreign_shader_compile_target *tgt =
		calloc(1, sizeof(struct foreign_shader_compile_target));
	if (!tgt) {
		free(label);
		if (vert_path != NULL)
			free(vert_path);
		free(frag_path);
		return cmd_results_new(CMD_FAILURE, "Allocation error");
	}
	tgt->label = strdup(label);
	if (vert_path != NULL)
		tgt->vert = strdup(vert_path);
	tgt->frag = strdup(frag_path);
	// FIXME: free pointers!
	
	list_add(config->foreign_shader_compile_queue, tgt);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *param(char *shader, char *param) {
	// FIXME: finish
	return cmd_results_new(CMD_SUCCESS, NULL);
}

// FIXME: passing two shader commands ruins everything
struct cmd_results *cmd_shader(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "shader", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	/* struct sway_container *con = config->handler_context.container; */
	/* if (con == NULL) { */
	/* 	return cmd_results_new(CMD_FAILURE, "No current container"); */
	/* } */

	if (argc == 2) {
		// Set shader as default
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	// FIXME: quotes and stuff
	if (!strcmp(argv[0], "load") && (argc == 3 || argc == 4)) {
		char *vert = argc == 4 ? argv[2] : NULL;
		char *frag = argc == 4 ? argv[3] : argv[2];
		if (expand_path(&frag) && (!vert || expand_path(&vert)))
			return load(argv[1], vert, frag);
	} else if (!strcmp(argv[0], "param")) {
		return param(argv[1], NULL);
	}

	return cmd_results_new(CMD_INVALID,
			"Expected: load [vert] <frag>|param", argv[0]);
}
