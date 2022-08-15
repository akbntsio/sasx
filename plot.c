#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include "sas.h"
#include "wave.h" /* sas_draw_clip_y */
#include "plot.h"

#define NUMCOLOR    8
static char *colorname[NUMCOLOR] =
	{ "#ff2020","#00a000","#0000a0", "#000000", "#a00000", "#80ff80", "#8080ff", "#404040"};
#define NUMDASH     5
static char *dashstr[NUMDASH] =
	{ NULL, "\012\002\377\377", "\006\002\377\377", "\003\001\377\377", "\001\001\377\377" };

GC sas_make_gc(SasProp *obj, char *cname)
{
	int         col;
	XGCValues   gcv;
	GC          gc;

	col = sas_color(obj, cname);
	gcv.foreground = col;
	gcv.line_width = 1;
	gcv.function = GXcopy;
	gcv.line_style = LineSolid; /* LineSolid,LineOnOffDash,LineDoubleDash */
	gcv.font = obj->win.font;
	gcv.fill_style = FillSolid;
	gc = XCreateGC(obj->win.disp, obj->win.win,
		GCForeground | GCLineWidth | GCFunction | GCLineStyle |\
		GCFont | GCFillStyle ,
		&gcv);

	return gc;
}

void
sas_change_gc(SasProp *obj, GC gc, char *cname, char *dashstr)
{
	int         col;
	XGCValues   gcv;

	col = sas_color(obj, cname);
	gcv.foreground = col;
	if( !dashstr ){
		gcv.line_style = LineSolid;
		XChangeGC(obj->win.disp, gc, GCForeground|GCLineStyle, &gcv);
	} else {
		gcv.line_style = LineOnOffDash;
		XChangeGC(obj->win.disp, gc, GCForeground|GCLineStyle, &gcv);
		XSetDashes(obj->win.disp, gc, 0, dashstr, 2);
	}
}

void sas_free_gc(SasProp *obj, GC gc)
{
	XFreeGC(obj->win.disp, gc);
}

/* 凡例を表示 */
int sas_draw_legend(SasProp *obj, GC gc, int x, int y, char *str)
{
	char  *p, tmp[3];
	XGCValues   gcv;
	int   mode = 0;

	for( p=str; *p; ){
		if( (*p)&0x80 && (*(p+1))&0x80 ){
			tmp[0] = (*p) & 0x7f;
			tmp[1] = (*(p+1)) & 0x7f;
			if( mode != 16 ){
				gcv.font = obj->win.font16;
				XChangeGC(obj->win.disp, gc, GCFont, &gcv);
				mode = 16;
			}
			XDrawString16(obj->win.disp, obj->win.win, gc,
				x, y, (XChar2b*)tmp, 1);
			XDrawString16(obj->win.disp, obj->win.pix, gc,
				x, y, (XChar2b*)tmp, 1);
			x += XTextWidth16(obj->win.font_struct16, (XChar2b*)tmp, 1);
			p+=2;
		} else {
			tmp[0] = (*p) & 0x7f;
			if( mode != 8 ){
				gcv.font = obj->win.font;
				XChangeGC(obj->win.disp, gc, GCFont, &gcv);
				mode = 8;
			}
			XDrawString(obj->win.disp, obj->win.win, gc, x, y, tmp, 1);
			XDrawString(obj->win.disp, obj->win.pix, gc, x, y, tmp, 1);
			x += XTextWidth(obj->win.font_struct, tmp, 1);
			p ++;
		}
	}
	return x;
}

/* プロットファイルの最大Xを返す */
float sas_plot_xmax(char *fname)
{
	FILE       *fp;
	float	    fx, fy, xmax;
	char        str[256], key[256], cmnt[256];
	if( !(fp=fopen(fname,"r")) ){
		fprintf(stderr,"can't open %s\n",fname);
		return 0;
	}
	xmax = 0;
	while( fgets(str,256,fp) ){
		key[0] = 0;
		if( sscanf(str,"%f %f %s",&fx, &fy, cmnt) < 2 
		&&  sscanf(str,"%s %f %f %s\n",key,&fx,&fy,cmnt) < 3 )
			continue;
		if( key[0] == '#' )
			continue;
		if( xmax < fx )
			xmax = fx;
	}
	fclose(fp);
	return xmax;
}

/* プロットデータを描画する */
void sas_draw_plot(SasProp *obj)
{
	FILE       *fp;
	char        str[256], cmnt[256], setname[256], key[256], *p;
	int         set=0, num=0;
	float       vs, vn;
	float       ymin, ymax, yrange;
	int         h, ww, w, lblh, scalew, scaleh;
	float	    fx, fy;
	int         x1=0, y1=0, x2, y2, a;
	GC          gc;
	int         mk;
	int         legy, legx, legnew;
	int         move;

	if( !(fp=fopen(obj->file.name,"r")) ){
		fprintf(stderr,"can't open %s\n",obj->file.name);
		return;
	}

	/* 表示範囲を確認 */
	vs = obj->view.sview;
	vn = obj->view.nview;
	ymax = obj->view.ymax;
	ymin = obj->view.ymin;
	yrange = ymax - ymin;
	lblh = (obj->nlbl)? obj->win.lblh:0;
	scaleh = obj->win.scaleh;
	scalew = obj->win.scalew;
	h = obj->win.size.height - scaleh*2 - lblh;
	ww = obj->win.size.width;
	w = ww - scalew;
	mk = obj->win.plotmk;     /* マーカ(△印)の大きさ */
	legx = scalew + 3;            /* 凡例のX座標 */
	legy = scaleh * 2;            /* 凡例のY座標 */
	setname[0] = 0;
	obj->file.size = 2;

	gc = sas_make_gc(obj, colorname[0]);
	/* 最初 */
	while( fgets(str,256,fp) ){
		if( (p=strchr(str,'\n')) ) *p = '\0';

		/* 空行を見付けてセットを切替える */
		if( sscanf(str,"%s",cmnt)<=0 ){
			if( num ) {		/* セット終了. 次のセット */
				set++;
				num=0;
				sas_change_gc(obj,gc,colorname[set%NUMCOLOR],
					dashstr[(set/NUMCOLOR)%NUMDASH]);
				setname[0] = 0;
			}
			continue;
		}

		/* プロットデータを読み込む */
		move = 0;
		if( sscanf(str,"%f %f %s",&fx, &fy, cmnt) < 2 ){
			if( str[0] == '"' ){
				strcpy(setname,&str[1]); /* セット名 */
				continue;
			}
			if( sscanf(str,"%s %f %f %s\n",key,&fx,&fy,cmnt) < 3 )
				continue;
			if( !strcmp(key, "move") )
				move = 1;
		}

		/* ファイルサイズ情報を変更しよう */
		if( (int)fx+1 > obj->file.size )
			obj->file.size = (int)fx+1;

		/* さてと、では表示 */
		x2 = scalew + w*(fx-vs)/vn;
		y2 = scaleh + h*(ymax-fy)/yrange;
		if( num > 0 ){
			if( (scalew<=x1 && x1<=ww) || (scalew<=x2 && x2<=w) ){
				if( x1 == x2 )
					a = (y2-y1)?(-1):1; /* 線の途切れ防止 */
				else
					a = 0;
				if( !move )
					sas_draw_clip_y(obj, gc, x1,y1, x2, y2+a, scaleh, scaleh+h );
			}
		} else if( *setname ){
			/* 最初のデータをプロットする時は凡例を表示 */
			legnew = sas_draw_legend(obj, gc, legx, legy, setname);
			sas_draw_clip_y(obj, gc, legx,legy+1, legnew,legy+1, scaleh, scaleh+h );
			legx = legnew + 2;
		}
		/* マーカ(△印)を表示する */
		if( mk > 0 ){
			if( (scalew<=x2 && x2<=w) ){
				sas_draw_clip_y(obj, gc, x2,y2-mk, x2+mk,y2+mk, scaleh, scaleh+h );
				sas_draw_clip_y(obj, gc, x2,y2-mk, x2-mk,y2+mk, scaleh, scaleh+h );
				sas_draw_clip_y(obj, gc, x2+mk,y2+mk, x2-mk,y2+mk, scaleh, scaleh+h );
			}
		}
		x1 = x2; y1 = y2;
		num++;
	}
	XFlush(obj->win.disp);
	sas_free_gc(obj,gc);
	fclose(fp);
}

