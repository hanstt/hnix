#define KEYCODE 0xFFFFFFFF

#define WORKSPACEKEYS(KEY, WORKSPACE) \
	{ Mod4Mask,		KEY,	view,		{.i = WORKSPACE} }, \
	{ Mod4Mask|ShiftMask,	KEY,	toworkspace,	{.i = WORKSPACE} }

static int borderpx = 1;
static const char font[] = "-*-fixed-*-*-*-*-7-*-*-*-*-*-*-*";
static const char normbordercolor[] = "#000000";
static const char normbgcolor[] = "#c0c0c0";
static const char normfgcolor[] = "#000000";
static const char selbordercolor[] = "#ffffff";
static const char selbgcolor[] = "#008000";
static const char selfgcolor[] = "#000000";
static const char warn1bgcolor[] = "#ff0000";
static const char warn1fgcolor[] = "#ffffff";
static const char warn2bgcolor[] = "#ffff00";
static const char warn2fgcolor[] = "#000000";
static int snap = 10;
static Bool topbar = True;
static int warn_rate_ms = 200;
static int warn_wait_s = 5;
static const char workspace[][WORKSPACE_NAME_MAX] = {"1", "2", "3", "4", "5", "6"};
static int workspaceCur = 0;

static const char *cmd_dmenu[] = { "dmenu_run", "-i", "-fn", font, "-nb", normbgcolor, "-nf", normfgcolor, "-sb", selbgcolor, "-sf", selfgcolor, NULL };
static const char *cmd_term[] = { "uxterm", "-kt", "vt220", NULL };

static struct Key keys[] = {
	{ControlMask | Mod4Mask,XK_r,		restart,	{0}},
	{ControlMask | Mod4Mask,XK_q,		quit,		{0}},
	{ControlMask | Mod4Mask,XK_x,		killclient,	{0}},
	WORKSPACEKEYS(		XK_1,				0),
	WORKSPACEKEYS(		XK_2,				1),
	WORKSPACEKEYS(		XK_3,				2),
	WORKSPACEKEYS(		XK_4,				3),
	WORKSPACEKEYS(		XK_5,				4),
	WORKSPACEKEYS(		XK_6,				5),
	{Mod4Mask,		XK_Left,	viewstep,	{.i = -1}},
	{Mod4Mask,		XK_Right,	viewstep,	{.i = +1}},
	{Mod4Mask,		XK_Tab,		nextfocus,	{.i = +1}},
	{Mod4Mask,		XK_a,		spawn,		{.v = cmd_term}},
	{Mod4Mask,		XK_p,		spawn,		{.v = cmd_dmenu}},
	{Mod4Mask,		XK_l,		nextlang,	{.i = +1}},
	{Mod4Mask,		XK_m,		furnish,	{0}},
	{Mod1Mask | Mod4Mask,	XK_Left,	keymove,	{.i = 0}},
	{Mod1Mask | Mod4Mask,	XK_Up,		keymove,	{.i = 1}},
	{Mod1Mask | Mod4Mask,	XK_Right,	keymove,	{.i = 2}},
	{Mod1Mask | Mod4Mask,	XK_Down,	keymove,	{.i = 3}},
	{Mod4Mask,		XK_s,		nextstatus,	{0}},
	{Mod4Mask,		XK_space,	zoom,		{.i = 0}},
	{Mod1Mask | Mod4Mask,	XK_space,	zoom,		{.i = 1}},
};

static struct Button buttons[] = {
	{CLK_CLIENT,	Mod4Mask,	Button1,	mousemove,	{0}},
	{CLK_CLIENT,	Mod4Mask,	Button3,	mouseresize,	{0}},
	{CLK_STATUS,	0,		Button1,	nextstatus,	{0}},
	{CLK_WORKSPACE,	0,		Button1,	view,		{0}},
	{CLK_WORKSPACE,	0,		Button4,	viewstep,	{.i = +1}},
	{CLK_WORKSPACE,	0,		Button5,	viewstep,	{.i = -1}},
	{CLK_WORKSPACE,	ShiftMask,	Button1,	toworkspace,	{0}},
};
