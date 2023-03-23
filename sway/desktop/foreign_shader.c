#include "sway/config.h"

void free_foreign_shader_compile_target(struct foreign_shader_compile_target *target) {
	free(target->frag);
	if (target->vert)
		free(target->vert);
	free(target);
}
