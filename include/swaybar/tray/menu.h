#ifndef _SWAYBAR_TRAY_MENU_H
#define _SWAYBAR_TRAY_MENU_H

#include <cairo.h>
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include "pool-buffer.h"

struct swaybar;
struct swaybar_output;
struct swaybar_sni;
struct swaybar_seat;

#define MENU_ITEM_HEIGHT 28
#define MENU_PADDING_X 12
#define MENU_PADDING_Y 4
#define MENU_SEPARATOR_HEIGHT 9
#define MENU_MIN_WIDTH 180
#define MENU_TOGGLE_SIZE 14
#define MENU_TOGGLE_MARGIN 6
#define MENU_SUBMENU_ARROW_SIZE 8
#define MENU_BG_COLOR 0x2E2E3EFF
#define MENU_BG_HOVER_COLOR 0x4444AAFF
#define MENU_TEXT_COLOR 0xDDDDDDFF
#define MENU_TEXT_DISABLED_COLOR 0x777777FF
#define MENU_SEPARATOR_COLOR 0x555577FF
#define MENU_BORDER_COLOR 0x6633BBFF
#define MENU_CHECK_COLOR 0x88CCFFFF

enum menu_toggle_type {
	MENU_TOGGLE_NONE = 0,
	MENU_TOGGLE_CHECKMARK,
	MENU_TOGGLE_RADIO,
};

enum menu_toggle_state {
	MENU_TOGGLE_STATE_OFF = 0,
	MENU_TOGGLE_STATE_ON = 1,
	MENU_TOGGLE_STATE_INDETERMINATE = -1,
};

struct swaybar_menu_item {
	int32_t id;
	char *label;
	char *icon_name;
	uint8_t *icon_data;
	size_t icon_data_len;
	cairo_surface_t *icon_surface; // rendered icon cache
	bool enabled;
	bool is_separator;
	bool has_submenu;
	enum menu_toggle_type toggle_type;
	enum menu_toggle_state toggle_state;
};

struct swaybar_menu {
	struct swaybar *bar;
	struct swaybar_output *output;
	struct swaybar_sni *sni;

	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct pool_buffer buffers[2];

	// Invisible fullscreen scrim for click-outside-to-dismiss
	struct wl_surface *scrim_surface;
	struct zwlr_layer_surface_v1 *scrim_layer_surface;
	struct pool_buffer scrim_buffers[2];
	bool scrim_configured;

	struct swaybar_menu_item *items;
	int item_count;
	int hovered_index; // -1 for none

	uint32_t width, height;
	int anchor_x; // x position of the tray icon that spawned this menu
	int toggle_column_width;  // extra left margin for toggle indicators
	int submenu_column_width; // extra right margin for submenu arrows
	bool configured;
	bool dirty;
};

// Global menu state (only one menu open at a time)
struct swaybar_menu *tray_menu_create(struct swaybar *bar);
void tray_menu_destroy(struct swaybar_menu *menu);

// Open the menu for a given SNI at the given position
void tray_menu_open(struct swaybar_menu *menu, struct swaybar_sni *sni,
		struct swaybar_output *output, int anchor_x);

// Close the menu if open
void tray_menu_close(struct swaybar_menu *menu);

// Returns true if the menu is currently open
bool tray_menu_is_open(struct swaybar_menu *menu);

// Input handling - returns true if the event was consumed
bool tray_menu_pointer_enter(struct swaybar_menu *menu,
		struct wl_surface *surface);
bool tray_menu_is_scrim(struct swaybar_menu *menu,
		struct wl_surface *surface);
void tray_menu_pointer_motion(struct swaybar_menu *menu, double x, double y);
bool tray_menu_pointer_button(struct swaybar_menu *menu, uint32_t button,
		uint32_t state);
void tray_menu_pointer_leave(struct swaybar_menu *menu);

// Render the menu
void tray_menu_render(struct swaybar_menu *menu);

#endif
