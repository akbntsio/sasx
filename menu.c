/*
 * popup menu library
 *
 * 2002.11.5 first version
 *    マウスの位置をデフォルトアイテム選択位置としてポップアップする
 *    ディスプレイからはみ出さないように窓を動かすが、カーソルは動かさない
 * memo
 *    simple_menu : メニュー内の１つを選ぶ(ポジション履歴なし)
 *    radio_menu  : メニュー内の１つを選ぶ(ポジション履歴あり)
 *    toggle_menu : メニュー内の１つ１つがON/OFFボタン(ポジション履歴あり)
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include "menu.h"

/*TIMES*/
/*static char        *menu_fontname = "-adobe-times-bold-r-*-*-12-*";*/
static char        *menu_fontname = "fixed";
static char        *menu_fontname16 = "k14";
static int          menu_margin = 2;
static char        *menu_fg = "black";
static char        *menu_bg = "white";
static int          menu_bw = 1;
static char        *menu_bd = "black";
static int          menu_min_width = 8;

int  getcolor(Display *disp, int screen, char *name)
{
	Colormap     cmap;
	XColor       exact, color;
	cmap = XDefaultColormap(disp, screen);
	XAllocNamedColor(disp, cmap, name, &exact, &color);
	return color.pixel;
}

void  getrootxy(Display *disp, int *rootx, int *rooty)
{
	Window        root, child;
	int           winx, winy;
	unsigned int  mask;
	
	XQueryPointer(disp, DefaultRootWindow(disp), &root, &child,
		rootx, rooty, &winx, &winy, &mask);
}

void  getrootwh(Display *disp, unsigned int *rootw, unsigned int *rooth)
{
	Window        root;
	int           x, y;
	unsigned int  b, d;
	
	XGetGeometry(disp, DefaultRootWindow(disp), &root,
		&x, &y, rootw, rooth, &b, &d);
}

int  menu_popup(
	Display *disp,   /* X display */
	int screen,      /* X screen */
	int size,        /* the number of items */
	char **item,     /* item strings */
	int id,          /* default item position */
	char *flag,      /* marking flags of each item */
	char *mark       /* mark string something like "-> " */
)
{
	int             fg, bd, bg, bw;
	int             rootx, rooty, x, y;
	unsigned int    rootw, rooth;
	int             winx, winy, winw, winh;
	Window          win;
	XSetWindowAttributes  atr;
	Font            font, font16;
	XFontStruct    *font_struct, *font_struct16;
	int             fonth, fonta, fontd;
	int             fonth16, fonta16, fontd16;
	XCharStruct     char_struct;
	XCharStruct     char_struct16;
	int             lblh;
	int             markw;
	XGCValues       gcv;
	GC              gc, gc16, bggc, revgc;
	int             i, loop, active;
	int             id0, id1;
	Cursor          cursor;

	/* GET COLOR */
	fg = getcolor(disp, screen, menu_fg);
	bg = getcolor(disp, screen, menu_bg);
	bd = getcolor(disp, screen, menu_bd);

	/* GET FONT */
	font = XLoadFont(disp, menu_fontname);
	font16 = XLoadFont(disp, menu_fontname16);
	font_struct = XQueryFont(disp,font);
	font_struct16 = XQueryFont(disp,font16);
	XTextExtents(font_struct, "", 0, &fonth, &fonta, &fontd, &char_struct);
	XTextExtents(font_struct16, "", 0, &fonth16, &fonta16, &fontd16, &char_struct16);
	lblh = ((fonta>fonta16)?fonta:fonta16)
		+ menu_margin * 2;

	/* GET POSITION and SIZE */
	getrootwh(disp, &rootw, &rooth);
	getrootxy(disp, &rootx, &rooty);
	winw = menu_min_width;
	for( i=0; i<size; i++ ){
		int   width;
		char *txt = item[i];
		width = XTextWidth(font_struct, txt, strlen(txt));
		if( winw < width ) winw = width;
	}
	if( mark && *mark )
		markw = XTextWidth(font_struct, mark, strlen(mark));
	else
		markw = 0;
	winw += markw;
	winw += menu_margin * 2;
	winh = size * lblh;
	winx = rootx - winw/2;
	if( winx >= (int)rootw - winw ) winx = rootw - winw;
	if( winx < 0 ) winx = 0;
	winy = rooty - lblh * id - lblh/2;
	if( winy >= (int)rooth - winh ) winy = rooth - winh;
	if( winy < 0 ) winy = 0;
	bw = menu_bw;

	/* GET WINDOW */
	win = XCreateSimpleWindow( disp, DefaultRootWindow(disp),
		winx, winy, winw, winh, bw, bd, bg);
	/* THIS WINDOW HAS NO TITLE */
	atr.override_redirect = True;
	atr.save_under = True;
	XChangeWindowAttributes(disp,win,CWOverrideRedirect|CWSaveUnder,&atr);
	/* SET THE WINDOW EVENT MASK */
	XSelectInput(disp, win,
		ButtonPressMask|ButtonReleaseMask|PointerMotionMask|KeyPressMask|ExposureMask);
	/* SET THE WINDOW VISIBLE */
	XMapRaised(disp,win);
	/* GET CURSOR */
	cursor = XCreateFontCursor(disp, XC_arrow);
	/* GRAB POINTER */
	XGrabPointer(disp, win, True, ButtonPressMask|ButtonReleaseMask,
		GrabModeAsync, GrabModeAsync, None, cursor, CurrentTime);

	/* GET GC */
	gcv.foreground = fg;
	gcv.function = GXcopy;
	gcv.line_width = 1;
	gcv.fill_style = FillSolid;
	gcv.font = font;
	gc = XCreateGC(disp,win,
		GCForeground | GCLineWidth | GCFunction | GCFillStyle | GCFont,
		&gcv);
	gcv.font = font16;
	gc16 = XCreateGC(disp,win,
		GCForeground | GCLineWidth | GCFunction | GCFillStyle | GCFont,
		&gcv);
	gcv.foreground = bg;
	bggc = XCreateGC(disp,win,
		GCForeground | GCLineWidth | GCFunction | GCFillStyle | GCFont,
		&gcv);
	gcv.function = GXxor;
	revgc = XCreateGC(disp,win,
		GCForeground | GCLineWidth | GCFunction | GCFillStyle | GCFont,
		&gcv);
	

	/* EVENT LOOP UNTIL RELEASE */
	id0 = id;
	active = 0;
	for(loop=1; loop; ){
		XEvent event;
		XNextEvent(disp,&event);
		switch(event.type){
		case Expose:
			/* DRAW ITEMS */
			if( event.xexpose.window != win ) break;
			XFillRectangle(disp, win, bggc, 0, 0, winw, winh);/* CLEAR */
			for( i=0; i<size; i++ ){
				char *txt = item[i];
				if( flag[i] && mark && *mark )
					XDrawString(disp, win, gc,
						menu_margin, (i+1)*lblh-menu_margin-1, mark, strlen(mark));
				XDrawString(disp, win, gc,
					menu_margin + markw, (i+1)*lblh-menu_margin-1, txt, strlen(txt));
			}
			XFillRectangle(disp, win, revgc, 0, id*lblh, winw, lblh);
			XFlush(disp);
		case LeaveNotify: break;
		case EnterNotify: break;
		case MotionNotify:
			active = 1;
			getrootxy(disp,&x,&y);
			id1 = (y - winy) / lblh;
			if( y < winy || y >= winy + winh ) id1 = -1;
			else if( x < winx || x >= winx + winw ) id1 = -1;
			if( id1 != id0 ){
				XFillRectangle(disp, win, revgc, 0, id0*lblh, winw, lblh);
				XFillRectangle(disp, win, revgc, 0, id1*lblh, winw, lblh);
				id0 = id1;
			}
			break;
		case ButtonPress:
			active = 1;
			break;
		case ButtonRelease:
			if(active) loop=0;
			break;
		default: break;
		}
	}

	/* TERMINATE */
	XUngrabPointer(disp, CurrentTime);
	XDestroyWindow(disp,win);
	XFreeGC(disp,gc);
	XFreeGC(disp,gc16);
	XFreeGC(disp,bggc);
	XFreeGC(disp,revgc);

	/* RETURN ITEM */
	return id0;
}

/* no mark on left */
/* default position id */
/* return select_id or (-1) */
int  simple_menu(Display *disp, int screen, int size, char **txt, int id)
{
	char *flag;
	int   i;

	flag = malloc(sizeof(char)*size);
	if(!flag){ fprintf(stderr,"simple_menu:malloc\n"); return(-1); }
	for(i=0; i<size; i++) flag[i] = 0;

	id = menu_popup(disp, screen, size, txt, id, flag, "");

	free(flag);

	return id;
}

/* mark current selected item on left */
/* default position *id */
int  radio_menu(Display *disp, int screen, int size, char **txt, int *id)
{
	char *flag;
	int   i;

	flag = malloc(sizeof(char)*size);
	if(!flag){ fprintf(stderr,"radio_menu:malloc\n"); return(-1); }
	for(i=0; i<size; i++) flag[i] = 0;
	if( *id >= 0 ) flag[*id] = 1;

	i = menu_popup(disp, screen, size, txt, *id, flag, "* ");
	if( i >= 0 )
		*id = i; /* (-1) means no change */

	free(flag);
	return i;
}


/* mark ON(1) items on left */
/* default position  *id */
int  toggle_menu(Display *disp, int screen, int size, char **txt, int *id, char *flag)
{
	int   i;

	i = menu_popup(disp, screen, size, txt, *id, flag, "* ");
	if( i >= 0 ){
		*id = i; /* (-1) means no change */
		flag[i] = 1 - flag[i];
	}

	return i;
}

#if 0
/* mode 0:simple  1:radio  2:toggle */
int  sample_menu(Display *disp, int screen, int mode)
{
	static char *simple_txt[5] = {"0:hello","1:good bye","2:hi","3:help","4:oh!"};
	static int   simple_id = -1;
	static char *radio_txt[5] = {"cocolo","kiss","osaka","NHK","alpha"};
	static int   radio_id = 2;
	static char *toggle_txt[5] = {"bold","italic","underline","dim","red"};
	static char  toggle_flag[5] = {0,0,0,0,0};
	static int   toggle_id = 2;
	int id;

	if( mode == 0 ){
		id = simple_menu(disp,screen, 5, simple_txt, simple_id);
		printf("simple_menu %d %s\n",id, (id<0)?"(none)":simple_txt[id]);
	}
	else if( mode == 1 ){
		id = radio_menu(disp,screen, 5, radio_txt, &radio_id);
		printf("radio_menu %d %s\n",id,(radio_id<0)?"(none)":radio_txt[radio_id]);
	}
	else if( mode == 2 ){
		id = toggle_menu(disp,screen, 5, toggle_txt, &toggle_id, toggle_flag);
		printf("toggle_menu %d %s\n",id,(toggle_id<0)?"(none)":toggle_txt[toggle_id]);
	}
	else id = -1;
	fflush(stdout);
	return id;
}
#endif
