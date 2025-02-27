#include <stdint.h>
#include <stdlib.h>
#include <wlr/util/addon.h>
#include "log.h"
#include "sway/scene_descriptor.h"

struct scene_descriptor {
	void *data;
	struct wlr_addon addon;
};

static const struct wlr_addon_interface addon_interface;

static struct scene_descriptor *scene_node_get_descriptor(
		struct wlr_scene_node *node, enum sway_scene_descriptor_type type) {
	struct wlr_addon *addon = wlr_addon_find(&node->addons, (void *)type, &addon_interface);
	if (!addon) {
		return NULL;
	}

	struct scene_descriptor *desc = wl_container_of(addon, desc, addon);
	return desc;
}

static void descriptor_destroy(struct scene_descriptor *desc) {
	wlr_addon_finish(&desc->addon);
	free(desc);
}

void *scene_descriptor_try_get(struct wlr_scene_node *node,
		enum sway_scene_descriptor_type type) {
	struct scene_descriptor *desc = scene_node_get_descriptor(node, type);
	if (!desc) {
		return NULL;
	}

	return desc->data;
}

void scene_descriptor_destroy(struct wlr_scene_node *node,
		enum sway_scene_descriptor_type type) {
	struct scene_descriptor *desc = scene_node_get_descriptor(node, type);
	if (!desc) {
		return;
	}
	descriptor_destroy(desc);
}

static void addon_handle_destroy(struct wlr_addon *addon) {
	struct scene_descriptor *desc = wl_container_of(addon, desc, addon);
	descriptor_destroy(desc);
}

static const struct wlr_addon_interface addon_interface = {
	.name = "sway_scene_descriptor",
	.destroy = addon_handle_destroy,
};

bool scene_descriptor_assign(struct wlr_scene_node *node,
		enum sway_scene_descriptor_type type, void *data) {
	struct scene_descriptor *desc = calloc(1, sizeof(*desc));
	if (!desc) {
		sway_log(SWAY_ERROR, "Could not allocate a scene descriptor");
		return false;
	}

	wlr_addon_init(&desc->addon, &node->addons, (void *)type, &addon_interface);
	desc->data = data;
	return true;
}

struct sway_scene_descriptor *scene_descriptor_try_get_first(struct wlr_scene_node *node) {
	struct wlr_addon *addon;
	wl_list_for_each(addon, &node->addons.addons, link) {
		if (addon->impl != &addon_interface) {
			continue;
		}
		enum sway_scene_descriptor_type desc_type = (uintptr_t) addon->owner;
		switch (desc_type) {
		case SWAY_SCENE_DESC_BUFFER_TIMER:
		case SWAY_SCENE_DESC_NON_INTERACTIVE:
		case SWAY_SCENE_DESC_CONTAINER:
		case SWAY_SCENE_DESC_VIEW:
		case SWAY_SCENE_DESC_LAYER_SHELL:
		case SWAY_SCENE_DESC_XWAYLAND_UNMANAGED:
		case SWAY_SCENE_DESC_POPUP:
		case SWAY_SCENE_DESC_DRAG_ICON:;
			struct scene_descriptor *desc = wl_container_of(addon, desc, addon);
			if (desc) {
				struct sway_scene_descriptor *result = malloc(sizeof(*result));
				if (result) {
					result->type = desc_type;
					result->data = desc->data;
					return result;
				}
			}
			break;
		default:
			break;
		}
	}
	return NULL;
}
