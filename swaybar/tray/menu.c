#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include <assert.h>
#include <cairo.h>
#include <math.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input-event-codes.h>
#include "swaybar/bar.h"
#include "swaybar/config.h"
#include "swaybar/tray/item.h"
#include "swaybar/tray/menu.h"
#include "swaybar/tray/tray.h"
#include "cairo_util.h"
#include "log.h"
#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static void menu_layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct swaybar_menu *menu = data;
	menu->width = width;
	menu->height = height;
	menu->configured = true;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	tray_menu_render(menu);
}

static void menu_layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	struct swaybar_menu *menu = data;
	tray_menu_close(menu);
}

static const struct zwlr_layer_surface_v1_listener menu_layer_surface_listener = {
	.configure = menu_layer_surface_configure,
	.closed = menu_layer_surface_closed,
};

struct swaybar_menu *tray_menu_create(struct swaybar *bar) {
	struct swaybar_menu *menu = calloc(1, sizeof(struct swaybar_menu));
	if (!menu) {
		return NULL;
	}
	menu->bar = bar;
	menu->hovered_index = -1;
	return menu;
}

void tray_menu_destroy(struct swaybar_menu *menu) {
	if (!menu) {
		return;
	}
	tray_menu_close(menu);
	free(menu);
}

static void menu_clear_items(struct swaybar_menu *menu) {
	if (menu->items) {
		for (int i = 0; i < menu->item_count; i++) {
			free(menu->items[i].label);
		}
		free(menu->items);
		menu->items = NULL;
		menu->item_count = 0;
	}
}

static int handle_menu_layout(sd_bus_message *msg, void *userdata,
		sd_bus_error *error) {
	struct swaybar_menu *menu = userdata;

	if (sd_bus_message_is_method_error(msg, NULL)) {
		const sd_bus_error *err = sd_bus_message_get_error(msg);
		sway_log(SWAY_ERROR, "dbusmenu GetLayout failed: %s", err->message);
		return -1;
	}

	menu_clear_items(menu);

	uint32_t revision;
	int ret = sd_bus_message_read(msg, "u", &revision);
	if (ret < 0) return ret;

	// Enter root struct (ia{sv}av)
	ret = sd_bus_message_enter_container(msg, 'r', "ia{sv}av");
	if (ret < 0) return ret;

	int32_t root_id;
	sd_bus_message_read(msg, "i", &root_id);

	// Skip root properties
	sd_bus_message_enter_container(msg, 'a', "{sv}");
	while (sd_bus_message_enter_container(msg, 'e', "sv") > 0) {
		sd_bus_message_skip(msg, "sv");
		sd_bus_message_exit_container(msg);
	}
	sd_bus_message_exit_container(msg);

	// Count children first by collecting into a temporary array
	// We'll allocate generously and shrink later
	int capacity = 64;
	struct swaybar_menu_item *items = calloc(capacity, sizeof(struct swaybar_menu_item));
	int count = 0;

	ret = sd_bus_message_enter_container(msg, 'a', "v");
	if (ret < 0) {
		free(items);
		return ret;
	}

	while (sd_bus_message_enter_container(msg, 'v', "(ia{sv}av)") > 0) {
		sd_bus_message_enter_container(msg, 'r', "ia{sv}av");

		int32_t item_id;
		sd_bus_message_read(msg, "i", &item_id);

		char *label = NULL;
		bool is_separator = false;
		bool enabled = true;
		bool visible = true;

		sd_bus_message_enter_container(msg, 'a', "{sv}");
		while (sd_bus_message_enter_container(msg, 'e', "sv") > 0) {
			const char *prop;
			sd_bus_message_read(msg, "s", &prop);

			if (strcmp(prop, "label") == 0) {
				if (sd_bus_message_enter_container(msg, 'v', "s") >= 0) {
					const char *val;
					sd_bus_message_read(msg, "s", &val);
					label = strdup(val);
					sd_bus_message_exit_container(msg);
				} else {
					sd_bus_message_skip(msg, "v");
				}
			} else if (strcmp(prop, "type") == 0) {
				if (sd_bus_message_enter_container(msg, 'v', "s") >= 0) {
					const char *val;
					sd_bus_message_read(msg, "s", &val);
					if (strcmp(val, "separator") == 0) {
						is_separator = true;
					}
					sd_bus_message_exit_container(msg);
				} else {
					sd_bus_message_skip(msg, "v");
				}
			} else if (strcmp(prop, "enabled") == 0) {
				if (sd_bus_message_enter_container(msg, 'v', "b") >= 0) {
					int val;
					sd_bus_message_read(msg, "b", &val);
					enabled = val;
					sd_bus_message_exit_container(msg);
				} else {
					sd_bus_message_skip(msg, "v");
				}
			} else if (strcmp(prop, "visible") == 0) {
				if (sd_bus_message_enter_container(msg, 'v', "b") >= 0) {
					int val;
					sd_bus_message_read(msg, "b", &val);
					visible = val;
					sd_bus_message_exit_container(msg);
				} else {
					sd_bus_message_skip(msg, "v");
				}
			} else {
				sd_bus_message_skip(msg, "v");
			}
			sd_bus_message_exit_container(msg); // dict entry
		}
		sd_bus_message_exit_container(msg); // a{sv}

		// Skip children
		sd_bus_message_skip(msg, "av");
		sd_bus_message_exit_container(msg); // (ia{sv}av)
		sd_bus_message_exit_container(msg); // v

		if (visible && count < capacity) {
			items[count].id = item_id;
			items[count].label = label ? label : (is_separator ? NULL : strdup("???"));
			items[count].is_separator = is_separator;
			items[count].enabled = enabled && !is_separator;
			count++;
		} else {
			free(label);
		}
	}

	menu->items = items;
	menu->item_count = count;

	sway_log(SWAY_DEBUG, "dbusmenu: loaded %d menu items", count);

	// Now compute size and create/show the surface
	int max_text_width = MENU_MIN_WIDTH;
	PangoLayout *layout = NULL;

	// We need a temporary cairo surface to measure text
	cairo_surface_t *tmp = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
	cairo_t *cr = cairo_create(tmp);
	layout = pango_cairo_create_layout(cr);

	if (menu->bar->config->font_description) {
		pango_layout_set_font_description(layout,
				menu->bar->config->font_description);
	}

	for (int i = 0; i < count; i++) {
		if (!items[i].is_separator && items[i].label) {
			pango_layout_set_text(layout, items[i].label, -1);
			int w, h;
			pango_layout_get_pixel_size(layout, &w, &h);
			if (w > max_text_width) {
				max_text_width = w;
			}
		}
	}

	g_object_unref(layout);
	cairo_destroy(cr);
	cairo_surface_destroy(tmp);

	// Calculate total menu size
	int total_height = MENU_PADDING_Y * 2; // top/bottom padding
	for (int i = 0; i < count; i++) {
		total_height += items[i].is_separator ? MENU_SEPARATOR_HEIGHT : MENU_ITEM_HEIGHT;
	}
	int total_width = max_text_width + MENU_PADDING_X * 2;

	// Create the layer surface
	if (menu->layer_surface) {
		// Already have a surface, just resize
		zwlr_layer_surface_v1_set_size(menu->layer_surface, total_width, total_height);
		wl_surface_commit(menu->surface);
	} else {
		menu->surface = wl_compositor_create_surface(menu->bar->compositor);
		assert(menu->surface);

		menu->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
				menu->bar->layer_shell, menu->surface, menu->output->output,
				ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "menu");
		assert(menu->layer_surface);

		zwlr_layer_surface_v1_add_listener(menu->layer_surface,
				&menu_layer_surface_listener, menu);

		// Anchor to bottom-right to position near the tray
		struct swaybar_config *config = menu->bar->config;
		bool top_bar = config->position & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;

		if (top_bar) {
			zwlr_layer_surface_v1_set_anchor(menu->layer_surface,
					ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
					ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
			// Margin: push down from top by bar height, push left from right edge
			int right_margin = menu->output->width - menu->anchor_x - total_width / 2;
			if (right_margin < 0) right_margin = 0;
			int top_margin = menu->output->height > 0 ?
					(int)menu->output->height : 30;
			zwlr_layer_surface_v1_set_margin(menu->layer_surface,
					top_margin + 4, right_margin, 0, 0);
		} else {
			zwlr_layer_surface_v1_set_anchor(menu->layer_surface,
					ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
					ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
			// Margin: push up from bottom by bar height
			int right_margin = menu->output->width - menu->anchor_x - total_width / 2;
			if (right_margin < 0) right_margin = 0;
			int bottom_margin = menu->output->height > 0 ?
					(int)menu->output->height : 30;
			zwlr_layer_surface_v1_set_margin(menu->layer_surface,
					0, right_margin, bottom_margin + 4, 0);
		}

		zwlr_layer_surface_v1_set_size(menu->layer_surface, total_width, total_height);
		zwlr_layer_surface_v1_set_exclusive_zone(menu->layer_surface, -1);
		// Allow keyboard interaction
		zwlr_layer_surface_v1_set_keyboard_interactivity(menu->layer_surface,
				ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);

		wl_surface_commit(menu->surface);
	}

	return 0;
}

void tray_menu_open(struct swaybar_menu *menu, struct swaybar_sni *sni,
		struct swaybar_output *output, int anchor_x) {
	if (!menu || !sni || !sni->menu) {
		return;
	}

	// Close existing menu first
	if (menu->layer_surface) {
		tray_menu_close(menu);
	}

	menu->sni = sni;
	menu->output = output;
	menu->anchor_x = anchor_x;
	menu->hovered_index = -1;
	menu->configured = false;

	sway_log(SWAY_DEBUG, "Opening tray menu for %s at x=%d", sni->watcher_id, anchor_x);

	// Call AboutToShow first, then GetLayout
	sd_bus_call_method_async(sni->tray->bus, NULL, sni->service,
			sni->menu, "com.canonical.dbusmenu", "AboutToShow",
			NULL, NULL, "i", (int32_t)0);

	sd_bus_call_method_async(sni->tray->bus, NULL, sni->service,
			sni->menu, "com.canonical.dbusmenu", "GetLayout",
			handle_menu_layout, menu, "iias", (int32_t)0, (int32_t)1, 0);
}

void tray_menu_close(struct swaybar_menu *menu) {
	if (!menu) {
		return;
	}

	sway_log(SWAY_DEBUG, "Closing tray menu");

	if (menu->layer_surface) {
		zwlr_layer_surface_v1_destroy(menu->layer_surface);
		menu->layer_surface = NULL;
	}
	if (menu->surface) {
		wl_surface_destroy(menu->surface);
		menu->surface = NULL;
	}

	destroy_buffer(&menu->buffers[0]);
	destroy_buffer(&menu->buffers[1]);

	menu_clear_items(menu);
	menu->sni = NULL;
	menu->output = NULL;
	menu->configured = false;
	menu->width = 0;
	menu->height = 0;
}

bool tray_menu_is_open(struct swaybar_menu *menu) {
	return menu && menu->layer_surface != NULL;
}

bool tray_menu_pointer_enter(struct swaybar_menu *menu,
		struct wl_surface *surface) {
	return menu && menu->surface == surface;
}

void tray_menu_pointer_motion(struct swaybar_menu *menu, double x, double y) {
	if (!menu || !menu->items) {
		return;
	}

	int old_hover = menu->hovered_index;
	menu->hovered_index = -1;

	double cy = MENU_PADDING_Y;
	for (int i = 0; i < menu->item_count; i++) {
		double item_h = menu->items[i].is_separator ?
				MENU_SEPARATOR_HEIGHT : MENU_ITEM_HEIGHT;
		if (y >= cy && y < cy + item_h && !menu->items[i].is_separator
				&& menu->items[i].enabled) {
			menu->hovered_index = i;
			break;
		}
		cy += item_h;
	}

	if (menu->hovered_index != old_hover) {
		tray_menu_render(menu);
	}
}

bool tray_menu_pointer_button(struct swaybar_menu *menu, uint32_t button,
		uint32_t state) {
	if (!menu || !tray_menu_is_open(menu)) {
		return false;
	}

	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return true; // consume release too
	}

	if (button == BTN_LEFT && menu->hovered_index >= 0) {
		struct swaybar_menu_item *item = &menu->items[menu->hovered_index];
		sway_log(SWAY_INFO, "Menu item clicked: id=%d label='%s'",
				item->id, item->label ? item->label : "");

		// Send dbusmenu Event
		if (menu->sni && menu->sni->menu) {
			sd_bus_call_method_async(menu->sni->tray->bus, NULL,
					menu->sni->service, menu->sni->menu,
					"com.canonical.dbusmenu", "Event",
					NULL, NULL, "isvu", item->id, "clicked", "s", "",
					(uint32_t)0);
		}
		tray_menu_close(menu);
		return true;
	}

	// Any other click closes the menu
	tray_menu_close(menu);
	return true;
}

void tray_menu_pointer_leave(struct swaybar_menu *menu) {
	if (!menu) {
		return;
	}
	if (menu->hovered_index != -1) {
		menu->hovered_index = -1;
		tray_menu_render(menu);
	}
}

void tray_menu_render(struct swaybar_menu *menu) {
	if (!menu || !menu->configured || !menu->surface || !menu->layer_surface) {
		return;
	}
	if (menu->width == 0 || menu->height == 0 || menu->item_count == 0) {
		return;
	}

	int scale = menu->output ? menu->output->scale : 1;
	struct pool_buffer *buffer = get_next_buffer(menu->bar->shm,
			menu->buffers, menu->width * scale, menu->height * scale);
	if (!buffer) {
		sway_log(SWAY_ERROR, "Failed to get buffer for menu");
		return;
	}

	cairo_t *cairo = buffer->cairo;
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_restore(cairo);

	cairo_scale(cairo, scale, scale);

	// Draw background with rounded corners
	double w = menu->width;
	double h = menu->height;
	double r = 8.0; // corner radius

	cairo_new_path(cairo);
	cairo_arc(cairo, r, r, r, M_PI, 1.5 * M_PI);
	cairo_arc(cairo, w - r, r, r, 1.5 * M_PI, 2 * M_PI);
	cairo_arc(cairo, w - r, h - r, r, 0, 0.5 * M_PI);
	cairo_arc(cairo, r, h - r, r, 0.5 * M_PI, M_PI);
	cairo_close_path(cairo);

	cairo_set_source_u32(cairo, MENU_BG_COLOR);
	cairo_fill_preserve(cairo);
	cairo_set_source_u32(cairo, MENU_BORDER_COLOR);
	cairo_set_line_width(cairo, 1.0);
	cairo_stroke(cairo);

	// Draw menu items
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	if (menu->bar->config->font_description) {
		pango_layout_set_font_description(layout,
				menu->bar->config->font_description);
	}

	double y = MENU_PADDING_Y;
	for (int i = 0; i < menu->item_count; i++) {
		struct swaybar_menu_item *item = &menu->items[i];

		if (item->is_separator) {
			// Draw separator line
			cairo_set_source_u32(cairo, MENU_SEPARATOR_COLOR);
			cairo_set_line_width(cairo, 1.0);
			cairo_move_to(cairo, MENU_PADDING_X / 2.0, y + MENU_SEPARATOR_HEIGHT / 2.0);
			cairo_line_to(cairo, w - MENU_PADDING_X / 2.0, y + MENU_SEPARATOR_HEIGHT / 2.0);
			cairo_stroke(cairo);
			y += MENU_SEPARATOR_HEIGHT;
			continue;
		}

		// Draw hover highlight
		if (i == menu->hovered_index) {
			cairo_set_source_u32(cairo, MENU_BG_HOVER_COLOR);
			// Clip to rounded rect for items at top/bottom
			cairo_rectangle(cairo, 2, y, w - 4, MENU_ITEM_HEIGHT);
			cairo_fill(cairo);
		}

		// Draw label
		if (item->label) {
			pango_layout_set_text(layout, item->label, -1);

			if (item->enabled) {
				cairo_set_source_u32(cairo, MENU_TEXT_COLOR);
			} else {
				cairo_set_source_u32(cairo, MENU_TEXT_DISABLED_COLOR);
			}

			int tw, th;
			pango_layout_get_pixel_size(layout, &tw, &th);
			cairo_move_to(cairo, MENU_PADDING_X,
					y + (MENU_ITEM_HEIGHT - th) / 2.0);
			pango_cairo_show_layout(cairo, layout);
		}

		y += MENU_ITEM_HEIGHT;
	}

	g_object_unref(layout);

	// Commit the surface
	wl_surface_set_buffer_scale(menu->surface, scale);
	wl_surface_attach(menu->surface, buffer->buffer, 0, 0);
	wl_surface_damage(menu->surface, 0, 0, menu->width, menu->height);
	wl_surface_commit(menu->surface);
}
