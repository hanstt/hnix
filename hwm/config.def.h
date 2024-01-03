/*
 * Copyright (c) 2015, 2024
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

static char const	c_bar_bg[] = "black";
static char const	c_bar_fg[] = "white";
static char const	c_border_focus[] = "red";
static char const	c_border_unfocus[] = "blue";
static char const	c_font_face[] = "fixed";
static char const	c_persist_file[] = "/tmp/hwm.txt";
static int const	c_snap_margin = 6;
static int const	c_text_padding = 4;
static int const	c_timeout_normal = 3000;
static int const	c_timeout_blink = 200;
static char const	c_urgent1_bg[] = "red";
static char const	c_urgent1_fg[] = "white";
static char const	c_urgent2_bg[] = "yellow";
static char const	c_urgent2_fg[] = "black";
static char const	*c_workspace_label[] = {
	"1", "2", "3", "4", "5", "6"
};
static char const	*c_app_dmenu[] = { "dmenu_run", "-i", "-fn",
	c_font_face, "-nb", c_bar_bg, "-nf", c_bar_fg, "-sb", c_bar_fg, "-sf",
	c_bar_bg, NULL};
static char const	*c_app_term[] = {"uxterm", NULL};

#define	MOD_MASK1 XCB_MOD_MASK_4
#define	MOD_MASK2 (XCB_MOD_MASK_1 | XCB_MOD_MASK_4)
#define	MOD_MASK3 (XCB_MOD_MASK_SHIFT | XCB_MOD_MASK_4)
#define BIND_WORKSPACE(keysym, id) \
	{keysym, MOD_MASK1, action_workspace_select, {id, NULL}},\
	{keysym, MOD_MASK2, action_client_relocate, {id, NULL}}
#define BIND_CLIENT_DIR(keysym, dir) \
	{keysym, MOD_MASK1, action_client_jump, {dir, NULL}},\
	{keysym, MOD_MASK2, action_client_expand, {dir, NULL}},\
	{keysym, MOD_MASK3, action_client_grow, {dir, NULL}}
static struct KeyBind const c_key_bind[] = {
	{XK_q, MOD_MASK2, action_quit, {RUN_QUIT, NULL}},
	{XK_a, MOD_MASK2, action_quit, {RUN_RESTART, NULL}},
	{XK_x, MOD_MASK2, action_kill, {0, NULL}},
	BIND_WORKSPACE(XK_w, 0),
	BIND_WORKSPACE(XK_f, 1),
	BIND_WORKSPACE(XK_p, 2),
	BIND_WORKSPACE(XK_r, 3),
	BIND_WORKSPACE(XK_s, 4),
	BIND_WORKSPACE(XK_t, 5),
	BIND_CLIENT_DIR(XK_Up, DIR_NORTH),
	BIND_CLIENT_DIR(XK_Left, DIR_WEST),
	BIND_CLIENT_DIR(XK_Right, DIR_EAST),
	BIND_CLIENT_DIR(XK_Down, DIR_SOUTH),
	{XK_space, MOD_MASK1, action_client_maximize, {MAX_BOTH, NULL}},
	{XK_space, MOD_MASK2, action_client_maximize, {MAX_VERT, NULL}},
	{XK_Tab, MOD_MASK1, action_client_browse, {0, NULL}},
	{XK_m, MOD_MASK1, action_furnish, {0, NULL}},
	{XK_a, MOD_MASK1, action_exec, {0, c_app_term}},
	{XK_g, MOD_MASK1, action_exec, {0, c_app_dmenu}}
};
static struct ButtonBind const c_button_bind[] = {
	{CLICK_WORKSPACE, 1, 0, action_workspace_select},
	{CLICK_CLIENT, 1, MOD_MASK1, action_client_move},
	{CLICK_CLIENT, 3, MOD_MASK1, action_client_resize}
};
