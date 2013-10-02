#define KEYCODE 0xFFFFFFFF

#define WORKSPACEKEYS(KEY, WORKSPACE) \
	{ Mod4Mask,		KEY,	view,		{.i = WORKSPACE} }, \
	{ Mod4Mask|ShiftMask,	KEY,	workspaceTo,	{.i = WORKSPACE} }

static int const BORDER_WIDTH = 1;
static char const font[] = "-*-terminus-*-*-*-*-12-*-*-*-*-*-*-*";
static char const normbordercolor[] = "#000080";
static char const normbgcolor[] = "#000000";
static char const normfgcolor[] = "#ffffff";
static char const selbordercolor[] = "#ff0000";
static char const selbgcolor[] = "#ffffff";
static char const selfgcolor[] = "#000000";
static char const warn1bgcolor[] = "#ff0000";
static char const warn1fgcolor[] = "#ffffff";
static char const warn2bgcolor[] = "#ffff00";
static char const warn2fgcolor[] = "#000000";
static int const SNAP = 10;
static Bool topbar = True;
static int const WARN_RATE_MS = 200;
static int const WARN_WAIT_S = 5;
static char const workspace_label[][WORKSPACE_NAME_MAX] = {"1", "2", "3", "4",
	"5", "6"};
static int workspaceCur = 0;

static char const *cmd_dmenu[] = { "dmenu_run", "-i", "-fn", font, "-nb",
	normbgcolor, "-nf", normfgcolor, "-sb", selbgcolor, "-sf", selfgcolor,
	NULL };
static char const *cmd_term[] = { "uxterm", NULL };

static struct Key const keys[] = {
	{ControlMask | Mod1Mask,XK_r,		restart,	{0}},
	{ControlMask | Mod1Mask,XK_q,		quit,		{0}},
	{ControlMask | Mod4Mask,XK_x,		killclient,	{0}},
	WORKSPACEKEYS(		XK_1,				0),
	WORKSPACEKEYS(		XK_2,				1),
	WORKSPACEKEYS(		XK_3,				2),
	WORKSPACEKEYS(		XK_4,				3),
	WORKSPACEKEYS(		XK_5,				4),
	WORKSPACEKEYS(		XK_6,				5),
	{Mod4Mask,		XK_h,		viewStep,	{.i = -1}},
	{Mod4Mask,		XK_l,		viewStep,	{.i = +1}},
	{Mod4Mask,		XK_Tab,		nextfocus,	{.i = +1}},
	{Mod4Mask,		XK_a,		spawn,		{.v = cmd_term}},
	{Mod4Mask,		XK_p,		spawn,		{.v = cmd_dmenu}},
	{Mod4Mask,		XK_k,		nextlang,	{.i = +1}},
	{Mod4Mask,		XK_m,		furnish,	{0}},
	{Mod1Mask | Mod4Mask,	XK_Left,	keymove,	{.i = 0}},
	{Mod1Mask | Mod4Mask,	XK_Up,		keymove,	{.i = 1}},
	{Mod1Mask | Mod4Mask,	XK_Right,	keymove,	{.i = 2}},
	{Mod1Mask | Mod4Mask,	XK_Down,	keymove,	{.i = 3}},
	{Mod4Mask,		XK_s,		nextstatus,	{0}},
	{Mod4Mask,		XK_space,	zoom,		{.i = 1}},
	{Mod1Mask | Mod4Mask,	XK_space,	zoom,		{.i = 2}},
};

static struct Button const buttons[] = {
	{CLK_CLIENT,	Mod4Mask,	Button1,	mousemove,	{0}},
	{CLK_CLIENT,	Mod4Mask,	Button3,	mouseresize,	{0}},
	{CLK_STATUS,	0,		Button1,	nextstatus,	{0}},
	{CLK_WORKSPACE,	0,		Button1,	view,		{0}},
	{CLK_WORKSPACE,	0,		Button4,	viewStep,	{.i = +1}},
	{CLK_WORKSPACE,	0,		Button5,	viewStep,	{.i = -1}},
	{CLK_WORKSPACE,	ShiftMask,	Button1,	workspaceTo,	{0}},
};
