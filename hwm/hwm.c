/*
 * Copyright (c) 2014-2015, 2017, 2024
 * Hans Toshihide Törnqvist <hans.tornqvist@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * TODO:
 * Open child windows on same workspace as parent and make urgent? Verify.
 * Some clients are not deleted properly.
 * Really keep mouse warping for alt-tab?
 */

#include <sys/queue.h>
#include <sys/time.h>
#include <assert.h>
#include <err.h>
#include <iconv.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/randr.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_icccm.h>
#include <xcb/xcb_keysyms.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#define FUNC_RETURNS __attribute__ ((warn_unused_result))

#define CALLOC(ptr, num) do {\
		ptr = calloc(num, sizeof *ptr); \
		if (!ptr) { \
			err(EXIT_FAILURE, "%s:%d: calloc(%d,%d)", \
			    __FILE__, __LINE__, (int)num, (int)sizeof *ptr); \
		} \
	} while (0)
#define CONVERGE(op, ref, test, cur) \
	cur = ref op test && test op cur ? test : cur
#define FREE(x) do { \
		free(x); \
		x = NULL; \
	} while (0)
#define HEIGHT_B(c) (c->height + 2 * c->border_width)
#define HWM_XCB_CHECKED(msg, func, args) do { \
		xcb_void_cookie_t cookie_; \
		xcb_generic_error_t *error_; \
		cookie_ = func args; \
		if (NULL != (error_ = xcb_request_check(g_conn, cookie_))) { \
			fprintf(stderr, msg" (%u,%u,0x%08x,%d,%d).\n", \
			    error_->response_type, error_->error_code, \
			    error_->resource_id, error_->minor_code, \
			    error_->major_code); \
			free(error_); \
			exit(EXIT_FAILURE); \
		} \
	} while (0)
#define LENGTH(x) (sizeof x / sizeof *x)
#define MAX(a, b) (a > b ? a : b)
#define MIN(a, b) (a < b ? a : b)
#define SNAP(op, ref, test, margin) \
	test = ref op test && test op margin ? ref : test
#define VIEW_BOTTOM(v) (v->y + v->height)
#define VIEW_LEFT(v) (v->x)
#define VIEW_RIGHT(v) (v->x + v->width)
#define VIEW_TOP(v) (v->y)
#define WIDTH_B(c) (c->width + 2 * c->border_width)
#define ZERO(x) memset(&x, 0, sizeof x)

enum Click { CLICK_ROOT, CLICK_WORKSPACE, CLICK_STATUS, CLICK_CLIENT };
enum JumpDirection { DIR_EAST, DIR_NORTH, DIR_WEST, DIR_SOUTH };
enum Maximize { MAX_NOPE, MAX_BOTH, MAX_VERT };
enum RunControl { RUN_LOOP, RUN_QUIT, RUN_RESTART };
enum Visibility { VISIBLE, HIDDEN };

struct Arg {
	int	i;
	void	const *v;
};
struct String {
	size_t	length;
	char	str[80];
};
TAILQ_HEAD(ViewList, View);
struct View {
	xcb_randr_output_t	output;
	int	x, y;
	int	width, height;
	TAILQ_ENTRY(View)	next;
};
TAILQ_HEAD(ClientList, Client);
struct Client {
	xcb_window_t	window;
	xcb_size_hints_t	hints;
	int	is_urgent;
	struct	String name;
	int	x, y;
	int	width, height;
	int	border_width;
	enum	Maximize maximize;
	int	max_old_x, max_old_y;
	int	max_old_width, max_old_height;
	TAILQ_ENTRY(Client)	next;
};
struct ButtonBind {
	enum	Click click;
	int	code;
	int	state;
	void	(*action)(struct Arg const *);
};
struct KeyBind {
	xcb_keysym_t	keysym;
	int	state;
	void	(*action)(struct Arg const *);
	struct	Arg arg;
};

static void			action_client_browse(struct Arg const *);
static void			action_client_expand(struct Arg const *);
static void			action_client_grow(struct Arg const *);
static void			action_client_jump(struct Arg const *);
static void			action_client_maximize(struct Arg const *);
static void			action_client_move(struct Arg const *);
static void			action_client_relocate(struct Arg const *);
static void			action_client_resize(struct Arg const *);
static void			action_exec(struct Arg const *);
static void			action_furnish(struct Arg const *);
static void			action_kill(struct Arg const *);
static void			action_quit(struct Arg const *);
static void			action_workspace_select(struct Arg const *);
static xcb_atom_t		atom_get(char const *) FUNC_RETURNS;
static void			bar_draw(void);
static void			bar_reset(void);
static void			button_grab(struct Client *);
static struct Client		*client_add(xcb_window_t, xcb_window_t)
	FUNC_RETURNS;
static struct Client		*client_add_details(xcb_window_t,
    xcb_window_t, int const *) FUNC_RETURNS;
static void			client_free(struct Client **);
static void			client_focus(struct Client *, int, int, int);
static struct Client		*client_get(xcb_window_t, size_t *)
	FUNC_RETURNS;
static void			client_move(struct Client *, enum Visibility);
static void			client_name_update(struct Client *);
static void			client_place(struct Client *);
static void			client_resize(struct Client *, int);
static void			client_snap_dimension(struct Client *);
static void			client_snap_position(struct Client *);
static uint32_t			color_get(char const *) FUNC_RETURNS;
static xcb_cursor_t		cursor_get(xcb_font_t, int) FUNC_RETURNS;
static void			event_button_press(xcb_button_press_event_t
    const *);
static void			event_configure_notify(
    xcb_configure_notify_event_t const *);
static void			event_configure_request(
    xcb_configure_request_event_t const *);
static void			event_destroy_notify(
    xcb_destroy_notify_event_t const *);
static void			event_enter_notify(xcb_enter_notify_event_t
    const *);
static void			event_expose(xcb_expose_event_t const *);
static void			event_handle(xcb_generic_event_t const *);
static void			event_key_press(xcb_key_press_event_t const
    *);
static void			event_map_request(xcb_map_request_event_t
    const *);
static void			event_property_notify(
    xcb_property_notify_event_t const *);
static void			event_unmap_notify(xcb_unmap_notify_event_t
    const *);
static void			my_exit(void);
static void			randr_update(void);
static void			root_name_update(void);
static void			string_convert(struct String *, char const *,
    size_t);
static int			text_draw(struct String const *, int, int,
    int, int);
static int			text_width(struct String const *);
static void			view_clear(void);
static struct View const	*view_find(int, int) FUNC_RETURNS;

#include "config.h"

typedef void (*EventHandler)(xcb_generic_event_t const *);

static struct String *g_workspace_label;
static uint32_t g_values[3];
static EventHandler g_event_handler[30];
static xcb_connection_t *g_conn;
static xcb_screen_t *g_screen;
static iconv_t g_iconv = (iconv_t)-1;
static xcb_key_symbols_t *g_key_symbols;
static xcb_drawable_t g_root;
static struct String g_root_name;
static int g_is_root_urgent;
static xcb_gc_t g_gc;
static xcb_pixmap_t g_pixmap;
static xcb_font_t g_font;
static int g_font_ascent, g_font_height;
static xcb_atom_t g_WM_DELETE_WINDOW, g_WM_PROTOCOLS;
static xcb_atom_t g_NET_WM_NAME;
static xcb_cursor_t g_cursor_normal, g_cursor_move, g_cursor_resize;
static xcb_window_t g_bar = XCB_NONE;
static int g_do_bar_redraw;
static uint32_t g_color_border_focus, g_color_border_unfocus;
static uint32_t g_color_bar_bg, g_color_bar_fg;
static uint32_t g_color_urgent1_bg, g_color_urgent1_fg;
static uint32_t g_color_urgent2_bg, g_color_urgent2_fg;
static struct ViewList g_view_list = TAILQ_HEAD_INITIALIZER(g_view_list);
static struct ClientList *g_client_list;
static size_t g_workspace_cur;
static struct Client *g_focus;
static enum RunControl g_run;
static int g_has_urgent, g_timeout, g_blink;
static double g_time_prev;
static xcb_window_t g_button_press_window;
static int g_button_press_x, g_button_press_y;
static uint8_t g_randr_evbase;

void
action_client_browse(struct Arg const *a_arg)
{
	struct Client *c, *prev;
	int do_browse;

	(void)a_arg;
	if (TAILQ_EMPTY(&g_client_list[g_workspace_cur]) || !(c =
	    TAILQ_NEXT(TAILQ_FIRST(&g_client_list[g_workspace_cur]), next))) {
		return;
	}
	prev = NULL;
	xcb_grab_keyboard(g_conn, 0, g_root, XCB_CURRENT_TIME,
	    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	for (do_browse = 1; do_browse;) {
		xcb_keysym_t keysym;
		xcb_generic_event_t *event;
		xcb_key_press_event_t const *kp;

		if (c != prev) {
			client_focus(c, 0, 1, 1);
			bar_draw();
			xcb_flush(g_conn);
			prev = c;
		}
		if (!(event = xcb_wait_for_event(g_conn))) {
			continue;
		}
		kp = (xcb_key_press_event_t const *)event;
		keysym = xcb_key_symbols_get_keysym(g_key_symbols, kp->detail,
		    0);
		switch (XCB_EVENT_RESPONSE_TYPE(event)) {
		case XCB_KEY_PRESS:
			if (XK_Tab == keysym) {
				if (!(c = TAILQ_NEXT(c, next))) {
					c = TAILQ_FIRST(&g_client_list[
					    g_workspace_cur]);
				}
			}
			break;
		case XCB_KEY_RELEASE:
			do_browse = XK_Super_L != keysym && XK_Super_R !=
			    keysym;
			break;
		case XCB_EXPOSE:
			/* FALLTHROUGH */
		case XCB_MAP_REQUEST:
			/* FALLTHROUGH */
		case XCB_CONFIGURE_REQUEST:
			event_handle(event);
			break;
		}
		free(event);
	}
	xcb_ungrab_keyboard(g_conn, XCB_CURRENT_TIME);
	client_focus(c, 1, 0, 0);
}

void
action_client_expand(struct Arg const *a_arg)
{
	struct View const *view;
	struct Client *c;
	int new, test;

	if (!g_focus) {
		return;
	}
	view = view_find(g_focus->x, g_focus->y);
	switch (a_arg->i) {
	case DIR_EAST: new = VIEW_RIGHT(view) - g_focus->x - 2; break;
	case DIR_NORTH: return;
	case DIR_WEST: return;
	case DIR_SOUTH: new = VIEW_BOTTOM(view) - g_focus->y - 2; break;
	default: abort();
	}
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		if (g_focus == c) {
			continue;
		}
		switch (a_arg->i) {
		case DIR_EAST:
			test = c->x - g_focus->x - 2;
			CONVERGE(<, WIDTH_B(g_focus), test, new);
			break;
		case DIR_NORTH:
			/* FALLTHROUGH */
		case DIR_WEST:
			break;
		case DIR_SOUTH:
			test = c->y - g_focus->y - 2;
			CONVERGE(<, HEIGHT_B(g_focus), test, new);
			break;
		}
	}
	(DIR_EAST == a_arg->i || DIR_WEST == a_arg->i) ? (g_focus->width =
	    new) : (g_focus->height = new);
	client_resize(g_focus, 0);
}

void
action_client_grow(struct Arg const *a_arg)
{
	int incw = 1, inch = 1;

	if (!g_focus) {
		return;
	}
	if (XCB_ICCCM_SIZE_HINT_P_RESIZE_INC & g_focus->hints.flags) {
		incw = g_focus->hints.width_inc;
		inch = g_focus->hints.height_inc;
	}
	switch (a_arg->i) {
	case DIR_EAST: g_focus->width += incw; break;
	case DIR_NORTH: g_focus->height -= inch; break;
	case DIR_WEST: g_focus->width -= incw; break;
	case DIR_SOUTH: g_focus->height += inch; break;
	default: abort();
	}
	client_resize(g_focus, 0);
}

void
action_client_jump(struct Arg const *a_arg)
{
	struct View const *view;
	struct Client *c;
	int new, test;

	if (!g_focus) {
		return;
	}
	view = view_find(g_focus->x, g_focus->y);
	switch (a_arg->i) {
	case DIR_EAST: new = VIEW_RIGHT(view) - WIDTH_B(g_focus); break;
	case DIR_NORTH: new = g_font_height; break;
	case DIR_WEST: new = 0; break;
	case DIR_SOUTH: new = VIEW_BOTTOM(view) - HEIGHT_B(g_focus); break;
	default: abort();
	}
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		if (g_focus == c) {
			continue;
		}
		switch (a_arg->i) {
		case DIR_EAST:
			test = c->x - WIDTH_B(g_focus);
			CONVERGE(<, g_focus->x, test, new);
			break;
		case DIR_NORTH:
			test = c->y + HEIGHT_B(c);
			CONVERGE(>, g_focus->y, test, new);
			break;
		case DIR_WEST:
			test = c->x + WIDTH_B(c);
			CONVERGE(>, g_focus->x, test, new);
			break;
		case DIR_SOUTH:
			test = c->y - HEIGHT_B(g_focus);
			CONVERGE(<, g_focus->y, test, new);
			break;
		}
	}
	(DIR_EAST == a_arg->i || DIR_WEST == a_arg->i) ? (g_focus->x = new) :
	    (g_focus->y = new);
	client_move(g_focus, VISIBLE);
	xcb_warp_pointer(g_conn, XCB_NONE, g_focus->window, 0, 0, 0, 0,
	    g_focus->width / 2, g_focus->height / 2);
}

void
action_client_maximize(struct Arg const *a_arg)
{
	struct View const *view;
	enum Maximize const c_toggle = a_arg->i;
	enum Maximize want;

	if (!g_focus) {
		return;
	}
	view = view_find(g_focus->x, g_focus->y);
	switch (g_focus->maximize) {
	case MAX_NOPE:
		g_focus->max_old_x = g_focus->x;
		g_focus->max_old_y = g_focus->y;
		g_focus->max_old_width = g_focus->width;
		g_focus->max_old_height = g_focus->height;
		want = c_toggle;
		break;
	case MAX_BOTH:
		want = MAX_BOTH == c_toggle ? MAX_NOPE : MAX_VERT;
		break;
	case MAX_VERT:
		g_focus->max_old_x = g_focus->x;
		want = MAX_VERT == c_toggle ? MAX_NOPE : MAX_BOTH;
		break;
	default:
		abort();
	}
	switch (want) {
	case MAX_NOPE:
		g_focus->x = g_focus->max_old_x;
		g_focus->y = g_focus->max_old_y;
		g_focus->width = g_focus->max_old_width;
		g_focus->height = g_focus->max_old_height;
		break;
	case MAX_BOTH:
		g_focus->x = 0;
		g_focus->y = g_font_height;
		g_focus->width = view->width - 2 * g_focus->border_width;
		g_focus->height = view->height - g_font_height - 2 *
		    g_focus->border_width;
		break;
	case MAX_VERT:
		g_focus->x = g_focus->max_old_x;
		g_focus->y = g_font_height;
		g_focus->width = g_focus->max_old_width;
		g_focus->height = view->height - g_font_height - 2 *
		    g_focus->border_width;
		break;
	}
	g_focus->maximize = want;
	client_move(g_focus, VISIBLE);
	client_resize(g_focus, 0);
}

void
action_client_move(struct Arg const *a_arg)
{
	xcb_generic_event_t *event;
	int dx, dy, do_move;

	(void)a_arg;
	if (!g_focus) {
		return;
	}
	dx = g_focus->x - g_button_press_x;
	dy = g_focus->y - g_button_press_y;
	xcb_grab_pointer(g_conn, 0, g_focus->window,
	    XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
	    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE, g_cursor_move,
	    XCB_CURRENT_TIME);
	xcb_flush(g_conn);
	for (do_move = 1; do_move;) {
		xcb_motion_notify_event_t *motion;

		if (!(event = xcb_wait_for_event(g_conn))) {
			continue;
		}
		switch (XCB_EVENT_RESPONSE_TYPE(event)) {
		case XCB_BUTTON_RELEASE:
			do_move = 0;
			break;
		case XCB_MOTION_NOTIFY:
			motion = (xcb_motion_notify_event_t *)event;
			g_focus->x = motion->root_x + dx;
			g_focus->y = motion->root_y + dy;
			client_snap_position(g_focus);
			client_move(g_focus, VISIBLE);
			xcb_flush(g_conn);
			break;
		case XCB_EXPOSE:
			/* FALLTHROUGH */
		case XCB_MAP_REQUEST:
			/* FALLTHROUGH */
		case XCB_CONFIGURE_REQUEST:
			event_handle(event);
			break;
	}
		free(event);
	}
	xcb_ungrab_pointer(g_conn, XCB_CURRENT_TIME);
	xcb_set_input_focus(g_conn, XCB_INPUT_FOCUS_POINTER_ROOT,
	    g_focus->window, XCB_CURRENT_TIME);
}

void
action_client_relocate(struct Arg const *a_arg)
{
	assert(0 <= a_arg->i && (int)LENGTH(c_workspace_label) > a_arg->i);
	if (!g_focus || (int)g_workspace_cur == a_arg->i) {
		return;
	}
	TAILQ_REMOVE(&g_client_list[g_workspace_cur], g_focus, next);
	TAILQ_INSERT_HEAD(&g_client_list[a_arg->i], g_focus, next);
	client_move(g_focus, HIDDEN);
	client_focus(NULL, 1, 0, 0);
}

void
action_client_resize(struct Arg const *a_arg)
{
	xcb_generic_event_t *event;
	int dx, dy, do_resize;

	(void)a_arg;
	if (!g_focus) {
		return;
	}
	dx = g_focus->width - g_button_press_x;
	dy = g_focus->height - g_button_press_y;
	xcb_grab_pointer(g_conn, 0, g_focus->window,
	    XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION,
	    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC, XCB_NONE,
	    g_cursor_resize, XCB_CURRENT_TIME);
	xcb_flush(g_conn);
	for (do_resize = 1; do_resize;) {
		xcb_motion_notify_event_t *motion;

		if (!(event = xcb_wait_for_event(g_conn))) {
			continue;
		}
		switch (XCB_EVENT_RESPONSE_TYPE(event)) {
		case XCB_BUTTON_RELEASE:
			do_resize = 0;
			break;
		case XCB_MOTION_NOTIFY:
			motion = (xcb_motion_notify_event_t *)event;
			g_focus->width = motion->root_x + dx;
			g_focus->height = motion->root_y + dy;
			client_snap_dimension(g_focus);
			client_resize(g_focus, 1);
			xcb_flush(g_conn);
			break;
		case XCB_EXPOSE:
			/* FALLTHROUGH */
		case XCB_MAP_REQUEST:
			/* FALLTHROUGH */
		case XCB_CONFIGURE_REQUEST:
			event_handle(event);
			break;
		}
		free(event);
	}
	xcb_ungrab_pointer(g_conn, XCB_CURRENT_TIME);
	xcb_set_input_focus(g_conn, XCB_INPUT_FOCUS_PARENT, g_focus->window,
	    XCB_CURRENT_TIME);
}

void
action_exec(struct Arg const *a_arg)
{
	pid_t pid;
	char const *arg0 = ((char const **)a_arg->v)[0];

	pid = fork();
	if (0 > pid) {
		warn("Could not fork for '%s'", arg0);
	} else if (0 == pid) {
		execvp(arg0, a_arg->v);
		warn("Failed to exec '%s'", arg0);
		_exit(1);
	}
}

void
action_furnish(struct Arg const *a_arg)
{
	struct Client *c, *prev_focus;
	int num = 0;

	(void)a_arg;
	prev_focus = g_focus;
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		c->x += 100000;
		++num;
	}
	while (0 < num--) {
		struct Client *best_client = NULL;
		int best_size = 0, best_ofs = 0;

		TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
			int const c_size = c->width * c->height;
			int const c_ofs = c->x + 10000 * c->y;

			if (10000 > c->x) {
				continue;
			}
			if (c_size > best_size ||
			    (c_size == best_size && c_ofs < best_ofs)) {
				best_client = c;
				best_size = c_size;
				best_ofs = c_ofs;
			}
		}
		client_place(best_client);
	}
	client_focus(prev_focus, 0, 0, 1);
}

void
action_kill(struct Arg const *a_arg)
{
	xcb_icccm_get_wm_protocols_reply_t proto;
	size_t i;
	int has_delete = 0;

	(void)a_arg;
	if (!g_focus) {
		return;
	}
	ZERO(proto);
	xcb_icccm_get_wm_protocols_reply(g_conn,
	    xcb_icccm_get_wm_protocols(g_conn, g_focus->window,
	    g_WM_PROTOCOLS), &proto, NULL);
	for (i = 0; proto.atoms_len > i; ++i) {
		has_delete |= g_WM_DELETE_WINDOW == proto.atoms[i];
	}
	xcb_icccm_get_wm_protocols_reply_wipe(&proto);
	if (has_delete) {
		xcb_client_message_event_t ev;

		ev.response_type = XCB_CLIENT_MESSAGE;
		ev.format = 32;
		ev.sequence = 0;
		ev.window = g_focus->window;
		ev.type = g_WM_PROTOCOLS;
		ev.data.data32[0] = g_WM_DELETE_WINDOW;
		ev.data.data32[1] = XCB_CURRENT_TIME;
		xcb_send_event(g_conn, 0, g_focus->window,
		    XCB_EVENT_MASK_NO_EVENT, (char *)&ev);
	} else {
		xcb_kill_client(g_conn, g_focus->window);
	}
}

void
action_quit(struct Arg const *a_arg)
{
	g_run = a_arg->i;
}

void
action_workspace_select(struct Arg const *a_arg)
{
	struct Client *c;

	assert(0 <= a_arg->i && (int)LENGTH(c_workspace_label) > a_arg->i);
	if ((int)g_workspace_cur == a_arg->i) {
		return;
	}
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		client_move(c, HIDDEN);
	}
	g_workspace_cur = a_arg->i;
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		client_move(c, VISIBLE);
	}
	client_focus(NULL, 1, 0, 0);
}

xcb_atom_t
atom_get(char const *a_name)
{
	xcb_intern_atom_reply_t *reply;
	xcb_atom_t atom;

	if (!(reply = xcb_intern_atom_reply(g_conn, xcb_intern_atom(g_conn, 1,
	    strlen(a_name), a_name), NULL))) {
		errx(EXIT_FAILURE, "Could not get atom '%s'.", a_name);
	}
	atom = reply->atom;
	free(reply);
	return atom;
}

void
bar_draw()
{
	xcb_rectangle_t rect;
	struct View const *view;
	struct Client *c;
	size_t i;
	int x;

	view = TAILQ_FIRST(&g_view_list);
	rect.x = 0;
	rect.y = 0;
	rect.width = view->width;
	rect.height = g_font_height;
	xcb_change_gc(g_conn, g_gc, XCB_GC_FOREGROUND, &g_color_bar_bg);
	xcb_poly_fill_rectangle(g_conn, g_pixmap, g_gc, 1, &rect);

	g_has_urgent = g_is_root_urgent;
	for (x = i = 0; LENGTH(c_workspace_label) > i; ++i) {
		int is_urgent = 0;

		TAILQ_FOREACH(c, &g_client_list[i], next) {
			g_has_urgent |= is_urgent |= c->is_urgent;
		}
		x += text_draw(&g_workspace_label[i], g_workspace_cur == i,
		    is_urgent, x, 0);
		if (!TAILQ_EMPTY(&g_client_list[i])) {
			rect.x = x - 4;
			rect.width = 3;
			rect.height = 3;
			xcb_poly_rectangle(g_conn, g_pixmap, g_gc, 1, &rect);
		}
	}
	text_draw(&g_root_name, 0, g_is_root_urgent, VIEW_RIGHT(view) -
	    text_width(&g_root_name), 0);
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		x += text_draw(&c->name, g_focus == c, c->is_urgent, x, 0);
	}

	xcb_copy_area(g_conn, g_pixmap, g_bar, g_gc, 0, 0, 0, 0,
	    VIEW_RIGHT(view), g_font_height);
}

void
bar_reset()
{
	struct View const *view;

	if (XCB_NONE != g_bar) {
		xcb_destroy_window(g_conn, g_bar);
		xcb_free_pixmap(g_conn, g_pixmap);
	}

	g_pixmap = xcb_generate_id(g_conn);
	view = TAILQ_FIRST(&g_view_list);
	xcb_create_pixmap(g_conn, g_screen->root_depth, g_pixmap, g_root,
	    view->width, g_font_height);

	g_bar = xcb_generate_id(g_conn);
	g_values[0] = g_color_bar_bg;
	g_values[1] = 1;
	g_values[2] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_EXPOSURE;
	xcb_create_window(g_conn, XCB_COPY_FROM_PARENT, g_bar, g_root,
	    view->x, view->y, view->width, g_font_height, 0,
	    XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
	    XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK,
	    g_values);
	xcb_map_window(g_conn, g_bar);
}

void
button_grab(struct Client *a_client)
{
	struct ButtonBind const *bind;
	size_t i;

	for (i = 0, bind = c_button_bind; LENGTH(c_button_bind) > i; ++i,
	    ++bind) {
		if (CLICK_CLIENT == bind->click) {
			xcb_grab_button(g_conn, 0, a_client->window,
			    XCB_EVENT_MASK_BUTTON_PRESS, XCB_GRAB_MODE_ASYNC,
			    XCB_GRAB_MODE_ASYNC, XCB_NONE, XCB_NONE,
			    bind->code, bind->state);
		}
	}
}

struct Client *
client_add(xcb_window_t a_window, xcb_window_t a_parent)
{
	int data = -1;

	return client_add_details(a_window, a_parent, &data);
}

struct Client *
client_add_details(xcb_window_t a_window, xcb_window_t a_parent, int const
    *a_data)
{
	xcb_get_window_attributes_reply_t *attr;
	xcb_get_geometry_reply_t *geom;
	xcb_query_pointer_reply_t *query;
	struct View const *view;
	struct Client *c;
	size_t workspace;

	if (a_window == g_root) {
		return NULL;
	}
	if ((attr = xcb_get_window_attributes_reply(g_conn,
	    xcb_get_window_attributes(g_conn, a_window), NULL))) {
		if (XCB_MAP_STATE_VIEWABLE != attr->map_state ||
		    attr->override_redirect) {
			free(attr);
			return NULL;
		}
		free(attr);
	}

	if (!(geom = xcb_get_geometry_reply(g_conn,
	    xcb_get_geometry(g_conn, a_window), NULL))) {
		fprintf(stderr, "client_add_details: Could not get geometry "
		    "of window 0x%08x.\n", a_window);
		return NULL;
	}

	CALLOC(c, 1);
	c->window = a_window;
	c->hints.flags = 0;
	xcb_icccm_get_wm_normal_hints_reply(g_conn,
	    xcb_icccm_get_wm_normal_hints(g_conn, a_window), &c->hints, NULL);
	client_name_update(c);
	query = xcb_query_pointer_reply(g_conn, xcb_query_pointer(g_conn,
	    g_root), NULL);
	view = view_find(query->root_x, query->root_y);
	free(query);
	c->x = view->x;
	c->y = view->y;
	c->width = geom->width;
	c->height = geom->height;
	free(geom);
	if (0 > a_data[0]) {
		if (XCB_WINDOW_NONE != a_parent) {
			/* TODO: Find parent workspace. */
			for (workspace = 0; LENGTH(c_workspace_label) >
			    workspace; ++workspace) {
				struct Client *rover;

				TAILQ_FOREACH(rover, &g_client_list[
				    workspace], next) {
					if (rover->window == a_parent) {
						goto found_parent;
					}
				}
			}
			workspace = g_workspace_cur;
			/*
			 * Parent is probably overriding, such megalomania
			 * will result in "unnatural" behaviour.
			 */
		} else {
			workspace = g_workspace_cur;
		}
found_parent:
		g_do_bar_redraw = c->is_urgent = g_workspace_cur != workspace;
		c->maximize = MAX_NOPE;
		c->border_width = c->width != view->width || c->height !=
		    view->height;
		c->max_old_y = c->max_old_x = 0;
		c->max_old_height = c->max_old_width = 0;
		client_place(c);
	} else {
		workspace = a_data[0];
		g_do_bar_redraw = c->is_urgent = a_data[1];
		c->x = a_data[2];
		c->y = a_data[3];
		c->maximize = a_data[4];
		c->border_width = a_data[5];
		c->max_old_x = a_data[6];
		c->max_old_y = a_data[7];
		c->max_old_width = a_data[8];
		c->max_old_height = a_data[9];
	}
	TAILQ_INSERT_HEAD(&g_client_list[workspace], c, next);

	button_grab(c);

	g_values[0] = g_color_border_unfocus;
	g_values[1] = XCB_EVENT_MASK_ENTER_WINDOW |
	    XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_change_window_attributes(g_conn, a_window, XCB_CW_BORDER_PIXEL |
	    XCB_CW_EVENT_MASK, g_values);
	g_values[0] = c->border_width;
	xcb_configure_window(g_conn, a_window, XCB_CONFIG_WINDOW_BORDER_WIDTH,
	    g_values);

	if (g_workspace_cur != workspace) {
		client_move(c, HIDDEN);
	}
	return c;
}

void
client_free(struct Client **a_client)
{
	struct Client *c;

	c = *a_client;
	if (!c) {
		return;
	}
	TAILQ_REMOVE(&g_client_list[g_workspace_cur], c, next);
	free(c);
	if (c == g_focus) {
		client_focus(g_focus = NULL, 1, 0, 0);
	}
	*a_client = NULL;
}

void
client_focus(struct Client *a_client, int a_do_reorder, int const a_do_raise,
    int a_do_warp)
{
	xcb_icccm_wm_hints_t hints;

	g_do_bar_redraw = 1;
	if (g_focus) {
		g_values[0] = g_color_border_unfocus;
		xcb_change_window_attributes(g_conn, g_focus->window,
		    XCB_CW_BORDER_PIXEL, g_values);
	}
	g_focus = a_client ? a_client :
	    TAILQ_FIRST(&g_client_list[g_workspace_cur]);
	if (!g_focus) {
		return;
	}

	if (xcb_icccm_get_wm_hints_reply(g_conn,
	    xcb_icccm_get_wm_hints(g_conn, g_focus->window), &hints, NULL)) {
		hints.flags &= ~XCB_ICCCM_WM_HINT_X_URGENCY;
		xcb_icccm_set_wm_hints(g_conn, g_focus->window, &hints);
	}
	g_focus->is_urgent = 0;
	if (a_do_reorder) {
		TAILQ_REMOVE(&g_client_list[g_workspace_cur], g_focus, next);
		TAILQ_INSERT_HEAD(&g_client_list[g_workspace_cur], g_focus,
		    next);
	}
	if (a_do_raise) {
		g_values[0] = XCB_STACK_MODE_TOP_IF;
		xcb_configure_window(g_conn, g_focus->window,
		    XCB_CONFIG_WINDOW_STACK_MODE, g_values);
	}
	if (a_do_warp) {
		xcb_warp_pointer(g_conn, XCB_NONE, g_focus->window, 0, 0, 0,
		    0, g_focus->width / 2, g_focus->height / 2);
	}
	g_values[0] = g_color_border_focus;
	xcb_change_window_attributes(g_conn, g_focus->window,
	    XCB_CW_BORDER_PIXEL, g_values);
	xcb_set_input_focus(g_conn, XCB_INPUT_FOCUS_POINTER_ROOT,
	    g_focus->window, XCB_CURRENT_TIME);
}

struct Client *
client_get(xcb_window_t a_window, size_t *a_workspace)
{
	struct Client *c;
	size_t i;

	for (i = 0; LENGTH(c_workspace_label) > i; ++i) {
		TAILQ_FOREACH(c, &g_client_list[i], next) {
			if (a_window == c->window) {
				a_workspace ? *a_workspace = i : 0;
				return c;
			}
		}
	}
	return NULL;
}

void
client_move(struct Client *a_client, enum Visibility a_visibility)
{
	if (VISIBLE == a_visibility) {
		g_values[0] = a_client->x;
		g_values[1] = a_client->y;
	} else {
		g_values[0] = 10000;
		g_values[1] = 0;
	}
	xcb_configure_window(g_conn, a_client->window, XCB_CONFIG_WINDOW_X |
	    XCB_CONFIG_WINDOW_Y, g_values);
}

void
client_name_update(struct Client *a_client)
{
	xcb_icccm_get_text_property_reply_t icccm;
	xcb_get_property_reply_t *reply;

	if ((reply = xcb_get_property_reply(g_conn, xcb_get_property(g_conn,
	    0, a_client->window, g_NET_WM_NAME, XCB_GET_PROPERTY_TYPE_ANY, 0,
	    UINT32_MAX), NULL))) {
		string_convert(&a_client->name, xcb_get_property_value(reply),
		    xcb_get_property_value_length(reply));
		free(reply);
		if (a_client->name.length) {
			return;
		}
	}
	if (xcb_icccm_get_wm_name_reply(g_conn, xcb_icccm_get_wm_name(g_conn,
	    a_client->window), &icccm, NULL)) {
		string_convert(&a_client->name, icccm.name, icccm.name_len);
		xcb_icccm_get_text_property_reply_wipe(&icccm);
		return;
	}
	string_convert(&a_client->name, "<noname>", 8);
}

void
client_place(struct Client *a_client)
{
	struct View const *view;
	int best_score = 1e9, best_x = 0, best_y = g_font_height, x, y =
	    g_font_height - 1;

	view = view_find(a_client->x, a_client->y);
	x = VIEW_RIGHT(view) - WIDTH_B(a_client);
	for (;;) {
		struct Client const *sibling;
		int score = 0, new_x, new_y, test;

		if (x + WIDTH_B(a_client) == VIEW_RIGHT(view)) {
			int height_b = HEIGHT_B(a_client);

			if (y + height_b == VIEW_BOTTOM(view)) {
				break;
			}
			new_y = VIEW_BOTTOM(view) - height_b;
			TAILQ_FOREACH(sibling,
			    &g_client_list[g_workspace_cur], next) {
				if (sibling != a_client) {
					test = sibling->y - height_b;
					CONVERGE(<, y, test, new_y);
					test = sibling->y;
					CONVERGE(<, y, test, new_y);
					test = sibling->y + HEIGHT_B(sibling)
					    - height_b;
					CONVERGE(<, y, test, new_y);
					test = sibling->y + HEIGHT_B(sibling);
					CONVERGE(<, y, test, new_y);
				}
			}
			x = VIEW_LEFT(view) - 1;
			test = g_font_height;
			if (y < test && test < new_y) {
				new_y = test;
			}
			y = new_y;
		}
		new_x = VIEW_RIGHT(view) - WIDTH_B(a_client);
		TAILQ_FOREACH(sibling, &g_client_list[g_workspace_cur], next)
		{
			if (sibling != a_client) {
				test = sibling->x - WIDTH_B(a_client);
				CONVERGE(<, x, test, new_x);
				test = sibling->x;
				CONVERGE(<, x, test, new_x);
				test = sibling->x + WIDTH_B(sibling) -
				    WIDTH_B(a_client);
				CONVERGE(<, x, test, new_x);
				test = sibling->x + WIDTH_B(sibling);
				CONVERGE(<, x, test, new_x);
			}
		}
		test = 0;
		if (x < test && test < new_x) {
			new_x = test;
		}
		x = new_x;

		TAILQ_FOREACH(sibling, &g_client_list[g_workspace_cur], next)
		{
			if (sibling != a_client) {
				int x0 = MAX(x, sibling->x);
				int y0 = MAX(y, sibling->y);
				int x1 = MIN(x + WIDTH_B(a_client),
				    sibling->x + WIDTH_B(sibling));
				int y1 = MIN(y + HEIGHT_B(a_client),
				    sibling->y + HEIGHT_B(sibling));
				int s = MAX(0, x1 - x0) * MAX(0, y1 - y0);

				score += s;
			}
		}
		if (best_score > score) {
			best_score = score;
			best_x = x;
			best_y = y;
		}
	}

	a_client->x = MIN(best_x, VIEW_RIGHT(view) - WIDTH_B(a_client));
	a_client->y = MIN(best_y, VIEW_BOTTOM(view) - HEIGHT_B(a_client));
	client_move(a_client, VISIBLE);
}

void
client_resize(struct Client *a_client, int a_do_round)
{
	xcb_size_hints_t const *hints = &a_client->hints;
	int base_width = 0, base_height = 0;

	if (XCB_ICCCM_SIZE_HINT_BASE_SIZE & hints->flags) {
		base_width = hints->base_width;
		base_height = hints->base_height;
	}
	if (XCB_ICCCM_SIZE_HINT_P_RESIZE_INC & hints->flags) {
		int wi = hints->width_inc;
		int hi = hints->height_inc;
		int f = a_do_round ? 1 : 0;

		a_client->width = ((a_client->width - base_width + f * wi / 2)
		    / wi) * wi + base_width;
		a_client->height = ((a_client->height - base_height + f * hi /
		    2) / hi) * hi + base_height;
	}
	if (XCB_ICCCM_SIZE_HINT_P_ASPECT & hints->flags) {
printf("Aspect = %d:%d .. %d:%d\n",
    hints->min_aspect_num, hints->min_aspect_den,
    hints->max_aspect_num, hints->max_aspect_den);
fflush(stdout);
	}
	if (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE & hints->flags) {
		a_client->width = MAX(a_client->width, hints->min_width);
		a_client->height = MAX(a_client->height, hints->min_height);
	}
	if (XCB_ICCCM_SIZE_HINT_P_MAX_SIZE & hints->flags) {
		a_client->width = MIN(a_client->width, hints->max_width);
		a_client->height = MIN(a_client->height, hints->max_height);
	}
	g_values[0] = a_client->width;
	g_values[1] = a_client->height;
	xcb_configure_window(g_conn, a_client->window, XCB_CONFIG_WINDOW_WIDTH
	    | XCB_CONFIG_WINDOW_HEIGHT, g_values);
}

void
client_snap_dimension(struct Client *a_client)
{
	struct View const *view;
	struct Client const *sibling;
	int ref;

	view = view_find(a_client->x, a_client->y);
	TAILQ_FOREACH(sibling, &g_client_list[g_workspace_cur], next) {
		if (a_client == sibling) {
			continue;
		}
		ref = sibling->x - a_client->x - 2;
		SNAP(<, ref, a_client->width, ref + c_snap_margin);
		ref = sibling->y - a_client->y - 2;
		SNAP(<, ref, a_client->height, ref + c_snap_margin);
	}
	ref = VIEW_RIGHT(view) - a_client->x - 2;
	SNAP(<, ref, a_client->width, ref + c_snap_margin);
	ref = VIEW_BOTTOM(view) - a_client->y - 2;
	SNAP(<, ref, a_client->height, ref + c_snap_margin);
}

void
client_snap_position(struct Client *a_client)
{
	struct View const *view;
	struct Client const *sibling;
	int ref;

	view = view_find(a_client->x, a_client->y);
	TAILQ_FOREACH(sibling, &g_client_list[g_workspace_cur], next) {
		if (a_client == sibling) {
			continue;
		}
		ref = sibling->x - WIDTH_B(a_client);
		SNAP(<, ref, a_client->x, ref + c_snap_margin);
		ref = sibling->x + WIDTH_B(sibling);
		SNAP(>, ref, a_client->x, ref - c_snap_margin);
		ref = sibling->y - HEIGHT_B(a_client);
		SNAP(<, ref, a_client->y, ref + c_snap_margin);
		ref = sibling->y + HEIGHT_B(sibling);
		SNAP(>, ref, a_client->y, ref - c_snap_margin);
	}
	ref = VIEW_RIGHT(view) - WIDTH_B(a_client);
	SNAP(<, ref, a_client->x, ref + c_snap_margin);
	ref = 0;
	SNAP(>, ref, a_client->x, ref - c_snap_margin);

	ref = VIEW_BOTTOM(view) - HEIGHT_B(a_client);
	SNAP(<, ref, a_client->y, ref + c_snap_margin);
	ref = g_font_height;
	SNAP(>, ref, a_client->y, ref - c_snap_margin);
}

uint32_t
color_get(char const *a_name)
{
	xcb_alloc_named_color_reply_t *color;
	uint32_t pixel;

	color = xcb_alloc_named_color_reply(g_conn,
	    xcb_alloc_named_color(g_conn, g_screen->default_colormap,
	    strlen(a_name), a_name), NULL);
	pixel = color->pixel;
	free(color);
	return pixel;
}

xcb_cursor_t
cursor_get(xcb_font_t a_font, int a_index)
{
	xcb_cursor_t cursor;

	cursor = xcb_generate_id(g_conn);
	xcb_create_glyph_cursor(g_conn, cursor, a_font, a_font, a_index,
	    a_index + 1, 0, 0, 0, 0xffff, 0xffff, 0xffff);
	return cursor;
}

void
event_button_press(xcb_button_press_event_t const *a_event)
{
	struct Arg arg;
	struct ButtonBind const *bind;
	struct Client *c;
	enum Click click = CLICK_ROOT;
	size_t i;

	if (a_event->event == g_bar) {
		struct View const *view;
		int x = 0;

		view = view_find(a_event->root_x, a_event->root_y);
		i = 0;
		do {
			x += text_width(&g_workspace_label[i]);
		} while (x <= a_event->root_x && LENGTH(c_workspace_label) > ++i);
		if (LENGTH(c_workspace_label) > i) {
			click = CLICK_WORKSPACE;
			arg.i = i;
		} else if (VIEW_RIGHT(view) - text_width(&g_root_name) <=
		    a_event->root_x) {
			click = CLICK_STATUS;
		}
	} else if ((c = client_get(a_event->event, NULL))) {
		click = CLICK_CLIENT;
		client_focus(c, 1, 1, 0);
	}
	g_button_press_window = a_event->event;
	g_button_press_x = a_event->root_x;
	g_button_press_y = a_event->root_y;
	for (i = 0, bind = c_button_bind; LENGTH(c_button_bind) > i; ++i,
	    ++bind) {
		if (bind->click == click && bind->code == a_event->detail &&
		    bind->state == a_event->state) {
			bind->action(&arg);
		}
	}
}

void
event_configure_notify(xcb_configure_notify_event_t const *a_event)
{
	struct Client *c;

	c = client_get(a_event->window, NULL);
	if (!c && !a_event->override_redirect) {
		c = client_add(a_event->window, XCB_WINDOW_NONE);
	} else if (c && a_event->override_redirect) {
		client_free(&c);
	}
}

void
event_configure_request(xcb_configure_request_event_t const *a_event)
{
	struct Client *c;

	if ((c = client_get(a_event->window, NULL))) {
		if (XCB_CONFIG_WINDOW_WIDTH & a_event->value_mask) {
			c->width = a_event->width;
		}
		if (XCB_CONFIG_WINDOW_HEIGHT & a_event->value_mask) {
			c->height = a_event->height;
		}
		if ((XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT) &
		    a_event->value_mask) {
			client_resize(c, 0);
		}
	} else {
		uint32_t *p;
		uint16_t mask;

		p = g_values;
		mask = (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
		    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT) &
		    a_event->value_mask;
		if (XCB_CONFIG_WINDOW_X & mask) {
			*p++ = a_event->x;
		}
		if (XCB_CONFIG_WINDOW_Y & mask) {
			*p++ = MAX(a_event->y, g_font_height);
		}
		if (XCB_CONFIG_WINDOW_WIDTH & mask) {
			*p++ = a_event->width;
		}
		if (XCB_CONFIG_WINDOW_HEIGHT & mask) {
			*p++ = a_event->height;
		}
		xcb_configure_window(g_conn, a_event->window, mask, g_values);
	}
}

void
event_destroy_notify(xcb_destroy_notify_event_t const *a_event)
{
	struct Client *c;

	c = client_get(a_event->window, NULL);
	client_free(&c);
}

void
event_enter_notify(xcb_enter_notify_event_t const *a_event)
{
	struct Client *c;
	size_t workspace;

	if ((c = client_get(a_event->event, &workspace)) &&
	    g_workspace_cur == workspace) {
		client_focus(c, 1, 0, 0);
	}
}

void
event_expose(xcb_expose_event_t const *a_event)
{
	if (0 == a_event->count && a_event->window == g_bar) {
		g_do_bar_redraw = 1;
	}
}

void
event_handle(xcb_generic_event_t const *a_event)
{
	uint8_t const c_i = XCB_EVENT_RESPONSE_TYPE(a_event);

	if (c_i == g_randr_evbase + XCB_RANDR_SCREEN_CHANGE_NOTIFY) {
		randr_update();
	} else if (LENGTH(g_event_handler) > c_i && g_event_handler[c_i]) {
		g_event_handler[c_i](a_event);
	}
}

void
event_key_press(xcb_key_press_event_t const *a_event)
{
	xcb_keysym_t keysym;
	struct KeyBind const *bind;
	size_t i;

	keysym = xcb_key_symbols_get_keysym(g_key_symbols, a_event->detail,
	    0);
	for (i = 0, bind = c_key_bind; LENGTH(c_key_bind) > i; ++i, ++bind) {
		if (bind->keysym == keysym && bind->state == a_event->state) {
			bind->action(&bind->arg);
		}
	}
}

void
event_map_request(xcb_map_request_event_t const *a_event)
{
	struct Client *c;

	xcb_map_window(g_conn, a_event->window);
	if (!(c = client_get(a_event->window, NULL))) {
		c = client_add(a_event->window, a_event->parent);
	}
	client_focus(c, 1, 1, 0);
}

void
event_property_notify(xcb_property_notify_event_t const *a_event)
{
	if (a_event->window == g_root) {
		if (XCB_ATOM_WM_NAME == a_event->atom) {
			root_name_update();
			g_do_bar_redraw = 1;
		}
	} else {
		struct Client *c;

		if ((c = client_get(a_event->window, NULL))) {
			if (XCB_ATOM_WM_HINTS == a_event->atom) {
				xcb_icccm_wm_hints_t hints;

				if (xcb_icccm_get_wm_hints_reply(g_conn,
				    xcb_icccm_get_wm_hints(g_conn,
				    a_event->window), &hints, NULL)) {
					if ((XCB_ICCCM_WM_HINT_X_URGENCY &
					    hints.flags) && g_focus != c) {
						c->is_urgent = 1;
						g_do_bar_redraw = 1;
					}
				}
			} else if (XCB_ATOM_WM_NORMAL_HINTS == a_event->atom) {
				xcb_icccm_get_wm_normal_hints_reply(g_conn,
				    xcb_icccm_get_wm_normal_hints(g_conn,
				    c->window), &c->hints, NULL);
			} else if (g_NET_WM_NAME == a_event->atom ||
			    XCB_ATOM_WM_NAME == a_event->atom) {
				client_name_update(c);
				g_do_bar_redraw = 1;
			}
		}
	}
}

void
event_unmap_notify(xcb_unmap_notify_event_t const *a_event)
{
	struct Client *c;

	c = client_get(a_event->window, NULL);
	client_free(&c);
}

void
my_exit()
{
	size_t i;

	for (i = 0; LENGTH(c_workspace_label) > i; ++i) {
		while (!TAILQ_EMPTY(&g_client_list[i])) {
			struct Client *c;

			c = TAILQ_FIRST(&g_client_list[i]);
			TAILQ_REMOVE(&g_client_list[i], c, next);
			client_free(&c);
		}
	}
	view_clear();
	if (g_conn) {
		xcb_flush(g_conn);
		xcb_disconnect(g_conn);
	}
	if ((iconv_t)-1 != g_iconv) {
		iconv_close(g_iconv);
	}
	FREE(g_workspace_label);
}

void
randr_update()
{
	xcb_timestamp_t timestamp;
	xcb_randr_get_screen_resources_current_reply_t *res;
	xcb_randr_output_t *output_array;
	xcb_randr_get_output_info_cookie_t *cookie_array;
	xcb_randr_get_output_primary_reply_t *primary;
	struct View *view;
	int i, len;

	view_clear();

	/* Get randr output IDs. */
	res = xcb_randr_get_screen_resources_current_reply(g_conn,
	    xcb_randr_get_screen_resources_current(g_conn, g_root), NULL);
	if (!res) {
		return;
	}
	timestamp = res->config_timestamp;
	len = xcb_randr_get_screen_resources_current_outputs_length(res);
	output_array = xcb_randr_get_screen_resources_current_outputs(res);

	primary = xcb_randr_get_output_primary_reply(g_conn,
	    xcb_randr_get_output_primary(g_conn, g_root), NULL);

	CALLOC(cookie_array, len);
	for (i = 0; len > i; ++i) {
		cookie_array[i] = xcb_randr_get_output_info(g_conn,
		    output_array[i], timestamp);
	}
	for (i = 0; len > i; ++i) {
		xcb_randr_get_output_info_reply_t *output_info;
		xcb_randr_get_crtc_info_reply_t *crtc;

		if (!(output_info = xcb_randr_get_output_info_reply(g_conn,
		    cookie_array[i], NULL))) {
			continue;
		}
		if (XCB_NONE == output_info->crtc) {
			free(output_info);
			continue;
		}
		crtc = xcb_randr_get_crtc_info_reply(g_conn,
		    xcb_randr_get_crtc_info(g_conn, output_info->crtc,
		    timestamp), NULL);
		free(output_info);
		if (!crtc) {
			continue;
		}
		CALLOC(view, 1);
		view->output = output_array[i];
		view->x = crtc->x;
		view->y = crtc->y;
		view->width = crtc->width;
		view->height = crtc->height;
		if (primary->output == view->output) {
			TAILQ_INSERT_HEAD(&g_view_list, view, next);
		} else {
			TAILQ_INSERT_TAIL(&g_view_list, view, next);
		}
		free(crtc);
	}
	assert(!TAILQ_EMPTY(&g_view_list));
	free(cookie_array);
	free(res);
	free(primary);
	bar_reset();
}

void
root_name_update()
{
	xcb_icccm_get_text_property_reply_t icccm;
	int do_default_name;

	do_default_name = 1;
	if (xcb_icccm_get_wm_name_reply(g_conn, xcb_icccm_get_wm_name(g_conn,
	    g_root), &icccm, NULL)) {
		char const *p = icccm.name;
		size_t len = icccm.name_len;

		if (0 != len) {
			if ((g_is_root_urgent = ('!' == *p))) {
				++p;
				--len;
			}
			string_convert(&g_root_name, p, len);
			do_default_name = 0;
		}
		xcb_icccm_get_text_property_reply_wipe(&icccm);
	}
	if (do_default_name) {
		string_convert(&g_root_name, "<hwm>", 5);
	}
}

void
string_convert(struct String *a_out, char const *a_in, size_t a_inlen)
{
	char *p = a_out->str;
	size_t inlen = a_inlen;
	size_t len;

	len = LENGTH(a_out->str);
	iconv(g_iconv, (char **)&a_in, &inlen, (char **)&p, &len);
	a_out->length = (LENGTH(a_out->str) - len) / 2;
}

int
text_draw(struct String const *a_text, int a_is_focused, int a_is_urgent, int
    a_x, int a_y)
{
	xcb_rectangle_t rect;
	uint32_t c_bg = a_is_urgent ? (g_blink ? g_color_urgent2_bg :
	    g_color_urgent1_bg) : a_is_focused ? g_color_bar_fg :
	    g_color_bar_bg;
	uint32_t c_fg = a_is_urgent ? (g_blink ? g_color_urgent2_fg :
	    g_color_urgent1_fg) : a_is_focused ? g_color_bar_bg :
	    g_color_bar_fg;
	int width;

	width = text_width(a_text);
	rect.x = a_x;
	rect.y = a_y;
	rect.width = width;
	rect.height = g_font_height;
	xcb_change_gc(g_conn, g_gc, XCB_GC_FOREGROUND, &c_bg);
	xcb_poly_fill_rectangle(g_conn, g_pixmap, g_gc, 1, &rect);

	xcb_change_gc(g_conn, g_gc, XCB_GC_BACKGROUND, &c_bg);
	xcb_change_gc(g_conn, g_gc, XCB_GC_FOREGROUND, &c_fg);
	xcb_image_text_16(g_conn, a_text->length, g_pixmap, g_gc, a_x +
	    c_text_padding, a_y + g_font_ascent,
	    (xcb_char2b_t const *)a_text->str);

	--rect.width;
	--rect.height;
	xcb_poly_rectangle(g_conn, g_pixmap, g_gc, 1, &rect);

	return width;
}

int
text_width(struct String const *a_text)
{
	xcb_query_text_extents_reply_t *reply;
	int width;

	reply = xcb_query_text_extents_reply(g_conn,
	    xcb_query_text_extents(g_conn, g_font, a_text->length,
	    (xcb_char2b_t const *)a_text->str), NULL);
	if (!reply) {
		width = 0;
	} else {
		width = reply->overall_width;
		free(reply);
	}
	return c_text_padding + width + c_text_padding;
}

void
view_clear()
{
	while (!TAILQ_EMPTY(&g_view_list)) {
		struct View *view;

		view = TAILQ_FIRST(&g_view_list);
		TAILQ_REMOVE(&g_view_list, view, next);
		FREE(view);
	}
}

struct View const *
view_find(int a_x, int a_y)
{
	struct View const *view;

	TAILQ_FOREACH(view, &g_view_list, next) {
		if (view->x <= a_x && a_x < view->x + view->width &&
		    view->y <= a_y && a_y < view->y + view->height) {
			return view;
		}
	}
	return TAILQ_FIRST(&g_view_list);
}

int
main(void)
{
	xcb_font_t cursor_font;
	xcb_screen_iterator_t it;
	struct pollfd fds;
	FILE *file;
	xcb_query_extension_reply_t const *ext_reply;
	xcb_query_font_reply_t *font_reply;
	xcb_query_tree_reply_t *tree_reply;
	struct KeyBind const *bind;
	size_t i;
	int screen_no, error;

	atexit(my_exit);

	CALLOC(g_client_list, LENGTH(c_workspace_label));
	for (i = 0; LENGTH(c_workspace_label) > i; ++i) {
		TAILQ_INIT(&g_client_list[i]);
	}

	if (SIG_ERR == signal(SIGCHLD, SIG_IGN)) {
		err(EXIT_FAILURE, "SIGCHLD=SIG_IGN failed");
	}
	if ((iconv_t)-1 == (g_iconv = iconv_open("UCS-2BE", "UTF-8"))) {
		err(EXIT_FAILURE, "Could not open iconv(UCS-2BE, UTF-8).");
	}
	CALLOC(g_workspace_label, LENGTH(c_workspace_label));
	for (i = 0; LENGTH(c_workspace_label) > i; ++i) {
		string_convert(&g_workspace_label[i], c_workspace_label[i],
		    strlen(c_workspace_label[i]));
	}

	/* XCB basics. */
	if (!(g_conn = xcb_connect(NULL, &screen_no))) {
		errx(EXIT_FAILURE, "NULL X11 connection.");
	}
	if (0 != (error = xcb_connection_has_error(g_conn))) {
		errx(EXIT_FAILURE, "X11 connection error=%d.", error);
	}
	for (it = xcb_setup_roots_iterator(xcb_get_setup(g_conn)); 0 <
	    screen_no--; xcb_screen_next(&it))
		;
	if (!(g_screen = it.data)) {
		errx(EXIT_FAILURE, "Could not get screen %d.", screen_no);
	}
	g_root = g_screen->root;

	/* RANDR. */
	ext_reply = xcb_get_extension_data(g_conn, &xcb_randr_id);
	if (!ext_reply->present) {
		errx(EXIT_FAILURE, "No RANDR.");
	}
	g_randr_evbase = ext_reply->first_event;
	xcb_randr_select_input(g_conn, g_root,
	    XCB_RANDR_NOTIFY_MASK_SCREEN_CHANGE |
	    XCB_RANDR_NOTIFY_MASK_CRTC_CHANGE |
	    XCB_RANDR_NOTIFY_MASK_OUTPUT_CHANGE |
	    XCB_RANDR_NOTIFY_MASK_OUTPUT_PROPERTY);

	/* Event handlers. */
#define EVENT_SET(type, func) g_event_handler[type] = (EventHandler)func
	EVENT_SET(XCB_KEY_PRESS, event_key_press);
	EVENT_SET(XCB_BUTTON_PRESS, event_button_press);
	EVENT_SET(XCB_ENTER_NOTIFY, event_enter_notify);
	EVENT_SET(XCB_EXPOSE, event_expose);
	EVENT_SET(XCB_DESTROY_NOTIFY, event_destroy_notify);
	EVENT_SET(XCB_UNMAP_NOTIFY, event_unmap_notify);
	EVENT_SET(XCB_MAP_REQUEST, event_map_request);
	EVENT_SET(XCB_CONFIGURE_NOTIFY, event_configure_notify);
	EVENT_SET(XCB_CONFIGURE_REQUEST, event_configure_request);
	EVENT_SET(XCB_PROPERTY_NOTIFY, event_property_notify);

	/* Pointer cursors. */
	cursor_font = xcb_generate_id(g_conn);
	xcb_open_font(g_conn, cursor_font, 6, "cursor");
	g_cursor_normal = cursor_get(cursor_font, XC_left_ptr);
	g_cursor_move = cursor_get(cursor_font, XC_fleur);
	g_cursor_resize = cursor_get(cursor_font, XC_bottom_right_corner);

	/* Can we start a WM? */
	g_values[0] = XCB_EVENT_MASK_STRUCTURE_NOTIFY |
	    XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
	    XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
	    XCB_EVENT_MASK_PROPERTY_CHANGE;
	g_values[1] = g_cursor_normal;
	HWM_XCB_CHECKED("Is another WM running?",
	    xcb_change_window_attributes_checked, (g_conn, g_root,
	    XCB_CW_EVENT_MASK | XCB_CW_CURSOR, g_values));

	g_key_symbols = xcb_key_symbols_alloc(g_conn);

	/* Atoms. */
	g_WM_DELETE_WINDOW = atom_get("WM_DELETE_WINDOW");
	g_WM_PROTOCOLS = atom_get("WM_PROTOCOLS");
	g_NET_WM_NAME = atom_get("_NET_WM_NAME");

	/* Graphics. */
	g_font = xcb_generate_id(g_conn);
	xcb_open_font(g_conn, g_font, sizeof(c_font_face) - 1, c_font_face);
	if (!(font_reply = xcb_query_font_reply(g_conn, xcb_query_font(g_conn,
	    g_font), NULL))) {
		errx(EXIT_FAILURE, "Could not load font face '%s'.",
		    c_font_face);
	}
	g_font_ascent = font_reply->font_ascent;
	g_font_height = g_font_ascent + font_reply->font_descent;
	free(font_reply);

	g_gc = xcb_generate_id(g_conn);
	xcb_create_gc(g_conn, g_gc, g_root, 0, NULL);
	xcb_change_gc(g_conn, g_gc, XCB_GC_FONT, &g_font);

	/* Configs. */
	g_color_border_focus = color_get(c_border_focus);
	g_color_border_unfocus = color_get(c_border_unfocus);
	g_color_bar_bg = color_get(c_bar_bg);
	g_color_bar_fg = color_get(c_bar_fg);
	g_color_urgent1_bg = color_get(c_urgent1_bg);
	g_color_urgent1_fg = color_get(c_urgent1_fg);
	g_color_urgent2_bg = color_get(c_urgent2_bg);
	g_color_urgent2_fg = color_get(c_urgent2_fg);

	for (i = 0, bind = c_key_bind; LENGTH(c_key_bind) > i; ++i, ++bind) {
		xcb_keycode_t *keycode_list;

		keycode_list = xcb_key_symbols_get_keycode(g_key_symbols,
		    bind->keysym);
		xcb_grab_key(g_conn, 1, g_root, bind->state, *keycode_list,
		    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
		free(keycode_list);
	}

	randr_update();

	/* Furnish existing windows, and reuse persist info. */
	if ((tree_reply = xcb_query_tree_reply(g_conn, xcb_query_tree(g_conn,
	    g_root), NULL))) {
		char line[80], *p;
		struct Client *c;
		int j[11];
		xcb_window_t *w;
		size_t num;

		(void)c;
		w = xcb_query_tree_children(tree_reply);
		num = xcb_query_tree_children_length(tree_reply);
		if ((file = fopen(c_persist_file, "rb"))) {
			xcb_window_t focus_id;

			fgets(line, sizeof line, file);
			g_workspace_cur = strtol(line, NULL, 10);
			g_workspace_cur = MIN(LENGTH(c_workspace_label),
			    g_workspace_cur);
			fgets(line, sizeof line, file);
			focus_id = strtol(line, NULL, 10);
			while (fgets(line, sizeof line, file)) {
				for (p = strtok(line, " "), i = 0; p && 11 >
				    i; p = strtok(NULL, " "), ++i) {
					j[i] = strtol(p, NULL, 10);
				}
				if (11 == i) {
					for (i = 0; num > i; ++i) {
						if (j[0] == (int)w[i]) {
							c = client_add_details
							    (w[i],
							     XCB_WINDOW_NONE,
							     j + 1);
							w[i] = XCB_NONE;
							break;
						}
					}
				}
			}
			if (XCB_NONE != focus_id) {
				g_focus = client_get(focus_id, NULL);
				client_focus(g_focus, 1, 1, 0);
			}
			fclose(file);
		}
		for (i = 0; num > i; ++i) {
			if (XCB_NONE != w[i]) {
				c = client_add(w[i], XCB_WINDOW_NONE);
			}
		}
		free(tree_reply);
	}

	/* Bar. */
	root_name_update();
	bar_draw();

	xcb_flush(g_conn);

	/* Main loop. */
	fds.fd = xcb_get_file_descriptor(g_conn);
	fds.events = POLLIN;
	g_has_urgent = 0;
	g_timeout = c_timeout_normal;
	for (g_run = RUN_LOOP; RUN_LOOP == g_run;) {
		xcb_generic_event_t *ev;

		poll(&fds, 1, g_timeout);
		g_do_bar_redraw = 0;
		while ((ev = xcb_poll_for_event(g_conn))) {
			event_handle(ev);
			xcb_flush(g_conn);
			free(ev);
		}
		if (g_has_urgent) {
			struct timeval tv;
			double dt, time_cur;

			gettimeofday(&tv, NULL);
			time_cur = 1e3 * tv.tv_sec + 1e-3 * tv.tv_usec;
			dt = time_cur - g_time_prev;
			g_time_prev = time_cur;
			g_timeout = MIN(g_timeout - dt, c_timeout_blink);
			if (0 >= g_timeout) {
				g_timeout = c_timeout_blink;
				g_blink ^= 1;
				g_do_bar_redraw = 1;
			}
		} else {
			g_timeout = c_timeout_normal;
			g_blink = 0;
		}
		if (g_do_bar_redraw) {
			bar_draw();
			xcb_flush(g_conn);
		}
	}

	/* Save persist info. */
	if (RUN_QUIT == g_run) {
		remove(c_persist_file);
		exit(0);
	}

	if (!(file = fopen(c_persist_file, "wb"))) {
		warn("Could not save persist info");
	} else {
		fprintf(file, "%d\n", (int)g_workspace_cur);
		fprintf(file, "%u\n", g_focus ? g_focus->window :
		    (xcb_window_t)XCB_NONE);
		for (i = 0; LENGTH(c_workspace_label) > i; ++i) {
			struct Client *c;

			TAILQ_FOREACH(c, &g_client_list[i], next) {
				fprintf(file, "%u %d %d %d %d %d %d %d %d "
				    "%d %d\n", c->window, (int)i,
				    c->is_urgent, c->x, c->y, c->maximize,
				    c->border_width, c->max_old_x,
				    c->max_old_y, c->max_old_width,
				    c->max_old_height);
			}
		}
		fclose(file);
	}

	exit(2);
}
