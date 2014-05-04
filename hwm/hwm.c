/*
 * TODO:
 * Better client placement with fudge (mm, fudge).
 */

#include <sys/queue.h>
#include <sys/time.h>
#include <assert.h>
#include <iconv.h>
#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_icccm.h>
#include <X11/cursorfont.h>

#define CONVERGE(op, ref, test, cur) do {\
	if (ref op test && test op cur) {\
		cur = test;\
	}\
} while (0)
#define EVENT_MAX 30
#define HEIGHT(c) (1 + (c)->height + 1)
#define HWM_XCB_CHECKED(msg, func, args) do {\
	xcb_void_cookie_t cookie_;\
	xcb_generic_error_t *error_;\
	cookie_ = func args;\
	if ((error_ = xcb_request_check(g_conn, cookie_))) {\
		fprintf(stderr, msg" (%u,%u,0x%08x,%d,%d).\n",\
		    error_->response_type, error_->error_code,\
		    error_->resource_id, error_->minor_code,\
		    error_->major_code);\
	}\
} while (0)
#define LENGTH(arr) (sizeof arr / sizeof arr[0])
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define NAME_LEN 80
#define SNAP(op, ref, test, margin) do {\
	if (ref op test && test op margin) {\
		test = ref;\
	}\
} while (0)
#define WIDTH(c) (1 + (c)->width + 1)

enum Click { CLICK_ROOT, CLICK_WORKSPACE, CLICK_STATUS, CLICK_CLIENT };
enum JumpDirection { JUMP_EAST, JUMP_NORTH, JUMP_WEST, JUMP_SOUTH };
enum Maximize { MAX_NOPE, MAX_BOTH, MAX_VERT };
enum RunControl { RUN_LOOP, RUN_QUIT, RUN_RESTART };
enum Visibility { VISIBLE, HIDDEN };

struct Arg {
	int	i;
	void	const *v;
};
struct String {
	size_t	length;
	char	str[NAME_LEN];
};
TAILQ_HEAD(ClientList, Client);
struct Client {
	xcb_window_t	window;
	xcb_size_hints_t	hints;
	int	is_urgent;
	struct	String name;
	int	x, y;
	int	width, height;
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
	int	code;
	int	state;
	void	(*action)(struct Arg const *);
	struct	Arg const arg;
};

static void		action_client_browse(struct Arg const *);
static void		action_client_jump(struct Arg const *);
static void		action_client_maximize(struct Arg const *);
static void		action_client_move(struct Arg const *);
static void		action_client_relocate(struct Arg const *);
static void		action_client_resize(struct Arg const *);
static void		action_exec(struct Arg const *);
static void		action_furnish(struct Arg const *);
static void		action_kill(struct Arg const *);
static void		action_quit(struct Arg const *);
static void		action_restart(struct Arg const *);
static void		action_workspace_select(struct Arg const *);
static xcb_atom_t	atom_get(char const *);
static void		bar_draw(void);
static void		button_grab(struct Client *);
static struct Client	*client_add(xcb_window_t);
static struct Client	*client_add_details(xcb_window_t, int const *);
static void		client_delete(struct Client *);
static void		client_focus(struct Client *, int, int);
static struct Client	*client_get(xcb_window_t, int *);
static void		client_move(struct Client *, enum Visibility);
static void		client_name_update(struct Client *);
static void		client_place(struct Client *);
static void		client_resize(struct Client *, int);
static void		client_snap_dimension(struct Client *);
static void		client_snap_position(struct Client *);
static uint32_t		color_get(char const *);
static xcb_cursor_t	cursor_get(xcb_font_t, int);
static void		die(char const *, ...);
static void		event_button_press(xcb_generic_event_t const *);
static void		event_configure_notify(xcb_generic_event_t const *);
static void		event_configure_request(xcb_generic_event_t const *);
static void		event_destroy_notify(xcb_generic_event_t const *);
static void		event_enter_notify(xcb_generic_event_t const *);
static void		event_expose(xcb_generic_event_t const *);
static void		event_handle(xcb_generic_event_t const *);
static void		event_key_press(xcb_generic_event_t const *);
static void		event_map_request(xcb_generic_event_t const *);
static void		event_property_notify(xcb_generic_event_t const *);
static void		event_unmap_notify(xcb_generic_event_t const *);
static void		root_name_update(void);
static void		string_convert(struct String *, char const *, size_t);
static int		text_draw(struct String const *, int, int, int, int);
static int		text_width(struct String const *);

/* Config { */

#define BORDER_FOCUS "red"
#define BORDER_UNFOCUS "blue"
#define BAR_BG "gray10"
#define BAR_FG "white"
#define URGENT1_BG "red"
#define URGENT1_FG "white"
#define URGENT2_BG "yellow"
#define URGENT2_FG "black"
#define BOOKEND_LENGTH 3
#define FONT_FACE "fixed"
#define MOD_MASK1 XCB_MOD_MASK_4
#define MOD_MASK2 (XCB_MOD_MASK_1 | XCB_MOD_MASK_4)
#define OVERLAP_FUDGE 10
#define PERSIST_FILE "/tmp/hwm.txt"
#define SNAP_MARGIN 6
#define TEXT_PADDING 4
#define TIMEOUT_NORMAL 3000
#define TIMEOUT_BLINK 200
#define WORKSPACE_NUM 6

static char const *c_workspace_label[WORKSPACE_NUM] = {
	"1", "2", "3", "4", "5", "6"
};
static char const *c_uxterm[] = {"uxterm", NULL};
static char const *c_dmenu[] = { "dmenu_run", "-i", "-fn", FONT_FACE, "-nb",
	BAR_BG, "-nf", BAR_FG, "-sb", BAR_FG, "-sf", BAR_BG, NULL};
static struct KeyBind c_key_bind[] = {
	{24, MOD_MASK2, action_quit, {0, NULL}},
	{38, MOD_MASK2, action_restart, {0, NULL}},
	{53, MOD_MASK2, action_kill, {0, NULL}},
	{25, MOD_MASK1, action_workspace_select, {0, NULL}},
	{26, MOD_MASK1, action_workspace_select, {1, NULL}},
	{27, MOD_MASK1, action_workspace_select, {2, NULL}},
	{39, MOD_MASK1, action_workspace_select, {3, NULL}},
	{40, MOD_MASK1, action_workspace_select, {4, NULL}},
	{41, MOD_MASK1, action_workspace_select, {5, NULL}},
	{25, MOD_MASK2, action_client_relocate, {0, NULL}},
	{26, MOD_MASK2, action_client_relocate, {1, NULL}},
	{27, MOD_MASK2, action_client_relocate, {2, NULL}},
	{39, MOD_MASK2, action_client_relocate, {3, NULL}},
	{40, MOD_MASK2, action_client_relocate, {4, NULL}},
	{41, MOD_MASK2, action_client_relocate, {5, NULL}},
	{111, MOD_MASK1, action_client_jump, {JUMP_NORTH, NULL}},
	{113, MOD_MASK1, action_client_jump, {JUMP_WEST, NULL}},
	{114, MOD_MASK1, action_client_jump, {JUMP_EAST, NULL}},
	{116, MOD_MASK1, action_client_jump, {JUMP_SOUTH, NULL}},
	{65, MOD_MASK1, action_client_maximize, {MAX_BOTH, NULL}},
	{65, MOD_MASK2, action_client_maximize, {MAX_VERT, NULL}},
	{23, MOD_MASK1, action_client_browse, {0, NULL}},
	{58, MOD_MASK1, action_furnish, {0, NULL}},
	{38, MOD_MASK1, action_exec, {0, c_uxterm}},
	{28, MOD_MASK1, action_exec, {0, c_dmenu}}
};
static struct ButtonBind c_button_bind[] = {
	{CLICK_WORKSPACE, 1, 0, action_workspace_select},
	{CLICK_CLIENT, 1, MOD_MASK1, action_client_move},
	{CLICK_CLIENT, 3, MOD_MASK1, action_client_resize}
};

/* } */

static struct String g_workspace_label[WORKSPACE_NUM];
static uint32_t g_values[3];
static void (*g_event_handler[EVENT_MAX])(xcb_generic_event_t const *);
static xcb_connection_t *g_conn;
static xcb_screen_t *g_screen;
static int g_width, g_height;
static iconv_t g_iconv;
static xcb_drawable_t g_root;
static struct String g_root_name;
static int g_root_is_urgent;
static xcb_gc_t g_gc;
static xcb_pixmap_t g_pixmap;
static xcb_font_t g_font;
static int g_font_ascent, g_font_height;
static xcb_atom_t g_WM_DELETE_WINDOW, g_WM_PROTOCOLS;
static xcb_atom_t g_NET_WM_NAME;
static xcb_cursor_t g_cursor_normal, g_cursor_move, g_cursor_resize;
static xcb_window_t g_bar;
static int g_bar_redraw;
static uint32_t g_color_border_focus, g_color_border_unfocus;
static uint32_t g_color_bar_bg, g_color_bar_fg;
static uint32_t g_color_urgent1_bg, g_color_urgent1_fg;
static uint32_t g_color_urgent2_bg, g_color_urgent2_fg;
static struct ClientList g_client_list[WORKSPACE_NUM];
static int g_workspace_cur;
static struct Client *g_focus;
static enum RunControl g_run;
static int g_has_urgent, g_timeout, g_blink;
static double g_time_prev;
static xcb_window_t g_button_press_window;
static int g_button_press_x, g_button_press_y;

void
action_client_browse(struct Arg const *a_arg)
{
	struct Client *c;
	int do_browse;

	(void)a_arg;
	if (TAILQ_EMPTY(&g_client_list[g_workspace_cur]) || !(c =
	    TAILQ_NEXT(TAILQ_FIRST(&g_client_list[g_workspace_cur]), next))) {
		return;
	}
	client_focus(c, 0, 1);
	bar_draw();
	xcb_grab_keyboard(g_conn, 0, g_root, XCB_CURRENT_TIME,
	    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	xcb_flush(g_conn);
	for (do_browse = 1; do_browse;) {
		xcb_generic_event_t *event;
		xcb_key_press_event_t const *kp;

		if (!(event = xcb_poll_for_event(g_conn))) {
			continue;
		}
		kp = (xcb_key_press_event_t const *)event;
		switch (XCB_EVENT_RESPONSE_TYPE(event)) {
			case XCB_KEY_PRESS:
				if (23 == kp->detail) {
					if (!(c = TAILQ_NEXT(c, next))) {
						c = TAILQ_FIRST(
						    &g_client_list[
						    g_workspace_cur]);
					}
					client_focus(c, 0, 1);
					bar_draw();
					xcb_flush(g_conn);
				}
				break;
			case XCB_KEY_RELEASE:
				do_browse = 133 != kp->detail && 134 !=
				    kp->detail;
				break;
			case XCB_EXPOSE:
			case XCB_MAP_REQUEST:
			case XCB_CONFIGURE_REQUEST:
				event_handle(event);
				break;
		}
		free(event);
	}
	xcb_ungrab_keyboard(g_conn, XCB_CURRENT_TIME);
	client_focus(c, 1, 0);
}

void
action_client_jump(struct Arg const *a_arg)
{
	struct Client *c;
	int new, test;

	if (!g_focus) {
		return;
	}
	switch (a_arg->i) {
		case JUMP_EAST: new = g_width - WIDTH(g_focus); break;
		case JUMP_NORTH: new = g_font_height; break;
		case JUMP_WEST: new = 0; break;
		case JUMP_SOUTH: new = g_height - HEIGHT(g_focus); break;
		default: abort();
	}
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		if (g_focus == c) {
			continue;
		}
		switch (a_arg->i) {
			case JUMP_EAST:
				test = c->x - WIDTH(g_focus);
				CONVERGE(<, g_focus->x, test, new);
				break;
			case JUMP_NORTH:
				test = c->y + HEIGHT(c);
				CONVERGE(>, g_focus->y, test, new);
				break;
			case JUMP_WEST:
				test = c->x + WIDTH(c);
				CONVERGE(>, g_focus->x, test, new);
				break;
			case JUMP_SOUTH:
				test = c->y - HEIGHT(g_focus);
				CONVERGE(<, g_focus->y, test, new);
				break;
		}
	}
	(JUMP_EAST == a_arg->i || JUMP_WEST == a_arg->i) ? (g_focus->x = new)
	    : (g_focus->y = new);
	client_move(g_focus, VISIBLE);
}

void
action_client_maximize(struct Arg const *a_arg)
{
	enum Maximize const c_toggle = a_arg->i;
	enum Maximize result;

	if (!g_focus) {
		return;
	}
	switch (g_focus->maximize) {
		case MAX_NOPE:
			g_focus->max_old_x = g_focus->x;
			g_focus->max_old_y = g_focus->y;
			g_focus->max_old_width = g_focus->width;
			g_focus->max_old_height = g_focus->height;
			result = c_toggle;
			break;
		case MAX_BOTH:
			result = MAX_BOTH == c_toggle ? MAX_NOPE : MAX_VERT;
			break;
		case MAX_VERT:
			result = MAX_VERT == c_toggle ? MAX_NOPE : MAX_BOTH;
			break;
		default:
			abort();
	}
	switch (result) {
		case MAX_NOPE:
			g_focus->x = g_focus->max_old_x;
			g_focus->y = g_focus->max_old_y;
			g_focus->width = g_focus->max_old_width;
			g_focus->height = g_focus->max_old_height;
			break;
		case MAX_BOTH:
			g_focus->x = 0;
			g_focus->y = g_font_height;
			g_focus->width = g_width - 2;
			g_focus->height = g_height - 2 - g_font_height;
			break;
		case MAX_VERT:
			g_focus->x = g_focus->max_old_x;
			g_focus->y = g_font_height;
			g_focus->width = g_focus->max_old_width;
			g_focus->height = g_height - 2 - g_font_height;
			break;
	}
	g_focus->maximize = result;
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

		event = xcb_poll_for_event(g_conn);
		if (!event) {
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
			case XCB_MAP_REQUEST:
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
	assert(0 <= a_arg->i && WORKSPACE_NUM > a_arg->i);
	if (!g_focus || g_workspace_cur == a_arg->i) {
		return;
	}
	TAILQ_REMOVE(&g_client_list[g_workspace_cur], g_focus, next);
	TAILQ_INSERT_HEAD(&g_client_list[a_arg->i], g_focus, next);
	client_move(g_focus, HIDDEN);
	client_focus(g_focus = NULL, 1, 0);
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

		if (!(event = xcb_poll_for_event(g_conn))) {
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
			case XCB_MAP_REQUEST:
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

	if (0 > (pid = fork())) {
		fprintf(stderr, "Could not fork for '%s'.\n", arg0);
	} else if (0 == pid) {
		execvp(arg0, a_arg->v);
		fprintf(stderr, "Failed to exec '%s'.\n", arg0);
		_exit(0);
	}
}

void
action_furnish(struct Arg const *a_arg)
{
	struct Client *c;
	int num = 0;

	(void)a_arg;
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		c->x += 10000;
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
}

void
action_kill(struct Arg const *a_arg)
{
	(void)a_arg;
	if (g_focus) {
		xcb_icccm_get_wm_protocols_reply_t proto;
		size_t i;
		int has_delete = 0;

		proto.atoms_len = 0;
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
		client_delete(g_focus);
	}
}

void
action_quit(struct Arg const *a_arg)
{
	(void)a_arg;
	g_run = RUN_QUIT;
}

void
action_restart(struct Arg const *a_arg)
{
	(void)a_arg;
	g_run = RUN_RESTART;
}

void
action_workspace_select(struct Arg const *a_arg)
{
	struct Client *c;

	assert(0 <= a_arg->i && WORKSPACE_NUM > a_arg->i);
	if (g_workspace_cur == a_arg->i) {
		return;
	}
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		client_move(c, HIDDEN);
	}
	g_workspace_cur = a_arg->i;
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		client_move(c, VISIBLE);
	}
	client_focus(g_focus = NULL, 1, 0);
}

xcb_atom_t
atom_get(char const *a_name)
{
	xcb_intern_atom_reply_t *reply;
	xcb_atom_t atom;

	if (!(reply = xcb_intern_atom_reply(g_conn, xcb_intern_atom(g_conn, 1,
	    strlen(a_name), a_name), NULL))) {
		die("Could not get atom '%s'.", a_name);
	}
	atom = reply->atom;
	free(reply);
	return atom;
}

void
bar_draw(void)
{
	xcb_rectangle_t rect;
	struct Client *c;
	int i, x;

	rect.x = 0;
	rect.y = 0;
	rect.width = g_width;
	rect.height = g_font_height;
	xcb_change_gc(g_conn, g_gc, XCB_GC_FOREGROUND, &g_color_bar_bg);
	xcb_poly_fill_rectangle(g_conn, g_pixmap, g_gc, 1, &rect);

	g_has_urgent = g_root_is_urgent;
	for (x = i = 0; WORKSPACE_NUM > i; ++i) {
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
	text_draw(&g_root_name, 0, g_root_is_urgent, g_width -
	    text_width(&g_root_name), 0);
	TAILQ_FOREACH(c, &g_client_list[g_workspace_cur], next) {
		x += text_draw(&c->name, g_focus == c, c->is_urgent, x, 0);
	}

	xcb_copy_area(g_conn, g_pixmap, g_bar, g_gc, 0, 0, 0, 0, g_width,
	    g_font_height);
}

void
button_grab(struct Client *a_client)
{
	struct ButtonBind *bind;
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
client_add(xcb_window_t a_window)
{
	int arr[9] = {-1};

	return client_add_details(a_window, arr);
}

struct Client *
client_add_details(xcb_window_t a_window, int const *a_data)
{
	xcb_get_window_attributes_reply_t *attr;
	xcb_get_geometry_reply_t *geom;
	struct Client *c;
	int workspace;

	if ((attr = xcb_get_window_attributes_reply(g_conn,
	    xcb_get_window_attributes(g_conn, a_window), NULL))) {
		if (XCB_MAP_STATE_VIEWABLE != attr->map_state ||
		    attr->override_redirect) {
			free(attr);
			return NULL;
		}
		free(attr);
	}

	geom = xcb_get_geometry_reply(g_conn, xcb_get_geometry(g_conn,
	    a_window), NULL);
	if (!geom) {
		fprintf(stderr, "client_add: Could not get geometry of window"
		    " 0x%08x.\n", a_window);
		return NULL;
	}

	c = malloc(sizeof(struct Client));
	c->window = a_window;
	c->hints.flags = 0;
	xcb_icccm_get_wm_normal_hints_reply(g_conn,
	    xcb_icccm_get_wm_normal_hints(g_conn, a_window), &c->hints, NULL);
	client_name_update(c);
	c->width = geom->width;
	c->height = geom->height;
	free(geom);
	if (0 > a_data[0]) {
		workspace = g_workspace_cur;
		c->is_urgent = 0;
		c->maximize = MAX_NOPE;
		c->max_old_y = c->max_old_x = 0;
		c->max_old_height = c->max_old_width = 0;
		client_place(c);
	} else {
		workspace = a_data[0];
		c->is_urgent = a_data[1];
		c->x = a_data[2];
		c->y = a_data[3];
		c->maximize = a_data[4];
		c->max_old_x = a_data[5];
		c->max_old_y = a_data[6];
		c->max_old_width = a_data[7];
		c->max_old_height = a_data[8];
	}
	TAILQ_INSERT_HEAD(&g_client_list[workspace], c, next);

	button_grab(c);

	g_values[0] = g_color_border_unfocus;
	g_values[1] = XCB_EVENT_MASK_ENTER_WINDOW |
	    XCB_EVENT_MASK_STRUCTURE_NOTIFY | XCB_EVENT_MASK_PROPERTY_CHANGE;
	HWM_XCB_CHECKED("Could not set c event mask.",
	    xcb_change_window_attributes_checked, (g_conn, a_window,
	    XCB_CW_BORDER_PIXEL | XCB_CW_EVENT_MASK, g_values));
	g_values[0] = 1;
	xcb_configure_window(g_conn, a_window, XCB_CONFIG_WINDOW_BORDER_WIDTH,
	    g_values);

	if (g_workspace_cur != workspace) {
		client_move(c, HIDDEN);
	}
	return c;
}

void
client_delete(struct Client *a_client)
{
	if (a_client) {
		TAILQ_REMOVE(&g_client_list[g_workspace_cur], a_client, next);
		free(a_client);
		if (g_focus == a_client) {
			client_focus(g_focus = NULL, 1, 0);
		}
	}
}

void
client_focus(struct Client *a_client, int a_do_reorder, int a_do_raise)
{
	if (g_focus) {
		g_values[0] = g_color_border_unfocus;
		xcb_change_window_attributes(g_conn, g_focus->window,
		    XCB_CW_BORDER_PIXEL, g_values);
	}
	g_focus = a_client ? a_client :
	    TAILQ_FIRST(&g_client_list[g_workspace_cur]);
	if (g_focus) {
		xcb_icccm_wm_hints_t hints;

		if (xcb_icccm_get_wm_hints_reply(g_conn,
		    xcb_icccm_get_wm_hints(g_conn, g_focus->window),
		    &hints, NULL)) {
			hints.flags &= ~XCB_ICCCM_WM_HINT_X_URGENCY;
			xcb_icccm_set_wm_hints(g_conn, g_focus->window,
			    &hints);
		}
		g_focus->is_urgent = 0;
		if (a_do_reorder) {
			TAILQ_REMOVE(&g_client_list[g_workspace_cur], g_focus,
			    next);
			TAILQ_INSERT_HEAD(&g_client_list[g_workspace_cur],
			    g_focus, next);
		}
		if (a_do_raise) {
			g_values[0] = XCB_STACK_MODE_TOP_IF;
			xcb_configure_window(g_conn, g_focus->window,
			    XCB_CONFIG_WINDOW_STACK_MODE, g_values);
		}
		g_values[0] = g_color_border_focus;
		xcb_change_window_attributes(g_conn, g_focus->window,
		    XCB_CW_BORDER_PIXEL, g_values);
		xcb_set_input_focus(g_conn, XCB_INPUT_FOCUS_POINTER_ROOT,
		    g_focus->window, XCB_CURRENT_TIME);
	}
	g_bar_redraw = 1;
}

struct Client *
client_get(xcb_window_t a_window, int *a_out_workspace)
{
	struct Client *c;
	size_t i;

	for (i = 0; WORKSPACE_NUM > i; ++i) {
		TAILQ_FOREACH(c, &g_client_list[i], next) {
			if (c->window == a_window) {
				a_out_workspace ? *a_out_workspace = i : 0;
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
	int best_score = 1e9, best_touching, best_x = 0, best_y =
	    g_font_height, x = g_width - WIDTH(a_client), y = g_font_height -
	    1;

	for (;;) {
		struct Client *sibling;
		int score = 0, touching = 0, new_x, new_y, test;

		if (x + WIDTH(a_client) == g_width) {
			if (y + HEIGHT(a_client) == g_height) {
				break;
			}
			new_y = g_height - HEIGHT(a_client);
			TAILQ_FOREACH(sibling,
			    &g_client_list[g_workspace_cur], next) {
				if (sibling != a_client) {
					test = sibling->y - HEIGHT(a_client);
					CONVERGE(<, y, test, new_y);
					test = sibling->y + OVERLAP_FUDGE;
					CONVERGE(<, y, test, new_y);
					test = sibling->y + HEIGHT(sibling) -
					    HEIGHT(a_client) - OVERLAP_FUDGE;
					CONVERGE(<, y, test, new_y);
					test = sibling->y + HEIGHT(sibling);
					CONVERGE(<, y, test, new_y);
				}
			}
			x = -1;
			test = g_font_height;
			if (y < test && test < new_y) {
				new_y = test;
			}
			y = new_y;
		}
		new_x = g_width - WIDTH(a_client);
		TAILQ_FOREACH(sibling, &g_client_list[g_workspace_cur], next)
		{
			if (sibling != a_client) {
				test = sibling->x - WIDTH(a_client);
				CONVERGE(<, x, test, new_x);
				test = sibling->x + OVERLAP_FUDGE;
				CONVERGE(<, x, test, new_x);
				test = sibling->x + WIDTH(sibling) -
				    WIDTH(a_client) - OVERLAP_FUDGE;
				CONVERGE(<, x, test, new_x);
				test = sibling->x + WIDTH(sibling);
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
				int x1 = MIN(x + WIDTH(a_client), sibling->x +
				    WIDTH(sibling));
				int y1 = MIN(y + HEIGHT(a_client), sibling->y
				    + HEIGHT(sibling));
				int s = MAX(0, x1 - x0) * MAX(0, y1 - y0);

				if (0 < s) {
					++touching;
				}
				score += s;
			}
		}
		if (best_score > score) {
			best_score = score;
			best_touching = touching;
			best_x = x;
			best_y = y;
		}
	}

	a_client->x = MIN(best_x + 0 * OVERLAP_FUDGE * best_touching, g_width
	    - WIDTH(a_client));
	a_client->y = MIN(best_y + 0 * OVERLAP_FUDGE * best_touching, g_height
	    - HEIGHT(a_client));
	client_move(a_client, VISIBLE);
}

void
client_resize(struct Client *a_client, int a_do_round)
{
	xcb_size_hints_t const *hints = &a_client->hints;
	int min_width = 0, min_height = 0;

	if (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE & hints->flags) {
		min_width = hints->min_width;
		min_height = hints->min_height;
	}
	if (XCB_ICCCM_SIZE_HINT_P_RESIZE_INC & hints->flags) {
		int wi = hints->width_inc;
		int hi = hints->height_inc;
		int f = a_do_round ? 1 : 0;

		a_client->width = ((a_client->width - min_width + f * wi / 2)
		    / wi) * wi + min_width;
		a_client->height = ((a_client->height - min_height + f * hi /
		    2) / hi) * hi + min_height;
	}
	if (XCB_ICCCM_SIZE_HINT_P_MIN_SIZE & hints->flags) {
		a_client->width = MAX(a_client->width, min_width);
		a_client->height = MAX(a_client->height, min_height);
	}
	if (XCB_ICCCM_SIZE_HINT_P_MAX_SIZE & hints->flags) {
		a_client->width = MIN(a_client->width, hints->max_width);
		a_client->height = MIN(a_client->height, hints->max_height);
	}
	if (XCB_ICCCM_SIZE_HINT_P_ASPECT & hints->flags) {
printf("Aspect = %d:%d .. %dx%d\n",
    hints->min_aspect_num, hints->min_aspect_den,
    hints->max_aspect_num, hints->max_aspect_den);
	}
	g_values[0] = a_client->width;
	g_values[1] = a_client->height;
	xcb_configure_window(g_conn, a_client->window, XCB_CONFIG_WINDOW_WIDTH
	    | XCB_CONFIG_WINDOW_HEIGHT, g_values);
}

void
client_snap_dimension(struct Client *a_client)
{
	struct Client *sibling;
	int ref;

	TAILQ_FOREACH(sibling, &g_client_list[g_workspace_cur], next) {
		if (a_client == sibling) {
			continue;
		}
		ref = sibling->x - a_client->x - 2;
		SNAP(<, ref, a_client->width, ref + SNAP_MARGIN);
		ref = sibling->y - a_client->y - 2;
		SNAP(<, ref, a_client->height, ref + SNAP_MARGIN);
	}
	ref = g_width - a_client->x - 2;
	SNAP(<, ref, a_client->width, ref + SNAP_MARGIN);
	ref = g_height - a_client->y - 2;
	SNAP(<, ref, a_client->height, ref + SNAP_MARGIN);
}

void
client_snap_position(struct Client *a_client)
{
	struct Client *sibling;
	int ref;

	TAILQ_FOREACH(sibling, &g_client_list[g_workspace_cur], next) {
		if (a_client == sibling) {
			continue;
		}
		ref = sibling->x - WIDTH(a_client);
		SNAP(<, ref, a_client->x, ref + SNAP_MARGIN);
		ref = sibling->x + WIDTH(sibling);
		SNAP(>, ref, a_client->x, ref - SNAP_MARGIN);
		ref = sibling->y - HEIGHT(a_client);
		SNAP(<, ref, a_client->y, ref + SNAP_MARGIN);
		ref = sibling->y + HEIGHT(sibling);
		SNAP(>, ref, a_client->y, ref - SNAP_MARGIN);
	}
	ref = g_width - WIDTH(a_client);
	SNAP(<, ref, a_client->x, ref + SNAP_MARGIN);
	ref = 0;
	SNAP(>, ref, a_client->x, ref - SNAP_MARGIN);

	ref = g_height - HEIGHT(a_client);
	SNAP(<, ref, a_client->y, ref + SNAP_MARGIN);
	ref = g_font_height;
	SNAP(>, ref, a_client->y, ref - SNAP_MARGIN);
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
die(char const *a_fmt, ...)
{
	va_list args;

	va_start(args, a_fmt);
	fprintf(stderr, a_fmt, args);
	va_end(args);
	if (g_conn) {
		xcb_disconnect(g_conn);
	}
	exit(EXIT_FAILURE);
}

void
event_button_press(xcb_generic_event_t const *a_event)
{
	struct Arg arg;
	xcb_button_press_event_t const *ev = (xcb_button_press_event_t const
	    *)a_event;
	struct ButtonBind *bind;
	struct Client *c;
	enum Click click = CLICK_ROOT;
	size_t i;

	if (ev->event == g_bar) {
		int x = 0;

		i = 0;
		do {
			x += text_width(&g_workspace_label[i]);
		} while (x <= ev->root_x && WORKSPACE_NUM > ++i);
		if (WORKSPACE_NUM > i) {
			click = CLICK_WORKSPACE;
			arg.i = i;
		} else if (g_width - text_width(&g_root_name) <= ev->root_x) {
			click = CLICK_STATUS;
		}
	} else if ((c = client_get(ev->event, NULL))) {
		click = CLICK_CLIENT;
		client_focus(c, 1, 1);
	}
	g_button_press_window = ev->event;
	g_button_press_x = ev->root_x;
	g_button_press_y = ev->root_y;
	for (i = 0, bind = c_button_bind; LENGTH(c_button_bind) > i; ++i,
	    ++bind) {
		if (bind->click == click && bind->code == ev->detail &&
		    bind->state == ev->state) {
			bind->action(&arg);
		}
	}
}

void
event_configure_notify(xcb_generic_event_t const *a_event)
{
	xcb_configure_notify_event_t const *ev = (xcb_configure_notify_event_t
	    const *)a_event;
	struct Client *c;

	c = client_get(ev->window, NULL);
	if (ev->override_redirect && c) {
		client_delete(c);
	} else if (!c) {
		client_add(ev->window);
	}
}

void
event_configure_request(xcb_generic_event_t const *a_event)
{
	xcb_configure_request_event_t const *ev =
	    (xcb_configure_request_event_t const *)a_event;
	uint32_t *p;
	struct Client *c;
	uint16_t mask;
	int workspace;

	if ((c = client_get(ev->window, &workspace))) {
		if (XCB_CONFIG_WINDOW_X & ev->value_mask) {
			c->x = ev->x;
		}
		if (XCB_CONFIG_WINDOW_Y & ev->value_mask) {
			c->y = MAX(ev->y, g_font_height);
		}
		if ((XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y) &
		    ev->value_mask) {
			client_move(c, g_workspace_cur == workspace ?
			    VISIBLE : HIDDEN);
		}
		if (XCB_CONFIG_WINDOW_WIDTH & ev->value_mask) {
			c->width = ev->width;
		}
		if (XCB_CONFIG_WINDOW_HEIGHT & ev->value_mask) {
			c->height = ev->height;
		}
		if ((XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT) &
		    ev->value_mask) {
			client_resize(c, 0);
		}
		client_place(c);
	} else {
		p = g_values;
		mask = (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
		    XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT) &
		    ev->value_mask;
		if (XCB_CONFIG_WINDOW_X & mask) {
			*p++ = ev->x;
		}
		if (XCB_CONFIG_WINDOW_Y & mask) {
			*p++ = MAX(ev->y, g_font_height);
		}
		if (XCB_CONFIG_WINDOW_WIDTH & mask) {
			*p++ = ev->width;
		}
		if (XCB_CONFIG_WINDOW_HEIGHT & mask) {
			*p++ = ev->height;
		}
		xcb_configure_window(g_conn, ev->window, mask, g_values);
	}
}

void
event_destroy_notify(xcb_generic_event_t const *a_event)
{
	xcb_destroy_notify_event_t const *ev = (xcb_destroy_notify_event_t
	    const *)a_event;
	struct Client *c;

	if ((c = client_get(ev->window, NULL))) {
		client_delete(c);
	}
}

void
event_enter_notify(xcb_generic_event_t const *a_event)
{
	xcb_enter_notify_event_t const *ev = (xcb_enter_notify_event_t const
	    *)a_event;
	struct Client *c;
	int workspace;

	if ((c = client_get(ev->event, &workspace)) && g_workspace_cur ==
	    workspace) {
		client_focus(c, 1, 0);
	}
}

void
event_expose(xcb_generic_event_t const *a_event)
{
	xcb_expose_event_t const *ev = (xcb_expose_event_t const *)a_event;

	if (0 == ev->count && ev->window == g_bar) {
		bar_draw();
	}
}

void
event_handle(xcb_generic_event_t const *a_event)
{
	size_t i = XCB_EVENT_RESPONSE_TYPE(a_event);

	if (EVENT_MAX > i && g_event_handler[i]) {
		g_event_handler[i](a_event);
	} else {
	}
}

void
event_key_press(xcb_generic_event_t const *a_event)
{
	xcb_key_press_event_t const *ev = (xcb_key_press_event_t const
	    *)a_event;
	struct KeyBind *bind;
	size_t i;

	for (i = 0, bind = c_key_bind; LENGTH(c_key_bind) > i; ++i, ++bind) {
		if (bind->code == ev->detail && bind->state == ev->state) {
			bind->action(&bind->arg);
		}
	}
}

void
event_map_request(xcb_generic_event_t const *a_event)
{
	xcb_map_request_event_t const *ev = (xcb_map_request_event_t const
	    *)a_event;
	struct Client *c;

	xcb_map_window(g_conn, ev->window);
	if ((c = (c = client_get(ev->window, NULL)) ? c :
	    client_add(ev->window))) {
		client_focus(c, 1, 1);
	}
}

void
event_property_notify(xcb_generic_event_t const *a_event)
{
	xcb_property_notify_event_t const *ev = (xcb_property_notify_event_t
	    const *)a_event;

	if (ev->window == g_root) {
		if (XCB_ATOM_WM_NAME == ev->atom) {
			root_name_update();
			g_bar_redraw = 1;
		}
	} else {
		struct Client *c;

		if ((c = client_get(ev->window, NULL))) {
			if (XCB_ATOM_WM_HINTS == ev->atom) {
				xcb_icccm_wm_hints_t hints;

				if (xcb_icccm_get_wm_hints_reply(g_conn,
				    xcb_icccm_get_wm_hints(g_conn,
				    ev->window), &hints, NULL)) {
					if ((XCB_ICCCM_WM_HINT_X_URGENCY &
					    hints.flags) && g_focus != c) {
						c->is_urgent = 1;
						g_bar_redraw = 1;
					}
				}
			} else if (XCB_ATOM_WM_NORMAL_HINTS == ev->atom) {
				xcb_icccm_get_wm_normal_hints_reply(g_conn,
				    xcb_icccm_get_wm_normal_hints(g_conn,
				    c->window), &c->hints, NULL);
			} else if (g_NET_WM_NAME == ev->atom ||
			    XCB_ATOM_WM_NAME == ev->atom) {
				client_name_update(c);
				g_bar_redraw = 1;
			}
		}
	}
}

void
event_unmap_notify(xcb_generic_event_t const *a_event)
{
	struct Client *c;

	if ((c = client_get(((xcb_unmap_notify_event_t const
	    *)a_event)->window, NULL))) {
		client_delete(c);
	}
}

void
root_name_update(void)
{
	xcb_icccm_get_text_property_reply_t icccm;

	if (xcb_icccm_get_wm_name_reply(g_conn, xcb_icccm_get_wm_name(g_conn,
	    g_root), &icccm, NULL)) {
		char const *p = icccm.name;
		size_t len = icccm.name_len;

		if ((g_root_is_urgent = ('!' == icccm.name[0]))) {
			++p;
			--len;
		}
		string_convert(&g_root_name, p, len);
		xcb_icccm_get_text_property_reply_wipe(&icccm);
	} else {
		string_convert(&g_root_name, "<hwm>", 5);
	}
}

void
string_convert(struct String *a_out, char const *a_in, size_t a_inlen)
{
	char *p = (char *)a_out->str;
	size_t len = NAME_LEN;

	iconv(g_iconv, (char **)&a_in, &a_inlen, (char **)&p, &len);
	a_out->length = (NAME_LEN - len) / 2;
}

int
text_draw(struct String const *a_text, int a_is_focused, int a_is_urgent, int
    a_x, int a_y)
{
	xcb_rectangle_t rect;
	uint32_t bg = a_is_urgent ? (g_blink ? g_color_urgent2_bg :
	    g_color_urgent1_bg) : a_is_focused ? g_color_bar_fg :
	    g_color_bar_bg;
	uint32_t fg = a_is_urgent ? (g_blink ? g_color_urgent2_fg :
	    g_color_urgent1_fg) : a_is_focused ? g_color_bar_bg :
	    g_color_bar_fg;
	int width;

	width = text_width(a_text);
	rect.x = a_x;
	rect.y = a_y;
	rect.width = width;
	rect.height = g_font_height;
	xcb_change_gc(g_conn, g_gc, XCB_GC_FOREGROUND, &bg);
	xcb_poly_fill_rectangle(g_conn, g_pixmap, g_gc, 1, &rect);

	xcb_change_gc(g_conn, g_gc, XCB_GC_BACKGROUND, &bg);
	xcb_change_gc(g_conn, g_gc, XCB_GC_FOREGROUND, &fg);
	xcb_image_text_16(g_conn, a_text->length, g_pixmap, g_gc, a_x +
	    TEXT_PADDING, a_y + g_font_ascent, (xcb_char2b_t const
	    *)a_text->str);

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
	width = reply->overall_width;
	free(reply);
	return TEXT_PADDING + width + TEXT_PADDING;
}

int
main()
{
	xcb_font_t cursor_font;
	xcb_screen_iterator_t it;
	struct pollfd fds;
	FILE *file;
	xcb_query_font_reply_t *font_reply;
	xcb_query_tree_reply_t *tree_reply;
	struct KeyBind const *bind;
	size_t i;
	int screen_no, error;

	/* Basic setup. */
	if ((iconv_t)-1 == (g_iconv = iconv_open("UCS-2BE", "UTF8"))) {
		die("Could not open iconv(UCS-2BE, UTF8).");
	}
	for (i = 0; WORKSPACE_NUM > i; ++i) {
		string_convert(&g_workspace_label[i], c_workspace_label[i],
		    strlen(c_workspace_label[i]));
	}

	if (!(g_conn = xcb_connect(NULL, &screen_no))) {
		die("NULL X11 connection.");
	}
	if ((error = xcb_connection_has_error(g_conn))) {
		die("X11 connection error=%d.", error);
	}
	for (it = xcb_setup_roots_iterator(xcb_get_setup(g_conn)); 0 <
	    screen_no--; xcb_screen_next(&it))
		;
	if (!(g_screen = it.data)) {
		die("Could not get current screen.");
	}
	g_width = g_screen->width_in_pixels;
	g_height = g_screen->height_in_pixels;
	g_root = g_screen->root;

	g_event_handler[XCB_KEY_PRESS] = event_key_press;
	g_event_handler[XCB_BUTTON_PRESS] = event_button_press;
	g_event_handler[XCB_ENTER_NOTIFY] = event_enter_notify;
	g_event_handler[XCB_EXPOSE] = event_expose;
	g_event_handler[XCB_DESTROY_NOTIFY] = event_destroy_notify;
	g_event_handler[XCB_UNMAP_NOTIFY] = event_unmap_notify;
	g_event_handler[XCB_MAP_REQUEST] = event_map_request;
	g_event_handler[XCB_CONFIGURE_NOTIFY] = event_configure_notify;
	g_event_handler[XCB_CONFIGURE_REQUEST] = event_configure_request;
	g_event_handler[XCB_PROPERTY_NOTIFY] = event_property_notify;

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

	/* Atoms. */
	g_WM_DELETE_WINDOW = atom_get("WM_DELETE_WINDOW");
	g_WM_PROTOCOLS = atom_get("WM_PROTOCOLS");
	g_NET_WM_NAME = atom_get("_NET_WM_NAME");

	/* Graphics. */
	g_font = xcb_generate_id(g_conn);
	xcb_open_font(g_conn, g_font, 5, FONT_FACE);
	if (!(font_reply = xcb_query_font_reply(g_conn, xcb_query_font(g_conn,
	    g_font), NULL))) {
		die("Could not load font face '%s'.", FONT_FACE);
	}
	g_font_ascent = font_reply->font_ascent;
	g_font_height = g_font_ascent + font_reply->font_descent;
	free(font_reply);

	g_gc = xcb_generate_id(g_conn);
	xcb_create_gc(g_conn, g_gc, g_root, 0, NULL);

	g_pixmap = xcb_generate_id(g_conn);
	xcb_create_pixmap(g_conn, g_screen->root_depth, g_pixmap, g_root,
	    g_width, g_font_height);

	/* Configs. */
	g_color_border_focus = color_get(BORDER_FOCUS);
	g_color_border_unfocus = color_get(BORDER_UNFOCUS);
	g_color_bar_bg = color_get(BAR_BG);
	g_color_bar_fg = color_get(BAR_FG);
	g_color_urgent1_bg = color_get(URGENT1_BG);
	g_color_urgent1_fg = color_get(URGENT1_FG);
	g_color_urgent2_bg = color_get(URGENT2_BG);
	g_color_urgent2_fg = color_get(URGENT2_FG);

	for (i = 0, bind = c_key_bind; LENGTH(c_key_bind) > i; ++i, ++bind) {
		xcb_grab_key(g_conn, 1, g_root, bind->state, bind->code,
		    XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
	}

	for (i = 0; WORKSPACE_NUM > i; ++i) {
		TAILQ_INIT(&g_client_list[i]);
	}

	/* Furnish existing windows, and reuse persisted info. */
	if ((tree_reply = xcb_query_tree_reply(g_conn, xcb_query_tree(g_conn,
	    g_root), NULL))) {
		char line[80], *p;
		int j[10];
		xcb_window_t *w;
		size_t num;

		w = xcb_query_tree_children(tree_reply);
		num = xcb_query_tree_children_length(tree_reply);
		if ((file = fopen(PERSIST_FILE, "rb"))) {
			while (fgets(line, 80, file)) {
				for (p = strtok(line, " "), i = 0; p && 10 >
				    i; p = strtok(NULL, " "), ++i) {
					j[i] = strtol(p, NULL, 10);
				}
				if (10 == i) {
					for (i = 0; num > i; ++i) {
						if (j[0] == (int)w[i]) {
							client_add_details(
							    w[i], j + 1);
							w[i] = 0;
							break;
						}
					}
				}
			}
			fclose(file);
		}
		for (i = 0; num > i; ++i) {
			if (w[i] && !client_get(w[i], NULL)) {
				client_add(w[i]);
			}
		}
		free(tree_reply);
		action_furnish(NULL);
	}
	client_focus(NULL, 1, 1);

	/* Bar. */
	g_bar = xcb_generate_id(g_conn);
	g_values[0] = g_color_bar_bg;
	g_values[1] = 1;
	g_values[2] = XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_EXPOSURE;
	xcb_create_window(g_conn, XCB_COPY_FROM_PARENT, g_bar, g_root, 0, 0,
	    g_width, g_font_height, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
	    XCB_COPY_FROM_PARENT, XCB_CW_BACK_PIXEL | XCB_CW_OVERRIDE_REDIRECT
	    | XCB_CW_EVENT_MASK, g_values);
	xcb_map_window(g_conn, g_bar);
	root_name_update();
	bar_draw();

	xcb_flush(g_conn);

	/* Main loop. */
	fds.fd = xcb_get_file_descriptor(g_conn);
	fds.events = POLLIN;
	g_has_urgent = 0;
	g_timeout = TIMEOUT_NORMAL;
	for (g_run = RUN_LOOP; RUN_LOOP == g_run;) {
		xcb_generic_event_t *ev;

		poll(&fds, 1, g_timeout);
		g_bar_redraw = 0;
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
			g_timeout = MIN(g_timeout - dt, TIMEOUT_BLINK);
			if (0 >= g_timeout) {
				g_timeout = TIMEOUT_BLINK;
				g_blink ^= 1;
				g_bar_redraw = 1;
			}
		} else {
			g_timeout = TIMEOUT_NORMAL;
			g_blink = 0;
		}
		if (g_bar_redraw) {
			bar_draw();
			xcb_flush(g_conn);
		}
	}

	/* Quit. */
	xcb_destroy_window(g_conn, g_bar);
	if (RUN_RESTART == g_run) {
		file = fopen(PERSIST_FILE, "wb");
		for (i = 0; WORKSPACE_NUM > i; ++i) {
			while (!TAILQ_EMPTY(&g_client_list[i])) {
				struct Client *c;

				c = TAILQ_FIRST(&g_client_list[i]);
				fprintf(file, "%d %d %d %d %d %d %d %d %d "
				    "%d\n", c->window, (int)i, c->is_urgent,
				    c->x, c->y, c->maximize, c->max_old_x,
				    c->max_old_y, c->max_old_width,
				    c->max_old_height);
				TAILQ_REMOVE(&g_client_list[i], c, next);
				client_delete(c);
			}
		}
		fclose(file);
	} else {
		remove(PERSIST_FILE);
	}
	xcb_flush(g_conn);
	xcb_disconnect(g_conn);

	iconv_close(g_iconv);

	return RUN_RESTART == g_run;
}
