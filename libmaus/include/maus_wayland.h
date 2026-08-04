#ifndef MAUS_WAYLAND_H
#define MAUS_WAYLAND_H

#include <stddef.h>
#include <stdint.h>

#define MAUS_EVQ_MAX 64

/* must forward declare so that other
   ANSI abiding files arent affected */
struct wl_display;
struct wl_buffer;
struct wl_registry;
struct wl_compositor;
struct wl_surface;
struct wl_shm;
struct wl_seat;
struct wl_pointer;
struct wl_keyboard;
struct wl_cursor_theme;
struct wl_cursor;

struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;

struct zwp_pointer_constraints_v1;
struct zwp_confined_pointer_v1;
struct zwp_relative_pointer_manager_v1;
struct zwp_relative_pointer_v1;

struct xkb_context;
struct xkb_keymap;
struct xkb_state;

typedef struct {
	int8_t  pending;
	int32_t type;

	uint32_t width;
	uint32_t height;

	int32_t  mouse_x;
	int32_t  mouse_y;
	int32_t  mouse_dx;
	int32_t  mouse_dy;
	uint32_t mouse_button;
	uint8_t  mouse_pressed;

	uint32_t key_code;
	uint32_t key_sym;
	char     key_text;
	uint8_t  key_pressed;
} MausEventPending;

typedef struct {
	struct wl_buffer* buffer;
	void*             data;
	size_t            size;
	int8_t            busy;
} MausWLBuffer;

typedef struct {
	struct wl_display*    display;
	struct wl_compositor* compositor;
	struct wl_surface*    surface;

	struct wl_shm* shm;
	MausWLBuffer   buffers[2];

	struct wl_seat*     seat;
	struct wl_pointer*  pointer;
	struct wl_keyboard* keyboard;

	struct wl_cursor_theme* cursor_theme;
	struct wl_cursor*       cursor;
	struct wl_surface*      cursor_surface;
	uint32_t                pointer_enter_serial;
	int32_t                 mouse_x;
	int32_t                 mouse_y;
	int8_t                  mouse_pos_set;
	int8_t                  cursor_state;

	struct zwp_pointer_constraints_v1*      pointer_constraints;
	struct zwp_confined_pointer_v1*         locked_pointer;
	struct zwp_relative_pointer_manager_v1* relative_pointer_manager;
	struct zwp_relative_pointer_v1*         relative_pointer;

	struct xkb_context* xkb_context;
	struct xkb_keymap*  xkb_keymap;
	struct xkb_state*   xkb_state;


	MausEventPending pending;
	MausEventPending evq[MAUS_EVQ_MAX];
	uint32_t evq_head;
	uint32_t evq_tail;
	uint32_t evq_count;

	int32_t  repeat_rate;
	int32_t  repeat_delay;
	uint32_t repeat_key_code;
	uint32_t repeat_key_sym;
	char     repeat_key_text;
	uint64_t repeat_next_ns;

	struct xdg_wm_base*  wm_base;
	struct xdg_surface*  xdg_surface;
	struct xdg_toplevel* xdg_toplevel;
	int8_t               configured;

	struct wl_registry*          registry;
} MausBackend;

#endif /* MAUS_WAYLAND_H */

