#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/XF86keysym.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#ifdef XKB
#include <X11/XKBlib.h>
#endif

#define BUTTONMASK (ButtonPressMask | ButtonReleaseMask)
#define CLEANMASK(mask) (mask & ~(numlockmask | LockMask))
#define INRECT(X,Y,RX,RY,RW,RH) ((X) >= (RX) && (X) < (RX) + (RW) && (Y) >= (RY) && (Y) < (RY) + (RH))
#define LENGTH(x) (sizeof x / sizeof x[0])
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define WORKSPACE_NAME_MAX 16
#define MOUSEMASK (BUTTONMASK | PointerMotionMask)
#define WIDTH(c) ((c)->w + 2 * (c)->bw)
#define HEIGHT(c) ((c)->h + 2 * (c)->bw)
#define TEXTW(x) (textnw(x, strlen(x)) + dc.font.height)
#define WORKSPACE_NUM LENGTH(workspace)

enum {
	CUR_NORMAL,
	CUR_RESIZE,
	CUR_MOVE,
	CUR_LAST
};
enum {
	COL_BG,
	COL_BORDER,
	COL_FG,
	COL_LAST
};
enum {
	NET_SUPPORTED,
	NET_WM_NAME,
	NET_LAST
};
enum {
	WM_DELETE,
	WM_PROTOCOLS,
	WM_STATE,
	WM_LAST
};
enum {
	CLK_CLIENT,
	CLK_ROOT,
	CLK_STATUS,
	CLK_TITLE,
	CLK_WORKSPACE,
	CLK_LAST
};

union Arg {
	int	i;
	float	f;
	void	*v;
};
struct Button {
	int	click;
	int	mask;
	int	button;
	void	(*func)(const union Arg *arg);
	const	union Arg arg;
};
struct Client {
	char	name[256];
	float	mina, maxa;
	int	x, y, w, h;
	int	basew, baseh, incw, inch, maxw, maxh, minw, minh;
	int	bw, oldbw;
	int	workspace;
	Bool	isurgent;
	struct	Client *prev;
	struct	Client *next;
	Window	win;
#ifdef XKB
	int	xkb_grpIdx;
#endif
	int	zoomed;
	int	zux, zuy, zuw, zuh;
};
struct DC {
	int	x, y, w, h;
	long	norm[COL_LAST];
	long	sel[COL_LAST];
	long	warn[2][COL_LAST];
	Drawable	drawable;
	GC	gc;
	struct {
		int	ascent;
		int	descent;
		int	height;
		XFontSet	set;
		XFontStruct	*xfont;
	} font;
};
struct Key {
	int	mod;
	KeySym	keysym;
	void	(*func)(const union Arg *);
	const	union Arg arg;
};

static void		 arrange(void);
static void		 attach(struct Client*);
static void		 buttonpress(XEvent*);
static void		 checkotherwm(void);
static void		 cleanup(void);
static void		 clearurgent(struct Client*);
static void		 configure(struct Client*);
static void		 configurenotify(XEvent*);
static void		 configurerequest(XEvent*);
static void		 destroynotify(XEvent*);
static void		 detach(struct Client*);
static void		 die(const char*, ...);
static void		 drawbar(void);
static void		 drawsquare(Bool, Bool, Bool, long [COL_LAST]);
static void		 drawtext(const char*, long [COL_LAST], Bool);
static void		 enternotify(XEvent*);
static void		 expose(XEvent*);
static void		 focus(struct Client*);
static void		 focusin(XEvent*);
static void		 focusset(void);
static void 		 furnish(const union Arg*);
static struct Client	*getclient(Window);
static long		 getcolor(const char*);
static long		 getstate(Window ww);
static Bool		 gettextprop(Window, Atom, char*, int);
static void		 grabbuttons(struct Client*, Bool);
static void		 grabkeys(void);
static void		 initfont(const char*);
static Bool		 isprotodel(struct Client*);
static void		 keymove(const union Arg*);
static void		 keypress(XEvent*);
static void		 keyrelease(XEvent*);
static void		 killclient(const union Arg*);
static void		 manage(Window, XWindowAttributes*);
static void		 mappingnotify(XEvent*);
static void		 maprequest(XEvent*);
static void		 mousemove(const union Arg*);
static void		 mouseresize(const union Arg*);
static void		 nextfocus(const union Arg*);
static void		 nextlang(const union Arg*);
static void		 nextstatus(const union Arg*);
static void		 place(struct Client*);
static void		 propertynotify(XEvent*);
static void		 quit(const union Arg*);
static void		 raisesel(void);
static void		 resize(struct Client*, int, int, int, int, Bool);
static void		 restart(const union Arg*);
static void		 run(void);
static void		 scan(void);
static void		 setclientstate(struct Client*, long);
static void		 setup(void);
static void		 sigchld(int);
static void		 snapclient(struct Client*, int*, int*, int, int, int, int);
static void		 spawn(const union Arg*);
static int		 textnw(const char*, int);
static void		 toworkspace(const union Arg*);
static void		 unmanage(struct Client*);
static void		 unmapnotify(XEvent*);
static void		 updatebar(void);
static void		 updategeom(void);
static void		 updatenumlockmask(void);
static void		 updatesizehints(struct Client*);
static void		 updatestatus(void);
static void		 updatetitle(struct Client*);
static void		 updatewmhints(struct Client*);
static void		 view(const union Arg*);
static void		 viewstep(const union Arg*);
static int		 xerror(Display*, XErrorEvent*);
static int		 xerrordummy(Display*, XErrorEvent*);
static int		 xerrorstart(Display*, XErrorEvent*);
static void		 zoom(const union Arg*);

#include "config.h"

#ifdef XKB
static int xkb_event;
static int xkb_grpNum = -1;
static char xkb_grpName[XkbNumKbdGroups][32];
#endif
static int swarn = 0;
static int sidx = 0;
static char stext[256];
static int connection;
static int screen;
static int sx, sy, sw, sh;
static int by;
static int wx, wy, ww, wh;
static int (*xerrorxlib)(Display*, XErrorEvent*);
static int numlockmask = 0;
static void (*handler[LASTEvent])(XEvent*) = {
	[ButtonPress] = buttonpress,
	[ConfigureNotify] = configurenotify,
	[ConfigureRequest] = configurerequest,
	[DestroyNotify] = destroynotify,
	[EnterNotify] = enternotify,
	[Expose] = expose,
	[FocusIn] = focusin,
	[KeyPress] = keypress,
	[KeyRelease] = keyrelease,
	[MappingNotify] = mappingnotify,
	[MapRequest] = maprequest,
	[PropertyNotify] = propertynotify,
	[UnmapNotify] = unmapnotify,
};
static Atom wmatom[WM_LAST], netatom[NET_LAST];
static Bool otherwm;
static Bool running = True;
static Bool tempFocus = False;
static struct Client *sel;
static Cursor cursor[CUR_LAST];
static Display *dpy;
static struct DC dc;
static Window root, barwin;
static struct Client *stack[WORKSPACE_NUM];

void
arrange(void)
{
	struct Client *c;
	int i;

	for (i = 0; i < WORKSPACE_NUM; i++)
		for (c = stack[i]; c != NULL; c = c->next)
			if (i == workspaceCur) {
				XMoveWindow(dpy, c->win, c->x, c->y);
				resize(c, c->x, c->y, c->w, c->h, True);
			} else
				XMoveWindow(dpy, c->win, c->x + 2 * sw, c->y);
}

void
attach(struct Client *c)
{
	c->prev = NULL;
	c->next = stack[c->workspace];
	if (stack[c->workspace] != NULL)
		stack[c->workspace]->prev = c;
	stack[c->workspace] = c;
}

void
buttonpress(XEvent *e)
{
	int i, click;
	int x;
	union Arg arg = {0};
	struct Client *c;
	XButtonPressedEvent *ev = &e->xbutton;

	click = CLK_ROOT;
	if (ev->window == barwin) {
		i = x = 0;
		do x += TEXTW(workspace[i]); while (ev->x >= x && ++i < WORKSPACE_NUM);
		if (i < WORKSPACE_NUM) {
			click = CLK_WORKSPACE;
			arg.i = i;
		} else if (ev->x > wx + ww - TEXTW(stext))
			click = CLK_STATUS;
		else
			click = CLK_TITLE;
	} else if ((c = getclient(ev->window))) {
		focus(c);
		click = CLK_CLIENT;
	}

	for (i = 0; i < LENGTH(buttons); i++)
		if (click == buttons[i].click
		    && CLEANMASK(ev->state) == CLEANMASK(buttons[i].mask)
		    && ev->button == buttons[i].button)
			buttons[i].func(click == CLK_WORKSPACE && buttons[i].arg.i == 0 ? &arg : &buttons[i].arg);
}

void
checkotherwm(void)
{
	otherwm = False;
	xerrorxlib = XSetErrorHandler(xerrorstart);
	XSelectInput(dpy, DefaultRootWindow(dpy), SubstructureRedirectMask);
	XSync(dpy, False);
	if (otherwm)
		die("dwm: Another window manager is already running.\n");
	XSetErrorHandler(xerror);
	XSync(dpy, False);
}

void
cleanup(void)
{
	int i;

	for (i = 0; i < WORKSPACE_NUM; i++)
		while (stack[i] != NULL)
			unmanage(stack[i]);
	if (dc.font.set)
		XFreeFontSet(dpy, dc.font.set);
	else
		XFreeFont(dpy, dc.font.xfont);
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	XFreePixmap(dpy, dc.drawable);
	XFreeGC(dpy, dc.gc);
	XFreeCursor(dpy, cursor[CUR_NORMAL]);
	XFreeCursor(dpy, cursor[CUR_RESIZE]);
	XFreeCursor(dpy, cursor[CUR_MOVE]);
	XDestroyWindow(dpy, barwin);
	XSync(dpy, False);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
}

void
clearurgent(struct Client *c)
{
	XWMHints *wmh;

	c->isurgent = False;
	if ((wmh = XGetWMHints(dpy, c->win)) == NULL)
		return;
	wmh->flags &= ~XUrgencyHint;
	XSetWMHints(dpy, c->win, wmh);
	XFree(wmh);
}

void
configure(struct Client *c)
{
	XConfigureEvent ce;

	ce.type = ConfigureNotify;
	ce.display = dpy;
	ce.event = c->win;
	ce.window = c->win;
	ce.x = c->x;
	ce.y = c->y;
	ce.width = c->w;
	ce.height = c->h;
	ce.border_width = c->bw;
	ce.above = None;
	ce.override_redirect = False;
	XSendEvent(dpy, c->win, False, StructureNotifyMask, (XEvent*)&ce);
}

void
configurenotify(XEvent *e)
{
	XConfigureEvent *ev = &e->xconfigure;

	if (ev->window == root && (ev->width != sw || ev->height != sh)) {
		sw = ev->width;
		sh = ev->height;
		updategeom();
		updatebar();
		arrange();
	}
}

void
configurerequest(XEvent *e)
{
	struct Client *c;
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;

	if ((c = getclient(ev->window))) {
		if (ev->value_mask & CWBorderWidth)
			c->bw = ev->border_width;
		else {
			if (ev->value_mask & CWX)
				c->x = sx + ev->x;
			if (ev->value_mask & CWY)
				c->y = sy + ev->y;
			if (ev->value_mask & CWWidth)
				c->w = ev->width;
			if (ev->value_mask & CWHeight)
				c->h = ev->height;
			if (c->x + WIDTH(c) < sx)
				c->x = sx - WIDTH(c);
			if (c->x > sx + sw)
				c->x = sx + sw - 1;
			if (c->y + HEIGHT(c) < sy)
				c->y = sy - HEIGHT(c);
			if (c->y > sy + sh)
				c->y = sy + sh - 1;
			if ((ev->value_mask & (CWX | CWY)) && !(ev->value_mask & (CWWidth | CWHeight)))
				configure(c);
			if (c->workspace == workspaceCur)
				XMoveResizeWindow(dpy, c->win, c->x, c->y, c->w, c->h);
		}
	} else {
		wc.x = ev->x;
		wc.y = ev->y;
		wc.width = ev->width;
		wc.height = ev->height;
		wc.border_width = ev->border_width;
		wc.sibling = ev->above;
		wc.stack_mode = ev->detail;
		XConfigureWindow(dpy, ev->window, ev->value_mask, &wc);
	}
	XSync(dpy, False);
}

void
destroynotify(XEvent *e)
{
	struct Client *c;
	XDestroyWindowEvent *ev = &e->xdestroywindow;

	if ((c = getclient(ev->window)) != NULL)
		unmanage(c);
}

void
detach(struct Client *c)
{
	if (c->prev != NULL)
		c->prev->next = c->next;
	else
		stack[c->workspace] = c->next;
	if (c->next != NULL)
		c->next->prev = c->prev;
}

void
die(const char *errstr, ...)
{
	va_list ap;

	va_start(ap, errstr);
	vfprintf(stderr, errstr, ap);
	va_end(ap);
	exit(EXIT_FAILURE);
}

void
drawbar(void)
{
	struct Client *c;
	long *col;
	char *s;
	int list_x, stat_x;
	int i, occ = 0, urg = 0;

	XSetForeground(dpy, dc.gc, dc.norm[COL_BG]);
	XFillRectangle(dpy, dc.drawable, dc.gc, 0, 0, sw, dc.h);

	for (i = 0; i < WORKSPACE_NUM; i++) {
		if (stack[i] != NULL)
			occ |= 1 << i;
		for (c = stack[i]; c != NULL; c = c->next)
			if (c->isurgent)
				urg |= 1 << i;
	}

	dc.x = 0;
	for (i = 0; i < WORKSPACE_NUM; i++) {
		dc.w = TEXTW(workspace[i]);
		col = i == workspaceCur ? dc.sel : dc.norm;
		drawtext(workspace[i], col, urg & 1 << i);
		drawsquare(sel != NULL && i == sel->workspace, occ & 1 << i, urg & 1 << i, col);
		dc.x += dc.w;
	}
#ifdef XKB
	s = xkb_grpName[sel != NULL ? sel->xkb_grpIdx : 0];
	dc.w = TEXTW(s);
	drawtext(s, dc.norm, False);
	dc.x += dc.w;
#endif
	list_x = dc.x;

	dc.w = TEXTW(stext);
	if (list_x < ww - dc.w)
		dc.x = ww - dc.w;
	else {
		dc.x = list_x;
		dc.w = ww - list_x;
	}
	stat_x = dc.x;
	drawtext(stext, swarn > 0 ? dc.warn[swarn - 1] : dc.norm, False);
	for (c = stack[workspaceCur], dc.x = list_x; c != NULL && dc.x < stat_x; c = c->next, dc.x += dc.w) {
		dc.w = TEXTW(c->name);
		if (dc.x + dc.w > stat_x)
			dc.w = stat_x - dc.x;
		drawtext(c->name, c == sel ? dc.sel : dc.norm, False);
	}
	XCopyArea(dpy, dc.drawable, barwin, dc.gc, 0, 0, ww, dc.h, 0, 0);
	XSync(dpy, False);
}

void
drawsquare(Bool filled, Bool empty, Bool invert, long col[COL_LAST])
{
	int x;
	XGCValues gcv;
	XRectangle r = { dc.x, dc.y, dc.w, dc.h };

	gcv.foreground = col[invert ? COL_BG : COL_FG];
	XChangeGC(dpy, dc.gc, GCForeground, &gcv);
	x = (dc.font.ascent + dc.font.descent + 2) / 4;
	r.x = dc.x + 1;
	r.y = dc.y + 1;
	if (filled) {
		r.width = r.height = x + 1;
		XFillRectangles(dpy, dc.drawable, dc.gc, &r, 1);
	} else if (empty) {
		r.width = r.height = x;
		XDrawRectangles(dpy, dc.drawable, dc.gc, &r, 1);
	}
}

void
drawtext(const char *text, long col[COL_LAST], Bool invert)
{
	char buf[256];
	int i, x, y, h, len, olen;

	XSetForeground(dpy, dc.gc, col[invert ? COL_FG : COL_BG]);
	XFillRectangle(dpy, dc.drawable, dc.gc, dc.x, dc.y, dc.w, dc.h);
	if (text == NULL)
		return;
	olen = strlen(text);
	h = dc.font.ascent + dc.font.descent;
	y = dc.y + (dc.h / 2) - (h / 2) + dc.font.ascent;
	x = dc.x + (h / 2);
	for (len = MIN(olen, (int)sizeof buf); len && textnw(text, len) > dc.w - h; len--)
		;
	if (!len)
		return;
	memcpy(buf, text, len);
	if (len < olen)
		for (i = len; i && i > len - 3; buf[--i] = '.')
			;
	XSetForeground(dpy, dc.gc, col[invert ? COL_BG : COL_FG]);
	if (dc.font.set)
		XmbDrawString(dpy, dc.drawable, dc.font.set, dc.gc, x, y, buf, len);
	else
		XDrawString(dpy, dc.drawable, dc.gc, x, y, buf, len);
}

void
enternotify(XEvent *e)
{
	struct Client *c;
	XCrossingEvent *ev = &e->xcrossing;

	if ((ev->mode != NotifyNormal || ev->detail == NotifyInferior) && ev->window != root)
		return;
	if ((c = getclient(ev->window)) != NULL)
		focus(c);
	else
		focus(NULL);
}

void
expose(XEvent *e)
{
	XExposeEvent *ev = &e->xexpose;

	if (ev->count == 0 && (ev->window == barwin))
		drawbar();
}

void
focus(struct Client *c)
{
	if (c == NULL || c->workspace != workspaceCur)
		c = stack[workspaceCur];
	if (sel != NULL && sel != c) {
		grabbuttons(sel, False);
		XSetWindowBorder(dpy, sel->win, dc.norm[COL_BORDER]);
	}
	if (c != NULL) {
		if (c->isurgent)
			clearurgent(c);
		if (!tempFocus) {
			detach(c);
			attach(c);
		}
		grabbuttons(c, True);
		XSetWindowBorder(dpy, c->win, dc.sel[COL_BORDER]);
		XSetInputFocus(dpy, c->win, RevertToPointerRoot, CurrentTime);
#ifdef XKB
		XkbLockGroup(dpy, XkbUseCoreKbd, c->xkb_grpIdx);
#endif
	} else
		XSetInputFocus(dpy, root, RevertToPointerRoot, CurrentTime);
	sel = c;
	drawbar();
}

void
focusin(XEvent *e)
{
	XFocusChangeEvent *ev = &e->xfocus;

	if (sel && ev->window != sel->win)
		XSetInputFocus(dpy, sel->win, RevertToPointerRoot, CurrentTime);
}

void
focusset(void)
{
	if (tempFocus) {
		tempFocus = False;
		if (sel != NULL) {
			detach(sel);
			attach(sel);
		}
	}
	drawbar();
}

void
furnish(const union Arg *arg)
{
	struct Client *c;
	struct Client *last = NULL;
	int maxSize = 1 << 20;

	for (c = stack[workspaceCur]; c != NULL; c = c->next) {
		c->x = sx + sw - 1;
		c->y = sy + sh - 1;
	}

	for (;;) {
		struct Client *big = NULL;
		int minSize = 0;

		for (c = stack[workspaceCur]; c != NULL; c = c->next) {
			if (c == last)
				maxSize = last->w + last->h;
			else if (c->w + c->h >= minSize && c->w + c->h < maxSize) {
				big = c;
				minSize = c->w + c->h;
			}
		}
		if (big == NULL)
			break;
		place(big);
		last = big;
		maxSize = last->w + last->h + 1;
	}
	arrange();
}

struct Client *
getclient(Window w)
{
	int i;
	struct Client *c;

	for (i = 0; i < WORKSPACE_NUM; i++) {
		for (c = stack[i]; c != NULL && w != c->win; c = c->next)
			;
		if (c != NULL)
			break;
	}
	return c;
}

long
getcolor(const char *colstr)
{
	Colormap cmap = DefaultColormap(dpy, screen);
	XColor color;

	if (!XAllocNamedColor(dpy, cmap, colstr, &color, &color))
		die("error, cannot allocate color '%s'\n", colstr);
	return color.pixel;
}

long
getstate(Window w)
{
	int format, status;
	long result = -1;
	unsigned char *p = NULL;
	unsigned long n, extra;
	Atom real;

	status = XGetWindowProperty(dpy, w, wmatom[WM_STATE], 0L, 2L, False, wmatom[WM_STATE],
			&real, &format, &n, &extra, (unsigned char**)&p);
	if (status != Success)
		return -1;
	if (n != 0)
		result = *p;
	XFree(p);
	return result;
}

Bool
gettextprop(Window w, Atom atom, char *text, int size)
{
	char **list = NULL;
	int n;
	XTextProperty name;

	if (!text || size == 0)
		return False;
	text[0] = '\0';
	XGetTextProperty(dpy, w, &name, atom);
	if (!name.nitems)
		return False;
	if (name.encoding == XA_STRING)
		strncpy(text, (char *)name.value, size - 1);
	else {
		if (XmbTextPropertyToTextList(dpy, &name, &list, &n) >= Success
		&& n > 0 && *list) {
			strncpy(text, *list, size - 1);
			XFreeStringList(list);
		}
	}
	text[size - 1] = '\0';
	XFree(name.value);
	return True;
}

void
grabbuttons(struct Client *c, Bool focused)
{
	int modifiers[] = {0, LockMask, numlockmask, numlockmask | LockMask};
	int i, j;

	updatenumlockmask();
	XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
	if (focused) {
		for (i = 0; i < LENGTH(buttons); i++)
			if (buttons[i].click == CLK_CLIENT)
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabButton(dpy, buttons[i].button, buttons[i].mask | modifiers[j], c->win, False, BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
	} else
		XGrabButton(dpy, AnyButton, AnyModifier, c->win, False,
		    BUTTONMASK, GrabModeAsync, GrabModeSync, None, None);
}

void
grabkeys(void)
{
	int modifiers[] = {0, LockMask, numlockmask, numlockmask | LockMask};
	KeyCode code;
	int i, j;

	updatenumlockmask();
	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	for (i = 0; i < LENGTH(keys); i++) {
		if (keys[i].mod == 0xFFFFFFFF)
			XGrabKey(dpy, keys[i].keysym, AnyModifier, root, True, GrabModeAsync, GrabModeAsync);
		else
			if ((code = XKeysymToKeycode(dpy, keys[i].keysym)))
				for (j = 0; j < LENGTH(modifiers); j++)
					XGrabKey(dpy, code, keys[i].mod | modifiers[j], root,
					    True, GrabModeAsync, GrabModeAsync);
	}
}

void
initfont(const char *fontstr)
{
	char *def, **missing = NULL;
	int i, n;

	dc.font.set = XCreateFontSet(dpy, fontstr, &missing, &n, &def);
	if (missing) {
		while (n--)
			fprintf(stderr, "dwm: missing fontset: %s\n", missing[n]);
		XFreeStringList(missing);
	}
	if (dc.font.set) {
		XFontSetExtents *font_extents;
		XFontStruct **xfonts;
		char **font_names;

		dc.font.ascent = dc.font.descent = 0;
		font_extents = XExtentsOfFontSet(dc.font.set);
		n = XFontsOfFontSet(dc.font.set, &xfonts, &font_names);
		for (i = 0, dc.font.ascent = 0, dc.font.descent = 0; i < n; i++) {
			dc.font.ascent = MAX(dc.font.ascent, (*xfonts)->ascent);
			dc.font.descent = MAX(dc.font.descent,(*xfonts)->descent);
			xfonts++;
		}
	} else {
		if (!(dc.font.xfont = XLoadQueryFont(dpy, fontstr))
		    && !(dc.font.xfont = XLoadQueryFont(dpy, "fixed")))
			die("error, cannot load font: '%s'\n", fontstr);
		dc.font.ascent = dc.font.xfont->ascent;
		dc.font.descent = dc.font.xfont->descent;
	}
	dc.font.height = dc.font.ascent + dc.font.descent;
}

Bool
isprotodel(struct Client *c)
{
	int i, n;
	Atom *protocols;
	Bool ret = False;

	if (XGetWMProtocols(dpy, c->win, &protocols, &n)) {
		for (i = 0; !ret && i < n; i++)
			if (protocols[i] == wmatom[WM_DELETE])
				ret = True;
		XFree(protocols);
	}
	return ret;
}

void
keymove(const union Arg *arg)
{
	struct Client *c;
	int next, test;

	if (sel == NULL)
		return;
	switch (arg->i) {
		case 0:
			next = wx;
			break;
		case 1:
			next = wy;
			break;	
		case 2:
			next = wx + ww - WIDTH(sel);
			break;
		case 3:
			next = wy + wh - HEIGHT(sel);
	}
	for (c = stack[workspaceCur]; c != NULL; c = c->next)
		if ((arg->i & 1) == 0) {
			if (c->y < sel->y + HEIGHT(sel)
			    && c->y + HEIGHT(c) > sel->y) {
				if (arg->i == 0) {
					test = c->x + WIDTH(c);
					if (test < sel->x && test > next)
						next = test;
				} else {
					test = c->x - WIDTH(sel);
					if (test > sel->x && test < next)
						next = test;
				}
			}
		} else {
			if (c->x < sel->x + WIDTH(sel)
			    && c->x + WIDTH(c) > sel->x) {
				if (arg->i == 1) {
					test = c->y + HEIGHT(c);
					if (test < sel->y && test > next)
						next = test;
				} else {
					test = c->y - HEIGHT(sel);
					if (test > sel->y && test < next)
						next = test;
				}
			}

		}
	if ((arg->i & 1) == 0)
		resize(sel, next, sel->y, sel->w, sel->h, False);
	else
		resize(sel, sel->x, next, sel->w, sel->h, False);
}

void
keypress(XEvent *e)
{
	XKeyEvent *ev = &e->xkey;
	KeySym keysym;
	int i;

	keysym = XKeycodeToKeysym(dpy, (KeyCode)ev->keycode, 0);
	for (i = 0; i < LENGTH(keys); i++)
		if (keysym == keys[i].keysym
		    && CLEANMASK(keys[i].mod) == CLEANMASK(ev->state)
		    && keys[i].func != NULL)
			keys[i].func(&keys[i].arg);
		else if (ev->keycode == keys[i].keysym
		    && keys[i].mod == 0xFFFFFFFF
		    && keys[i].func != NULL)
			keys[i].func(&keys[i].arg);
}

void
keyrelease(XEvent *e)
{
	XKeyEvent *ev = &e->xkey;

	if (ev->keycode == XKeysymToKeycode(dpy, XK_Alt_L)
	    || ev->keycode == XKeysymToKeycode(dpy, XK_Alt_R))
		focusset();
}

void
killclient(const union Arg *arg)
{
	XEvent ev;

	if (!sel)
		return;
	if (isprotodel(sel)) {
		ev.type = ClientMessage;
		ev.xclient.window = sel->win;
		ev.xclient.message_type = wmatom[WM_PROTOCOLS];
		ev.xclient.format = 32;
		ev.xclient.data.l[0] = wmatom[WM_DELETE];
		ev.xclient.data.l[1] = CurrentTime;
		XSendEvent(dpy, sel->win, False, NoEventMask, &ev);
	} else
		XKillClient(dpy, sel->win);
}

void
manage(Window w, XWindowAttributes *wa)
{
	struct Client *c, *t = NULL;
	Window trans = None;
	XWindowChanges wc;

	if ((c = malloc(sizeof(struct Client))) == NULL)
		die("fatal: could not malloc() %u bytes\n", sizeof(struct Client));
	memset(c, 0, sizeof(struct Client));
	c->win = w;

	c->x = wa->x;
	c->y = wa->y;
	c->w = wa->width;
	c->h = wa->height;
	if (c->w < 10)
		c->w = 10;
	if (c->h < 10)
		c->h = 10;
	c->oldbw = wa->border_width;
	if (c->w == sw && c->h == sh) {
		c->x = sx;
		c->y = sy;
		c->bw = 0;
	} else
		c->bw = borderpx;

	c->zoomed = 0;
#ifdef XKB
	c->xkb_grpIdx = 0;
#endif

	wc.border_width = c->bw;
	XConfigureWindow(dpy, w, CWBorderWidth, &wc);
	XSetWindowBorder(dpy, w, dc.norm[COL_BORDER]);
	configure(c);
	updatesizehints(c);
	XSelectInput(dpy, w, EnterWindowMask | FocusChangeMask | KeyReleaseMask | PropertyChangeMask | StructureNotifyMask);
	grabbuttons(c, False);
	updatetitle(c);
	if (XGetTransientForHint(dpy, w, &trans))
		t = getclient(trans);
	c->workspace = t ? t->workspace : workspaceCur;
	attach(c);
	if (c->bw != 0)
		place(c);
	XRaiseWindow(dpy, c->win);
	XMapWindow(dpy, c->win);
	setclientstate(c, NormalState);
	XMoveWindow(dpy, c->win, c->x, c->y);
	resize(c, c->x, c->y, c->w, c->h, True);
	focus(c);
}

void
mappingnotify(XEvent *e)
{
	XMappingEvent *ev = &e->xmapping;

	XRefreshKeyboardMapping(ev);
	if (ev->request == MappingKeyboard)
		grabkeys();
}

void
maprequest(XEvent *e)
{
	static XWindowAttributes wa;
	XMapRequestEvent *ev = &e->xmaprequest;

	if (!XGetWindowAttributes(dpy, ev->window, &wa))
		return;
	if (wa.override_redirect)
		return;
	if (!getclient(ev->window))
		manage(ev->window, &wa);
}

void
mousemove(const union Arg *arg)
{
	int omx, omy, ocx, ocy, di, nx, ny;
	unsigned int dui;
	struct Client *c, *f;
	Window dummy;
	XEvent ev;

	if ((c = sel) == NULL)
		return;
	raisesel();
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
	    None, cursor[CUR_MOVE], CurrentTime) != GrabSuccess)
		return;
	XQueryPointer(dpy, root, &dummy, &dummy, &omx, &omy, &di, &di, &dui);
	do {
		XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
		switch (ev.type) {
			case ConfigureRequest:
			case Expose:
			case MapRequest:
				handler[ev.type](&ev);
				break;
			case MotionNotify:
				nx = ocx + (ev.xmotion.x - omx);
				ny = ocy + (ev.xmotion.y - omy);
				if (snap > 0) {
					for (f = stack[workspaceCur]; f != NULL; f = f->next)
						snapclient(c, &nx, &ny, f->x + WIDTH(f), f->y + HEIGHT(f), f->x, f->y);
					snapclient(c, &nx, &ny, wx, wy, wx + ww, wy + wh);
				}
				resize(c, nx, ny, c->w, c->h, False);
				break;
		}
	} while (ev.type != ButtonRelease);
	XUngrabPointer(dpy, CurrentTime);
}

void
mouseresize(const union Arg *arg)
{
	int ocx, ocy;
	int nw, nh;
	struct Client *c;
	XEvent ev;

	if (!(c = sel))
		return;
	raisesel();
	ocx = c->x;
	ocy = c->y;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
	    None, cursor[CUR_RESIZE], CurrentTime) != GrabSuccess)
		return;
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	do {
		XMaskEvent(dpy, MOUSEMASK | ExposureMask | SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
			handler[ev.type](&ev);
			break;
		case MotionNotify:
			nw = MAX(ev.xmotion.x - ocx - 2 * c->bw + 1, 1);
			nh = MAX(ev.xmotion.y - ocy - 2 * c->bw + 1, 1);

			resize(c, c->x, c->y, nw, nh, True);
			break;
		}
	} while (ev.type != ButtonRelease);
	XWarpPointer(dpy, None, c->win, 0, 0, 0, 0, c->w + c->bw - 1, c->h + c->bw - 1);
	XUngrabPointer(dpy, CurrentTime);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
		;
}

void
nextfocus(const union Arg *arg)
{
	struct Client *c = NULL;

	tempFocus = True;
	if (sel != NULL)
		c = arg->i > 0 ? sel->next : sel->prev;
	if (c == NULL)
		c = stack[workspaceCur];
	if (c != NULL) {
		focus(c);
		raisesel();
	}
}

void
nextlang(const union Arg *arg)
{
#ifdef XKB
	if (sel) {
		sel->xkb_grpIdx = (sel->xkb_grpIdx + 1) % xkb_grpNum;
		XkbLockGroup(dpy, XkbUseCoreKbd, sel->xkb_grpIdx);
		drawbar();
	}
#endif
}

void
nextstatus(const union Arg *arg)
{
	sidx++;
	updatestatus();
}

void
place(struct Client *c)
{
	int minInfl = 1 << 24;
	int minRad = 1 << 24;
	int minX = wx;
	int minY = wy;

	c->x = wx;
	c->y = wy;
	for (;;) {
		int infl = 0;
		int rad = 0;
		struct Client *f;
		int next;
		int test;

		for (f = stack[c->workspace]; f != NULL; f = f->next) {
			int x1 = MAX(c->x, f->x);
			int y1 = MAX(c->y, f->y);
			int x2 = MIN(c->x + c->w + 2*c->bw, f->x + f->w + 2*f->bw);
			int y2 = MIN(c->y + c->h + 2*c->bw, f->y + f->h + 2*f->bw);

			infl += MAX(x2-x1, 0) * MAX(y2-y1, 0);
		}
		rad = c->x*c->x + c->y*c->y;
		if (infl < minInfl) {
			minInfl = infl;
			minRad = rad;
			minX = c->x;
			minY = c->y;
		} else if (infl == minInfl && rad < minRad) {
			minRad = rad;
			minX = c->x;
			minY = c->y;
		}

		if (c->x + WIDTH(c) == wx + ww) {
			if (c->y + HEIGHT(c) == wy + wh)
				break;

			next = wy + wh - HEIGHT(c);
			for (f = stack[c->workspace]; f != NULL; f = f->next) {
				test = f->y - HEIGHT(c);
				if (test > c->y && test < next)
					next = test;
				test = f->y;
				if (test > c->y && test < next)
					next = test;
				test = f->y + HEIGHT(f) - HEIGHT(c);
				if (test > c->y && test < next)
					next = test;
				test = f->y + HEIGHT(f);
				if (test > c->y && test < next)
					next = test;
			}
			c->x = wx;
			c->y = next;
		} else {
			next = wx + ww - WIDTH(c);
			for (f = stack[c->workspace]; f != NULL; f = f->next) {
				test = f->x - WIDTH(c);
				if (test > c->x && test < next)
					next = test;
				test = f->x;
				if (test > c->x && test < next)
					next = test;
				test = f->x + WIDTH(f) - WIDTH(c);
				if (test > c->x && test < next)
					next = test;
				test = f->x + WIDTH(f);
				if (test > c->x && test < next)
					next = test;
			}
			c->x = next;
		}
	}

	c->x = minX;
	c->y = minY;
}

void
propertynotify(XEvent *e)
{
	struct Client *c;
	Window trans;
	XPropertyEvent *ev = &e->xproperty;

	if ((ev->window == root) && (ev->atom == XA_WM_NAME))
		updatestatus();
	else if (ev->state == PropertyDelete)
		return;
	else if ((c = getclient(ev->window))) {
		switch (ev->atom) {
		default: break;
		case XA_WM_TRANSIENT_FOR:
			XGetTransientForHint(dpy, c->win, &trans);
			break;
		case XA_WM_NORMAL_HINTS:
			updatesizehints(c);
			break;
		case XA_WM_HINTS:
			updatewmhints(c);
			drawbar();
			break;
		}
		if (ev->atom == XA_WM_NAME || ev->atom == netatom[NET_WM_NAME]) {
			updatetitle(c);
			if (c == sel)
				drawbar();
		}
	}
}

void
quit(const union Arg *arg)
{
	running = False;
}

void
raisesel(void)
{
	XEvent ev;

	drawbar();
	if (sel == NULL)
		return;
	XRaiseWindow(dpy, sel->win);
	XSync(dpy, False);
	while (XCheckMaskEvent(dpy, EnterWindowMask, &ev))
		;
}

void
resize(struct Client *c, int x, int y, int w, int h, Bool sizehints)
{
	XWindowChanges wc;

	if (sizehints) {
		Bool baseismin = c->basew == c->minw && c->baseh == c->minh;

		w = MAX(1, w);
		h = MAX(1, h);

		if (!baseismin) {
			w -= c->basew;
			h -= c->baseh;
		}

		if (c->mina > 0 && c->maxa > 0) {
			if (c->maxa < (float)w / h)
				w = h * c->maxa;
			else if (c->mina < (float)h / w)
				h = w * c->mina;
		}

		if (baseismin) {
			w -= c->basew;
			h -= c->baseh;
		}

		if (c->incw)
			w -= w % c->incw;
		if (c->inch)
			h -= h % c->inch;

		w += c->basew;
		h += c->baseh;

		w = MAX(w, c->minw);
		h = MAX(h, c->minh);

		if (c->maxw)
			w = MIN(w, c->maxw);

		if (c->maxh)
			h = MIN(h, c->maxh);
	}
	if (w <= 0 || h <= 0)
		return;
	if (x >= sx + sw)
		x = sx + sw - 1;
	if (y >= sy + sh)
		y = sy + sh - 1;
	if (x + w + 2 * c->bw < sx)
		x = sx;
	if (y + h + 2 * c->bw < sy)
		y = sy;
	if (h < dc.h)
		h = dc.h;
	if (w < dc.h)
		w = dc.h;
	if (c->x != x || c->y != y || c->w != w || c->h != h) {
		c->x = wc.x = x;
		c->y = wc.y = y;
		c->w = wc.width = w;
		c->h = wc.height = h;
		wc.border_width = c->bw;
		XConfigureWindow(dpy, c->win,
		    CWX | CWY | CWWidth | CWHeight | CWBorderWidth, &wc);
		configure(c);
		XSync(dpy, False);
	}
}

void
restart(const union Arg *arg)
{
	execlp("dwm", "dwm", NULL);
	running = False;
}

void
run(void)
{
	fd_set fds;
	struct timeval timeout;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	XSync(dpy, False);
	while (running) {
		if (timeout.tv_sec == 0 && timeout.tv_usec == 0) {
			if (swarn) {
				timeout.tv_sec = 0;
				timeout.tv_usec = 1000 * warn_rate_ms;
				swarn = 3 - swarn;
			} else {
				timeout.tv_sec = warn_wait_s;
				timeout.tv_usec = 0;
			}
			updatestatus();
		}
		FD_ZERO(&fds);
		FD_SET(connection, &fds);
		select(connection + 1, &fds, NULL, NULL, &timeout);

		while (XPending(dpy)) {
#ifdef XKB
			XkbEvent ev;
			XNextEvent(dpy, &ev.core);
			if (ev.type == xkb_event) {
				fprintf(stderr, "%d\n", ev.any.xkb_type);
				switch (ev.any.xkb_type) {
					case XkbControlsNotify:
						printf("Controls: num_groups = %d\n", ev.ctrls.num_groups);
						break;
					case XkbNamesNotify:
						printf("Names: changed = %d, groups = \n", ev.names.changed_groups);
						break;
				}
			} else {
				if (xkb_grpNum < 0 && ev.type == KeyPress) {
					XkbDescPtr xkb;
					char *s;
					int i;

					xkb = XkbGetMap(dpy, 0, XkbUseCoreKbd);
					XkbGetControls(dpy, XkbGroupNamesMask, xkb);
					xkb_grpNum = xkb->ctrls->num_groups;
					XkbGetNames(dpy, XkbGroupNamesMask, xkb);
					for (i = 0; i < xkb_grpNum; i++) {
						s = XGetAtomName(dpy, xkb->names->groups[i]);
						strcpy(xkb_grpName[i], s);
						XFree(s);
					}
					XkbFreeKeyboard(xkb, XkbGroupNamesMask, True);
				}
				if (handler[ev.type] != NULL)
					(handler[ev.type])(&ev.core);
			}
#else
			XEvent ev;
			XNextEvent(dpy, &ev);
			if (handler[ev.type] != NULL)
				(handler[ev.type])(&ev);
#endif
		}
	}
}

void
scan(void)
{
	Window d1, d2, *wins = NULL;
	unsigned int i, num;
	XWindowAttributes wa;

	if (XQueryTree(dpy, root, &d1, &d2, &wins, &num)) {
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa)
			    || wa.override_redirect
			    || XGetTransientForHint(dpy, wins[i], &d1))
				continue;
			if (wa.map_state == IsViewable
			    || getstate(wins[i]) == IconicState)
				manage(wins[i], &wa);
		}
		for (i = 0; i < num; i++) {
			if (!XGetWindowAttributes(dpy, wins[i], &wa))
				continue;
			if (XGetTransientForHint(dpy, wins[i], &d1)
			    && (wa.map_state == IsViewable
			    || getstate(wins[i]) == IconicState))
				manage(wins[i], &wa);
		}
		if (wins)
			XFree(wins);
	}
	arrange();
	focus(NULL);
}

void
setclientstate(struct Client *c, long state)
{
	long data[] = {state, None};

	XChangeProperty(dpy, c->win, wmatom[WM_STATE], wmatom[WM_STATE], 32,
			PropModeReplace, (unsigned char*)data, 2);
}

void
setup(void)
{
	GC gc;
	Pixmap bitmap, pixmap;
	XGCValues gcv;
	XSetWindowAttributes wa;

	connection = ConnectionNumber(dpy);

	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	initfont(font);
	sx = 0;
	sy = 0;
	sw = DisplayWidth(dpy, screen);
	sh = DisplayHeight(dpy, screen);
	dc.h = dc.font.height + 2;
	updategeom();

	wmatom[WM_PROTOCOLS] = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmatom[WM_DELETE] = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	wmatom[WM_STATE] = XInternAtom(dpy, "WM_STATE", False);
	netatom[NET_SUPPORTED] = XInternAtom(dpy, "_NET_SUPPORTED", False);
	netatom[NET_WM_NAME] = XInternAtom(dpy, "_NET_WM_NAME", False);

	gcv.background = getcolor(bgbgcolor);
	gcv.foreground = getcolor(bgfgcolor);
	gc = XCreateGC(dpy, root, GCForeground | GCBackground, &gcv);
	bitmap = XCreateBitmapFromData(dpy, root, (char*)bg, bgw, bgh);
	pixmap = XCreatePixmap(dpy, root, bgw, bgh, DefaultDepth(dpy, screen));
	XCopyPlane(dpy, bitmap, pixmap, gc, 0, 0, bgw, bgh, 0, 0, 1);
	XSetWindowBackgroundPixmap(dpy, root, pixmap);
	XFreeGC(dpy, gc);
	XFreePixmap(dpy, bitmap);
	XFreePixmap(dpy, pixmap);
	XClearWindow(dpy, root);

	wa.cursor = cursor[CUR_NORMAL] = XCreateFontCursor(dpy, XC_left_ptr);
	cursor[CUR_RESIZE] = XCreateFontCursor(dpy, XC_sizing);
	cursor[CUR_MOVE] = XCreateFontCursor(dpy, XC_fleur);

	dc.norm[COL_BORDER] = getcolor(normbordercolor);
	dc.norm[COL_BG] = getcolor(normbgcolor);
	dc.norm[COL_FG] = getcolor(normfgcolor);
	dc.sel[COL_BORDER] = getcolor(selbordercolor);
	dc.sel[COL_BG] = getcolor(selbgcolor);
	dc.sel[COL_FG] = getcolor(selfgcolor);
	dc.warn[0][COL_BG] = getcolor(warn1bgcolor);
	dc.warn[0][COL_FG] = getcolor(warn1fgcolor);
	dc.warn[1][COL_BG] = getcolor(warn2bgcolor);
	dc.warn[1][COL_FG] = getcolor(warn2fgcolor);
	dc.drawable = XCreatePixmap(dpy, root, DisplayWidth(dpy, screen), dc.h,
	    DefaultDepth(dpy, screen));
	dc.gc = XCreateGC(dpy, root, 0, 0);
	XSetLineAttributes(dpy, dc.gc, 1, LineSolid, CapButt, JoinMiter);
	if (!dc.font.set)
		XSetFont(dpy, dc.gc, dc.font.xfont->fid);

	wa.override_redirect = 1;
	wa.background_pixmap = ParentRelative;
	wa.event_mask = ButtonPressMask | ExposureMask;
	barwin = XCreateWindow(dpy, root, wx, by, ww, dc.h, 0,
	    DefaultDepth(dpy, screen), CopyFromParent,
	    DefaultVisual(dpy, screen),
	    CWOverrideRedirect | CWBackPixmap | CWEventMask, &wa);
	XDefineCursor(dpy, barwin, cursor[CUR_NORMAL]);
	XMapRaised(dpy, barwin);
	updatestatus();

	XChangeProperty(dpy, root, netatom[NET_SUPPORTED], XA_ATOM, 32,
	    PropModeReplace, (unsigned char*)netatom, NET_LAST);

	wa.event_mask = SubstructureRedirectMask | SubstructureNotifyMask
	    | ButtonPressMask | KeyReleaseMask
	    | EnterWindowMask | LeaveWindowMask
	    | StructureNotifyMask | PropertyChangeMask;
	XChangeWindowAttributes(dpy, root, CWEventMask | CWCursor, &wa);
	XSelectInput(dpy, root, wa.event_mask);

	grabkeys();

#ifdef XKB
	XkbSelectEvents(dpy, XkbUseCoreKbd, XkbAllEventsMask, 1);
#endif
}

void
sigchld(int signal)
{
	while (0 < waitpid(-1, NULL, WNOHANG));
}

void
snapclient(struct Client *c, int *nx, int *ny, int left, int top, int right, int bottom)
{
	int x = *nx;
	int y = *ny;
	int x2 = x + WIDTH(c);
	int y2 = y + HEIGHT(c);

	if (x2 > right && x2 < right + snap)
		x = right - WIDTH(c);
	if (y2 > bottom && y2 < bottom + snap)
		y = bottom - HEIGHT(c);
	if (x < left && x > left - snap)
		x = left;
	if (y < top && y > top - snap)
		y = top;
	*nx = x;
	*ny = y;
}

void
spawn(const union Arg *arg)
{
	signal(SIGCHLD, sigchld);
	if (fork() == 0) {
		if (dpy != NULL)
			close(connection);
		setsid();
		execvp(((char**)arg->v)[0], (char**)arg->v);
		fprintf(stderr, "dwm: execvp %s", ((char**)arg->v)[0]);
		perror(" failed");
		exit(0);
	}
}

int
textnw(const char *text, int len)
{
	XRectangle r;

	if (dc.font.set) {
		XmbTextExtents(dc.font.set, text, len, NULL, &r);
		return r.width;
	}
	return XTextWidth(dc.font.xfont, text, len);
}

void
toworkspace(const union Arg *arg)
{
	if (sel != NULL) {
		detach(sel);
		sel->workspace = arg->i;
		attach(sel);
		place(sel);
		arrange();
		focus(NULL);
	}
}

void
unmanage(struct Client *c)
{
	XWindowChanges wc;

	wc.border_width = c->oldbw;
	XGrabServer(dpy);
	XSetErrorHandler(xerrordummy);
	XConfigureWindow(dpy, c->win, CWBorderWidth, &wc);
	detach(c);
	if (sel == c)
		focus(NULL);
	XUngrabButton(dpy, AnyButton, AnyModifier, c->win);
	setclientstate(c, WithdrawnState);
	free(c);
	XSync(dpy, False);
	XSetErrorHandler(xerror);
	XUngrabServer(dpy);
}

void
unmapnotify(XEvent *e)
{
	struct Client *c;

	if ((c = getclient(e->xunmap.window)) != NULL)
		unmanage(c);
}

void
updatebar(void)
{
	if (dc.drawable != 0)
		XFreePixmap(dpy, dc.drawable);
	dc.drawable = XCreatePixmap(dpy, root, ww, dc.h, DefaultDepth(dpy, screen));
	XMoveResizeWindow(dpy, barwin, wx, by, ww, dc.h);
}

void
updategeom(void)
{
#ifdef XINERAMA
	XineramaScreenInfo *info = NULL;
	int n, i = 0;

	if (XineramaIsActive(dpy) && (info = XineramaQueryScreens(dpy, &n))) { 
		if (n > 1) {
			Window dummy;
			int di, x, y;
			int dui;

			if (XQueryPointer(dpy, root, &dummy, &dummy, &x, &y, &di, &di, &dui))
				for (i = 0; i < n; i++)
					if (INRECT(x, y, info[i].x_org, info[i].y_org, info[i].width, info[i].height))
						break;
		}
		wx = info[i].x_org;
		wy = showbar && topbar ? info[i].y_org + dc.h : info[i].y_org;
		ww = info[i].width;
		wh = showbar ? info[i].height - dc.h : info[i].height;
		XFree(info);
	} else
#endif
	{
		wx = sx;
		wy = topbar ? sy + dc.h : sy;
		ww = sw;
		wh = sh - dc.h;
	}

	by = topbar ? wy - dc.h : wy + wh;
}

void
updatenumlockmask(void)
{
	XModifierKeymap *modmap;
	int i, j;

	numlockmask = 0;
	modmap = XGetModifierMapping(dpy);
	for (i = 0; i < 8; i++)
		for (j = 0; j < modmap->max_keypermod; j++)
			if (modmap->modifiermap[i * modmap->max_keypermod + j] == XKeysymToKeycode(dpy, XK_Num_Lock))
				numlockmask = (1 << i);
	XFreeModifiermap(modmap);
}

void
updatesizehints(struct Client *c)
{
	long msize;
	XSizeHints size;

	if (!XGetWMNormalHints(dpy, c->win, &size, &msize))
		size.flags = PSize; 
	if (size.flags & PBaseSize) {
		c->basew = size.base_width;
		c->baseh = size.base_height;
	} else if (size.flags & PMinSize) {
		c->basew = size.min_width;
		c->baseh = size.min_height;
	} else
		c->basew = c->baseh = 0;
	if (size.flags & PResizeInc) {
		c->incw = size.width_inc;
		c->inch = size.height_inc;
	} else
		c->incw = c->inch = 0;
	if (size.flags & PMaxSize) {
		c->maxw = size.max_width;
		c->maxh = size.max_height;
	} else
		c->maxw = c->maxh = 0;
	if (size.flags & PMinSize) {
		c->minw = size.min_width;
		c->minh = size.min_height;
	} else if (size.flags & PBaseSize) {
		c->minw = size.base_width;
		c->minh = size.base_height;
	} else
		c->minw = c->minh = 0;
	if (size.flags & PAspect) {
		c->mina = (float)size.min_aspect.y / (float)size.min_aspect.x;
		c->maxa = (float)size.max_aspect.x / (float)size.max_aspect.y;
	} else
		c->maxa = c->mina = 0.0;
}

void
updatestatus(void)
{
	char tmp[256], *p;
	int i;

	if (gettextprop(root, XA_WM_NAME, tmp, sizeof tmp)) {
		for (p = tmp; *p != '!' && *p != '\0'; p++)
			;
		swarn = *p == '!' ? (swarn ? swarn : 1) : 0;
		for (p = tmp, i = 0; *p != '\0' && i < sidx; p++)
			if (*p == '#')
				i++;
		if (sidx > i) {
			sidx = 0;
			p = tmp;
		}
		if (p[0] == '!')
			p++;
		for (i = 0; p[i] != '\0' && p[i] != '#'; i++)
			;
		memcpy(stext, p, i);
		stext[i] = '\0';
	} else
		strcpy(stext, "dwm-"VERSION);
	drawbar();
}

void
updatetitle(struct Client *c)
{
	if (!gettextprop(c->win, netatom[NET_WM_NAME], c->name, sizeof c->name))
		gettextprop(c->win, XA_WM_NAME, c->name, sizeof c->name);
}

void
updatewmhints(struct Client *c)
{
	XWMHints *wmh;

	if ((wmh = XGetWMHints(dpy, c->win)) != NULL) {
		if (c == sel && wmh->flags & XUrgencyHint) {
			wmh->flags &= ~XUrgencyHint;
			XSetWMHints(dpy, c->win, wmh);
		} else
			c->isurgent = (wmh->flags & XUrgencyHint) ? True : False;
		XFree(wmh);
	}
}

void
view(const union Arg *arg)
{
	if (arg->i == workspaceCur)
		return;
	workspaceCur = arg->i;
	arrange();
	focus(NULL);
}

void
viewstep(const union Arg *arg)
{
	if (arg->i < 0)
		workspaceCur = (workspaceCur + WORKSPACE_NUM - 1) % WORKSPACE_NUM;
	else
		workspaceCur = (workspaceCur + 1) % WORKSPACE_NUM;
	arrange();
	focus(NULL);
}

int
xerror(Display *dpy, XErrorEvent *ee)
{
	if (ee->error_code == BadWindow
	    || (ee->request_code == X_SetInputFocus && ee->error_code == BadMatch)
	    || (ee->request_code == X_PolyText8 && ee->error_code == BadDrawable)
	    || (ee->request_code == X_PolyFillRectangle && ee->error_code == BadDrawable)
	    || (ee->request_code == X_PolySegment && ee->error_code == BadDrawable)
	    || (ee->request_code == X_ConfigureWindow && ee->error_code == BadMatch)
	    || (ee->request_code == X_GrabButton && ee->error_code == BadAccess)
	    || (ee->request_code == X_GrabKey && ee->error_code == BadAccess)
	    || (ee->request_code == X_CopyArea && ee->error_code == BadDrawable))
		return 0;
	fprintf(stderr, "dwm: Fatal error: request code=%d, error code=%d\n",
	    ee->request_code, ee->error_code);
	return xerrorxlib(dpy, ee);
}

int
xerrordummy(Display *dpy, XErrorEvent *ee)
{
	return 0;
}

int
xerrorstart(Display *dpy, XErrorEvent *ee)
{
	otherwm = True;
	return -1;
}

void
zoom(const union Arg *arg)
{
	if (sel != NULL) {
		if (sel->zoomed) {
			resize(sel, sel->zux, sel->zuy, sel->zuw, sel->zuh, True);
			sel->zoomed = 0;
		} else {
			sel->zux = sel->x;
			sel->zuy = sel->y;
			sel->zuw = sel->w;
			sel->zuh = sel->h;
			if ((sel->zoomed = arg->i + 1) == 1)
				resize(sel, wx, wy, ww - 2*sel->bw, wh - 2*sel->bw, True);
			else
				resize(sel, sel->x, wy, sel->w, wh - 2*sel->bw, True);
		}
	}
}

int
main(int argc, char *argv[])
{
	if (argc != 1)
		die("usage: dwm [-v]\n");

	if (setlocale(LC_CTYPE, "") == NULL || !XSupportsLocale())
		fprintf(stderr, "warning: no locale support\n");

#ifdef XKB
	if ((dpy = XkbOpenDisplay(NULL, &xkb_event, NULL, NULL, NULL, NULL)) == NULL)
#else
	if ((dpy = XOpenDisplay(NULL)) == NULL)
#endif
		die("dwm: cannot open display\n");

	checkotherwm();
	setup();
	scan();
	run();
	cleanup();

	XCloseDisplay(dpy);
	return 0;
}
