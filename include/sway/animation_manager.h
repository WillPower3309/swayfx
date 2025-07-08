#ifndef _SWAY_ANIMATION_MANAGER_H
#define _SWAY_ANIMATION_MANAGER_H

#include "sway/server.h"

void animation_manager_init(struct sway_server *server);

void refresh_animation_manager_timing();

void start_animation(void (update_callback)(void), void (complete_callback)(void));

int get_animated_value(float from, float to);

#endif
