/* sas window */
/*
980421	draw_scale (sample)がうまくうごくようになった。時刻も表示したい
	font を指定できるようにしなくては
980425	セレクト領域のつじつまはあうようになった
	ssel<0(何も選択されてない) ssel>=0(何か選択されている)
	nsel=0(ポイントだけ選択) nsel!=0(エリアが選ばれている.符号あり)
980426	Keyイベントz Z a でズーム f F b B で移動するように
980430	amp も snap されるように(でもまだ４捨５入になってない)
980401	parse_geometry でサイズが変えられるように等
980505	create,realize を変えて色を変えたりできるように等(mainはまだ)
	sel_all のバグとり, print_sel とりあえず
980505	振幅スケールを描く. -s で初期選択領域指定. print_sel print_pos なおす
980507	b2press b3press 代入だけしておく
980510	win.mk, win.mkgc つくる
980613  cross_hair(obj obj) 引数をobj にして周波数も描けるように
991207	wave.c 別にする
以下省略

*/

#include <stdio.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include "sas.h"
#include "wave.h"
#include "spectro.h"
#include "plot.h"
#include "label.h"
#include "menu.h"
#include "datatype.h"

#ifdef WM_CLOSE
//Atom   WM_PROTOCOLS, WM_DELETE_WINDOW;
Atom   WM_DELETE_WINDOW;
int    wm_delete_init = 0;
#endif

int global_shift = 0;
int global_ctrl = 0;
int global_meta = 0;
int global_alt = 0;

/* =================================================================
オブジェクトの生成(sas_realizeするまで表示はしない)
使い方
	SasProp* obj = sas_create( disp )
	... この間で変更を加えることができる
	sas_realize( obj );
================================================================= */
void *sas_create( Display *disp )
{
	SasProp    *obj;

	/* ALLOC */
	if( ! (obj=malloc(sizeof(SasProp))) ){
		fprintf(stderr,"can't alloc sas object\n");
		return 0;
	}

	/* FILE PROPERTY */
	obj->file.name[0] = '\0';
	obj->file.type = 'N';
	obj->file.unit = 2;
	obj->file.endian = 'L';	/* L or B */
	obj->file.chan = 1;
	obj->file.freq = 12000;
	obj->file.filter[0] = 0;
/*	obj->file.fd = (-1); */
	obj->file.size = 0;
	obj->file.stat = 0;
	obj->file.hsize = 0;
	obj->file.offset = 0;

	/* WINDOW PROPERTY */
	obj->win.disp = disp;
	obj->win.screen = DefaultScreen(disp);
	obj->win.parent = DefaultRootWindow(obj->win.disp);
	obj->win.win = 0;
	obj->win.id = 0;
	strcpy(obj->win.name,"sas");
	strcpy(obj->win.fontname, "fixed");
	strcpy(obj->win.fontname16, "k14");
	obj->win.size.x = 0;
	obj->win.size.y = 0;
	obj->win.size.width = 512;
	obj->win.size.height = 128;
	obj->win.size.flags = USPosition|USSize; /* enables -geometry */
	obj->win.bw = 0; /* 窓枠 */
	obj->win.mw = 1; /* マーカ幅 */
	obj->win.csmgn = 4; /* カーソル近傍マージン */
	obj->win.plotmk = 0; /* プロット用マーカサイズ */
	obj->win.csmode = -1; /* 最初はなんでもない */

	/* デフォルト色 */
	obj->win.fg = "black";
	obj->win.bg = "white";
	obj->win.bd = "black";	/* bw=0なのでほとんど使わない */
	obj->win.mk = "darkblue";
	obj->win.cs = "red";
	obj->win.sc = "darkgreen";
	obj->win.dim = "gray";

	/* do not change */
	obj->win.pix = (Pixmap)NULL;	/* 必要な時にalloc */

	/* 各種分析パラメータ */
	obj->view.mode = MODE_WAVE;
	obj->view.frameskip = 10.0;	/* 表示用のみ */
	obj->view.framesize = 20.0;	/* analysis window用 */
	obj->view.fscale = FSC_LINEAR; /* share wish ana */

	obj->view.daloop = 0;
	obj->view.nosync = 0;
	obj->view.sview = 0;
	obj->view.nview = 1000;
	obj->view.ssel = 0;
	obj->view.nsel = 0;
	obj->view.sfreq = 0;
	obj->view.efreq = -1;
	obj->view.fsel = 0;
	obj->view.xscale = SCALE_TIME;
	obj->view.ymin = -32768;
	obj->view.ymax =  32768;
	obj->view.vskip  = 100;
	obj->view.vskips = 10;
	obj->view.cpuendian = cpuendian();
	obj->view.cson = 0;
	obj->view.b1press = 0;
	obj->view.b2press = 0;
	obj->view.b3press = 0;
	obj->view.b1sel   = 0;

	obj->view.frameskipwb = 0.5;
	obj->view.framesizewb = 5.0;
	obj->view.narrow = 0;   /* default wide band */
	obj->view.frameskipnb = 10.0;
	obj->view.framesizenb = 100.0;
	obj->view.daon = 0;
	obj->view.marker = -1;	/* sample < 0 not shown */
	obj->view.markerstr[0] = '\0';
	obj->view.mx = 0;
	obj->view.my = 0;
	obj->view.spp = 1;

	/* SPECTROGRAM */
	obj->view.spow  = 0.35;  /* pow((H(w)**2)/size,spow)*sgain) */
	obj->view.sgain = 2000;  /* gain factor saturation amplitude */
	obj->view.smax =  -1;  /* gain factor saturation amplitude */
	obj->view.pre = 0.95;
	obj->view.fzoom = 1;

	/* PITCH */
	obj->view.pitch = 0;		/* 0:off */
	obj->view.pitchsft = 5;		/* ms */
	obj->view.pitchroot = 3.0;	/*  */
	obj->view.pitchup = 4;
	obj->view.pitchmin = 40;	/* Hz */
	obj->view.pitchmax = 500;	/* Hz */
	obj->view.pitchthr = 0.12;	/* c[p]/c[0] */
	obj->view.pitchfile[0] = 0;
	obj->view.pitchsave[0] = 0;
	obj->view.pitchbuff = 0;
	obj->view.pitchnum = 1;

	/* POWER */
	obj->view.power = 1;		/* 0:off */
	obj->view.powerh = 20;		/* pixel  */

	/* LBL */
	obj->lblmode = 0;         /* 0:区間とセット 1:詰めたりする */
	obj->lbltype = LBL_AUTO;         /* 0:auto 1:seg 2:ATR 3:CVI */
	obj->lblarc  = 0;         /* 補助線引く */
	obj->nlbl = 0;
	obj->lbl = NULL;
	obj->lblfile[0] = '\0';
	obj->lblsave[0] = '\0';
	obj->clbl = -1;			/* selecting label (-1):not */
	obj->mlbl = 0;			/* editmode 0:no 1:grab 2:editing */
	obj->albl = -1;
	obj->dlbl = -1;
	obj->std = 0;			/* editmode 0:no 1:start 2:label 3:end */

	/* OTHER ANALYSIS */
	obj->view.anapre = 0.98;
	obj->view.anapref = 1;
	obj->view.analagbw = 0.01;
	obj->view.analpc = 16;
	obj->view.anacep = 12;
	obj->view.anafbnum = 24;
	obj->view.analifter = 22;
	obj->view.single = 0;
	obj->view.anawarp = 0.38;  /* 0.37:12k */
	obj->view.anawin = HAMMING; /* HAMMING, SQURARE */
	obj->view.anapow = 1; /* display power */
	obj->view.powmode = 0; /* 0:cor[0]  1:調整パワー */
	obj->view.mfcctilt = 0; /* MFCC傾き補正 0:なし 1:あり */
	obj->view.fmin = 0;
	obj->view.fmax = -1;
	obj->view.mmin = 0;
	obj->view.mmax = -1;
	obj->view.grayscale = 0;

	return  obj;
}

/* =================================================================
波形ウィンドウを見せる
================================================================= */
int  sas_realize( SasProp *obj )
{
	XGCValues   gcv;
	int fg,bg,bd,cs,sc,mk,dim;	/* COLOR */
	int sas_color(SasProp*, char*);

	if( !obj ){
		fprintf(stderr,"illegal sas object %lx\n", (unsigned long)obj);
		return -1;
	}

	/* 色を作る */
	obj->win.cmap = XDefaultColormap(obj->win.disp,obj->win.screen);
	fg  = sas_color(obj, obj->win.fg);
	bg  = sas_color(obj, obj->win.bg);
	bd  = sas_color(obj, obj->win.bd);
	cs  = sas_color(obj, obj->win.cs);
	sc  = sas_color(obj, obj->win.sc);
	mk  = sas_color(obj, obj->win.mk);
	dim = sas_color(obj, obj->win.dim);

	/* 窓を作る */
	obj->win.win = XCreateSimpleWindow(
		obj->win.disp, obj->win.parent,
		obj->win.size.x, obj->win.size.y,
		obj->win.size.width, obj->win.size.height,
		obj->win.bw, bd, bg );

	XSetStandardProperties(
		obj->win.disp, obj->win.win,
		obj->win.name, obj->win.name,
		None,0/*argv*/,0/*argc*/,&obj->win.size );

#ifdef WM_CLOSE
	//WM_PROTOCOLS = XInternAtom(obj->win.disp, "WM_PROTOCOLS", False);
	if (wm_delete_init == 0) {
		WM_DELETE_WINDOW = XInternAtom(obj->win.disp, "WM_DELETE_WINDOW", False);
		wm_delete_init = 1;
	}
	XSetWMProtocols(obj->win.disp, obj->win.win, &WM_DELETE_WINDOW, 1);
	//XChangeProperty(obj->win.disp, obj->win.win,
	//	WM_PROTOCOLS, XA_ATOM, 32, PropModeReplace, (unsigned char *) &WM_DELETE_WINDOW, 1);
#endif


	XMoveResizeWindow(obj->win.disp, obj->win.win,
		obj->win.size.x,obj->win.size.y,
		obj->win.size.width,obj->win.size.height);

	sas_calc_spp(obj);

	/* カーソルを変えて遊ぶ */
	obj->win.csfont[CS_NORMAL] = XCreateFontCursor(obj->win.disp, XC_gumby);
	obj->win.csfont[CS_LR] = XCreateFontCursor(obj->win.disp, XC_sb_h_double_arrow);
	obj->win.csfont[CS_UD] = XCreateFontCursor(obj->win.disp, XC_sb_v_double_arrow);
	obj->win.csfont[CS_TEXT] = XCreateFontCursor(obj->win.disp, XC_xterm);
	obj->win.csfont[CS_HAND] = XCreateFontCursor(obj->win.disp, XC_hand2);
	obj->win.csfont[CS_ARROW] = XCreateFontCursor(obj->win.disp, XC_top_left_arrow);

	/* デフォルトカーソル */
	sas_change_cursor(obj, CS_NORMAL);
	
	/* フォントを割り付ける */
	obj->win.font = XLoadFont(obj->win.disp, obj->win.fontname);
	obj->win.font16 = XLoadFont(obj->win.disp, obj->win.fontname16);
	obj->win.font_struct = XQueryFont(obj->win.disp,obj->win.font);
	obj->win.font_struct16 = XQueryFont(obj->win.disp,obj->win.font16);
	XTextExtents(
		obj->win.font_struct,"",0,
		&obj->win.fonth,&obj->win.fonta,&obj->win.fontd,
		&obj->win.char_struct );
	XTextExtents(
		obj->win.font_struct16,"",0,
		&obj->win.fonth16,&obj->win.fonta16,&obj->win.fontd16,
		&obj->win.char_struct16 );
	obj->win.scaleh = obj->win.fonta+1;
	obj->win.scalew = XTextWidth(obj->win.font_struct, "-88888", 6);
	obj->win.lblh = (obj->win.fonta > obj->win.fonta16)?obj->win.fonta:obj->win.fonta16;
	obj->win.lblh += 5;	/* 目盛のtic分持ち上げるため */

	/* GC を作る */
	/* 波形は黒 */
	gcv.foreground = fg;
	gcv.function = GXcopy;
	gcv.line_width = 1;
	gcv.fill_style = FillSolid;
	gcv.font = obj->win.font;
	obj->win.fggc = XCreateGC(obj->win.disp,obj->win.win,
		GCForeground | GCLineWidth | GCFunction | GCFillStyle | GCFont ,
		&gcv);

	/* 背景は白 */
	gcv.foreground = bg;
	gcv.function = GXcopy;
	gcv.line_width = 1;
	gcv.fill_style = FillSolid;
	gcv.font = obj->win.font;
	obj->win.bggc = XCreateGC(obj->win.disp,obj->win.win,
		GCForeground | GCLineWidth | GCFunction | GCFillStyle | GCFont ,
		&gcv);

	/* スケールは緑 */
	gcv.foreground = sc;
	gcv.function = GXcopy;
	gcv.line_width = 1;
	gcv.fill_style = FillSolid;
	gcv.font = obj->win.font;
	obj->win.scgc = XCreateGC(obj->win.disp,obj->win.win,
		GCForeground | GCLineWidth | GCFunction | GCFillStyle | GCFont ,
		&gcv);

	/* グリッド */
	gcv.foreground = sc;
	gcv.function = GXcopy;
	gcv.line_width = 1;
	gcv.font = obj->win.font;
	gcv.line_style = LineOnOffDash; /* LineSolid,LineOnOffDash,LineDoubleDash */
	obj->win.grgc = XCreateGC(obj->win.disp,obj->win.win,
		GCForeground | GCLineWidth | GCLineStyle | GCFunction | GCFont ,
		&gcv);
	XSetDashes(obj->win.disp,obj->win.grgc,0,"\001\001\377\377",2);


	/* カーソルはXOR (bgの上でcs色に見えるようにする) */
	gcv.foreground = cs^bg;
	gcv.function = GXxor;
	gcv.line_width = 1;
	gcv.fill_style = FillSolid;
	gcv.line_style = LineSolid; /* LineSolid,LineOnOffDash,LineDoubleDash */
	gcv.plane_mask = fg|bg|cs|mk|dim;
	gcv.font = obj->win.font;
	obj->win.csgc = XCreateGC(obj->win.disp,obj->win.win,
		GCForeground | GCLineWidth | GCLineStyle | GCFunction | GCFillStyle | GCFont | GCPlaneMask,
		&gcv);


	/* マーカもXOR  smk=ソリッド mk=破線 mk16=シフトした破線 */
	gcv.foreground = mk^bg;
	gcv.function = GXxor; /* GXcopy GXxor */
	gcv.line_width = obj->win.mw;
	gcv.fill_style = FillSolid;
	gcv.line_style = LineSolid; /* LineSolid,LineOnOffDash,LineDoubleDash */
	gcv.plane_mask = fg|bg|mk;
	gcv.font = obj->win.font;
	obj->win.smkgc = XCreateGC(obj->win.disp,obj->win.win,
		GCForeground | GCLineWidth | GCLineStyle | GCFunction | GCFillStyle | GCFont | GCPlaneMask,
		&gcv);

	gcv.line_style = LineOnOffDash; /* LineSolid,LineOnOffDash,LineDoubleDash */
	obj->win.mkgc = XCreateGC(obj->win.disp,obj->win.win,
		GCForeground | GCLineWidth | GCLineStyle | GCFunction | GCFillStyle | GCFont | GCPlaneMask,
		&gcv);
	XSetDashes(obj->win.disp,obj->win.mkgc,0,"\010\004\377\377",2);


	gcv.font = obj->win.font16;
	obj->win.mkgc16 = XCreateGC(obj->win.disp,obj->win.win,
		GCForeground | GCLineWidth | GCLineStyle | GCFunction | GCFillStyle | GCFont | GCPlaneMask,
		&gcv);
	XSetDashes(obj->win.disp,obj->win.mkgc16,6,"\010\004\377\377",2);
//	XSetDashes(obj->win.disp,obj->win.mkgc16,0,"\004\002\377\377",2);


	/* 編集用の薄い表示 */
	gcv.foreground = dim;
	gcv.function = GXcopy;
	gcv.line_width = 1;
	gcv.fill_style = FillSolid;
	gcv.font = obj->win.font;
	obj->win.dimgc = XCreateGC(obj->win.disp,obj->win.win,
		GCForeground | GCLineWidth | GCFunction | GCFillStyle | GCFont ,
		&gcv);

	/* それではウィンドウの登場です */
	XMapRaised(obj->win.disp, obj->win.win);
	XFlush(obj->win.disp);
	XSelectInput(obj->win.disp,obj->win.win,
		ButtonPressMask|ButtonReleaseMask|StructureNotifyMask|
		EnterWindowMask|LeaveWindowMask|PointerMotionMask|
		KeyPressMask|KeyReleaseMask|ExposureMask|FocusChangeMask);
	obj->view.redraw = 1;

	/* 表示周波数に指定があれば周波数ズームを設定 */
	if( obj->view.smax > 0 && obj->view.smax < obj->file.freq/2 ){
		obj->view.fzoom = obj->file.freq/2/obj->view.smax;
	}

	/* 実は指定通りに窓が開いてない事もある */
	sas_check_resize(obj);

	/* タイトル表示 */
	if( !obj->win.name[0] ){
		if( obj->file.name[0] )
			sprintf(obj->win.name,"%s : sasx[%d]",obj->file.name, obj->win.id);
		else
			sprintf(obj->win.name,"nofile : sasx[%d]", obj->win.id);
	}
	sas_set_title(obj);

	/* 描画 */
	/* ここで redraw しなくてもexpose イベントが出て redrawされるけど */
	sas_redraw(obj);

	return 0;
}

/* =================================================================
SAS窓を捨てる
================================================================= */
void sas_destroy( SasProp *obj )
{
	if( ! obj ) return;
	if( obj->win.win ){ 
		XDestroyWindow(obj->win.disp, obj->win.win);
		if( obj->win.pix )
			XFreePixmap(obj->win.disp, obj->win.pix);
		if( obj->win.fggc )
			XFreeGC(obj->win.disp, obj->win.fggc);
		if( obj->win.bggc )
			XFreeGC(obj->win.disp, obj->win.bggc);
		if( obj->win.scgc )
			XFreeGC(obj->win.disp, obj->win.scgc);
		if( obj->win.csgc )
			XFreeGC(obj->win.disp, obj->win.csgc);
		if( obj->win.dimgc )
			XFreeGC(obj->win.disp, obj->win.dimgc);
	}
	free(obj);
	return;
}

/* =================================================================
文字列で与えられた色を返す
	"red" でも "#ff0000" でもいけるはず。
================================================================= */
int sas_color(SasProp *obj, char *name){
	XColor      color, exact;
	XAllocNamedColor(obj->win.disp, obj->win.cmap, name, &exact, &color);
	return color.pixel;
}

/* =================================================================
サイズを変える
================================================================= */
void  sas_resize(SasProp *obj, int w, int h)
{
	obj->win.size.width = w;
	obj->win.size.height = h;
	if( obj->win.win )
		XResizeWindow(obj->win.disp, obj->win.win, w, h);
}
/* =================================================================
場所を変える
================================================================= */
void  sas_move(SasProp *obj, int x, int y)
{
	obj->win.size.x = x;
	obj->win.size.y = y;
	if( obj->win.win )
		XMoveWindow(obj->win.disp, obj->win.win, x, y);
}
/* =================================================================
サイズと場所を変える
================================================================= */
void  sas_move_resize(SasProp *obj, int x, int y, unsigned int w, unsigned int h)
{
	obj->win.size.width = w;
	obj->win.size.height = h;
	obj->win.size.x = x;
	obj->win.size.y = y;
	if( obj->win.win )
		XMoveResizeWindow(obj->win.disp, obj->win.win, x, y, w, h);
}

/* =================================================================
引数のサイズに合わせる
  指定になければ元の値のまま
================================================================= */
void sas_parse_geometry(SasProp *obj, char *geom)
{
	int          x,y;
	unsigned int w, h;
	x = obj->win.size.x;
	y = obj->win.size.y;
	w = obj->win.size.width;
	h = obj->win.size.height;
	XParseGeometry(geom, &x, &y, &w, &h);
	sas_move_resize(obj, x, y, w, h);
}




/* =================================================================
新しいファイルに切替える
ファイルは実際に描画する時だけオープンする
================================================================= */
int sas_file_open(SasProp *obj, char *fname)
{
	int        size;
	DATAINFO   datainfo;
	if( ! fname || ! strlen(fname) || (size=filesize(fname))<0 ) {
		/* NO FILE */
		obj->file.stat = 0;
		obj->file.size = 0;
		obj->file.name[0] = 0;
		obj->view.ssel = 0;
		obj->view.nsel = 0;
		obj->view.redraw = 1;
		sprintf(obj->win.name,"nofile : sasx[%d]",obj->win.id);
		sas_set_title(obj);
		sas_redraw(obj);
		return 0;
	} 
	obj->file.stat = 1;
	get_datainfo(fname,&datainfo);
	if( obj->file.type == 'N'
	    && datainfo.type != ERR
	    && datainfo.type != RAW ){
		/* つまり WAV or AU */
		/* ここへは type=='N'で最初の時しか来ない */
		obj->file.hsize = datainfo.headerSize; 
		obj->file.endian = datainfo.endian;
		obj->file.unit = datainfo.unit;
		obj->file.chan = datainfo.chan;
		obj->file.freq = datainfo.freq;
		if( obj->file.unit == 1 ) obj->file.type = 'U';
		else                      obj->file.type = 'S';
		obj->file.size = datainfo.dataSize/obj->file.unit/obj->file.chan;
	} else if( obj->file.type == 'P' ){
		obj->file.size = sas_plot_xmax(fname);
	} else if( obj->file.type == 'A' ){
		obj->file.size = ascii_filesize(fname);
		obj->file.size /= obj->file.chan;
	} else {
		if     ( obj->file.type == 'U' ) obj->file.unit = 1;
		else if( obj->file.type == 'C' ) obj->file.unit = 1;
		else if( obj->file.type == 'S' ) obj->file.unit = 2;
		else if( obj->file.type == 'L' ) obj->file.unit = 4;
		else if( obj->file.type == 'F' ) obj->file.unit = 4;
		else if( obj->file.type == 'D' ) obj->file.unit = 8;
		else {
			obj->file.type = 'S';
			obj->file.unit = 2;
		}
		obj->file.size = size/obj->file.unit/obj->file.chan;
	}
	if( obj->file.type == 'C' ){
		obj->view.ymax = 128;
		obj->view.ymin = -128;
	} else if( obj->file.type == 'U' ){
		obj->view.ymax = 256;
		obj->view.ymin = 0;
	}
	strcpy(obj->file.name,fname);
	sprintf(obj->win.name,"%s : sasx[%d]",fname, obj->win.id);
	obj->view.sview = 0;
	obj->view.nview = obj->file.size;
	if( obj->view.nview == 0 ) /* ファイル長が 0 の時などに落ちないよう */
		obj->view.nview = obj->file.freq/10;
	if( obj->view.nview == 0 ) /* さらに周波数が 0 でも落ちないよう */
		obj->view.nview = 1000;
/*
	obj->view.ssel = 0;
	obj->view.nsel = 0;
*/
	obj->view.redraw = 1;
	sas_calc_spp(obj);
	sas_set_title(obj);
	sas_redraw(obj);
	return 0;
}

/* =================================================================
ファイルを開けない状態にする
================================================================= */
int sas_file_close(SasProp *obj)
{
	obj->file.stat = 0;
	obj->file.name[0] = 0;
	sprintf(obj->win.name,"nofile : sas[%d]",obj->win.id);
	sas_set_title(obj);
	obj->view.redraw=1;
	sas_redraw(obj);
	return 0;
}


/* =================================================================
ファイルサイズを調べる
================================================================= */
int filesize(char *fname)
{
	struct stat	stt;
	if( stat(fname,&stt) )		/* normally 0 otherwise -1 */
		return( -1 );
	else
		return( stt.st_size );
}

/* =================================================================
マシンのネイティブ形式(LかB)を調べる
================================================================= */
int cpuendian()
{
	union {char c[2]; short s;} u;
	u.s = 1;
	if( u.c[0] ) return 'L';
	return 'B';
}

/* =================================================================
バッファの内容をバイトスワップする
================================================================= */
void swapbyte(int size, int unit, char *buff)
{
	int i, b, j, k, t;
	if( unit <= 1 ) return;
	for( i=0; i<size; i+=unit ){
		for(b=0,k=i+unit; b<unit/2; b++){
			j = i + b;
			k--;
			t = buff[j];
			buff[j] = buff[k];
			buff[k] = t;
		}
	}
}

/* =================================================================
ウィンドウのタイトルをセットする
================================================================= */
void sas_set_title(SasProp *obj)
{
	if( ! obj->win.win ) return;
	XSetStandardProperties(obj->win.disp,obj->win.win,
		obj->win.name,obj->win.name,
		None,0/*argv*/,0/*argc*/,0/*size*/);
}

/* =================================================================
ピクセルあたりのサンプル数を計算する
================================================================= */
void sas_calc_spp(SasProp *obj)
{
	int width = obj->win.size.width - obj->win.scalew;
	obj->view.spp = (float)(obj->view.nview)/(width-1);
	obj->view.csbias = (int)(1.0/obj->view.spp)/2;
}


/* =================================================================
表示エリアを指定する
	はみ出すような表示でもやろうとしてしまう
	長さがマイナスの時は始点終点を逆にする
	始点がマイナスの時は０にする
================================================================= */
int  sas_set_view(SasProp *obj, int start, int len)
{
	extern void sel_end_windows(SasProp *obj);

	if( len < 0 ){
		start = start + len;
		len = -len;
	}
#if 1
	if( start < -obj->file.offset )
		start = -obj->file.offset;
#else
	if( start < 0 )
		start = 0;
#endif
	if( len < 2 )
		len = 2;
	if( start == obj->view.sview && len == obj->view.nview )
		return 0;
	obj->view.sview = start;
	obj->view.nview = len;
	if( ! obj->win.win ) return 0;

	sas_calc_spp(obj);
	sas_move_cursor(obj, obj->view.mx, obj->view.my);
	/* 選択領域変更中の(ボタンが押されている)とき */
	if( obj->view.b1sel ){
		sas_sel_end(obj, obj->view.cssmpl, obj->view.csfreq, obj->view.fsel);
		sel_end_windows(obj);
	}
	obj->view.redraw = 1;
	sas_redraw(obj);
	return 1;
}

/* =================================================================
時間をズームイン
	選択されていたらそこへ
	選択されていなければ半分の領域にズーム
================================================================= */
void sas_zoom_up(SasProp *obj)
{
	if( obj->view.nsel != 0 )
		sas_set_view(obj, obj->view.ssel, obj->view.nsel);
	else
		sas_set_view(obj, obj->view.sview, obj->view.nview/2);
}

/* =================================================================
時間をズームアウト
	倍の領域を表示
================================================================= */
void sas_zoom_down(SasProp *obj)
{
	sas_set_view(obj,obj->view.sview,obj->view.nview*2);
}

/* =================================================================
全区間表示
================================================================= */
void sas_view_all(SasProp *obj)
{
#if 1
	sas_set_view(obj,-obj->file.offset, obj->file.size+obj->file.offset-1);
#else
	sas_set_view(obj,0,obj->file.size-1);
#endif
}
/* =================================================================
右側を表示 vskips(10%)
================================================================= */
void sas_view_right(SasProp *obj)
{
	int nview, skip;
	nview = obj->view.nview;
	skip  = nview * obj->view.vskips / 100;
	//if( obj->view.sview + skip < obj->file.size -1 )
		sas_set_view(obj, obj->view.sview + skip, nview);
}
/* =================================================================
左側を表示 vskips(10%)
================================================================= */
void sas_view_left(SasProp *obj)
{
	int nview = obj->view.nview;
	int skip  = nview * obj->view.vskips / 100;
	sas_set_view(obj, obj->view.sview - skip, nview);
}
/* =================================================================
右側を表示 vskip(100)%
================================================================= */
void sas_skip_right(SasProp *obj)
{
	int size = obj->file.size;
	int nview = obj->view.nview;
	int skip =  nview * obj->view.vskip / 100;
	if( obj->view.sview + skip < size-1 )
		sas_set_view(obj, obj->view.sview + skip, nview);
}
/* =================================================================
左側を表示 vskip(100)%
================================================================= */
void sas_skip_left(SasProp *obj)
{
	int nview = obj->view.nview;
	int skip =  nview * obj->view.vskip / 100;
	sas_set_view(obj, obj->view.sview - skip, nview);
}

/* =================================================================
ゲイン２倍
================================================================= */
void  sas_gain_up(SasProp *obj)
{
	float ymax, ymin, center;
	ymax = obj->view.ymax;
	ymin = obj->view.ymin;
	center = (ymax + ymin)/2;
	if( strchr("CSL",obj->file.type) && ymax - ymin < 4 ) return;
	obj->view.ymax = center + (ymax-center)/2;
	obj->view.ymin = center + (ymin-center)/2;
	obj->view.redraw = 1;
	sas_move_cursor(obj, obj->view.mx, obj->view.my);
	sas_redraw(obj);
}

/* =================================================================
ゲイン１／２倍
================================================================= */
void  sas_gain_down(SasProp *obj)
{
	float ymax, ymin, center;
	ymax = obj->view.ymax;
	ymin = obj->view.ymin;
	center = (ymax + ymin)/2;
//	if( ymax - ymin > 65536 ) return;
	obj->view.ymax = center + (ymax-center)*2;
	obj->view.ymin = center + (ymin-center)*2;
	obj->view.redraw = 1;
	sas_move_cursor(obj, obj->view.mx, obj->view.my);
	sas_redraw(obj);
}

/* =================================================================
周波数１／２倍
================================================================= */
void  sas_fzoom_up(SasProp *obj)
{
/*	if( obj->view.fzoom >= obj->view.framesizewb/8 ) return;*/
/*	obj->view.fzoom *= 2;*/
	obj->view.narrow = 1;
	obj->view.fzoom = (float)obj->file.freq/2/obj->view.pitchmax;
	obj->view.redraw = 1;
	sas_redraw(obj);
}

/* =================================================================
周波数２倍
================================================================= */
void  sas_fzoom_down(SasProp *obj)
{
	if( obj->view.fzoom <= 1 ) return;
/*	obj->view.fzoom /= 2;*/
	obj->view.narrow = 0;
	obj->view.fzoom = 1;
	obj->view.redraw = 1;
	sas_redraw(obj);
}

/* =================================================================
ウィンドウがリサイズされていたらコピー領域を確保し直す
================================================================= */
int sas_check_resize(SasProp *obj)
{
	Window r;
	int minw, minh;
	int x, y, flag=0;
	unsigned int w, h, b, d;

	minw = obj->win.scalew + 1 + SAS_MIN_WIDTH;
	minh = obj->win.scaleh * 2 + SAS_MIN_HEIGHT;
	if( obj->view.mode == MODE_SPECTRO && obj->view.power )
		minh += obj->view.powerh * obj->file.chan;
	XGetGeometry(obj->win.disp,obj->win.win, &r,&x,&y,&w,&h,&b,&d);
	if( w < minw ) {
		w = minw;
		flag = 1;
	}
	if( h < minh ){
		h = minh;
		flag = 1;
	}
	if( flag ) sas_resize(obj, w, h);

	/* IF RESIZED, REALLOC */
	if( flag || obj->win.pix == (Pixmap)NULL ||
	w != obj->win.size.width || h != obj->win.size.height ){
		if( obj->win.pix ) XFreePixmap(obj->win.disp, obj->win.pix);
		obj->win.pix = XCreatePixmap(obj->win.disp,obj->win.win,w,h,d);
		obj->win.size.width = w; obj->win.size.height = h;
		obj->view.redraw = 1;
		sas_calc_spp(obj);
		return 1;
	}
	return 0;
}

/* =================================================================
ウィンドウサイズに合わせて再描画する
================================================================= */
void sas_redraw(SasProp *obj)
{
	if( ! obj->win.win ) return;

	/* サイズが変わったかどうか */
	sas_check_resize(obj);	/* view.redraw=1 if resized */

	/* グラフを書く */
	if( obj->view.redraw ){ /* 真面目に描く */
		sas_clear_win(obj);
		sas_draw_scale(obj);
		sas_draw_wave(obj); /* win, pix 両方に描画 */
		sas_draw_scale(obj);
	} else { /* pix から win にコピー */
		XCopyArea(obj->win.disp, obj->win.pix, obj->win.win,
		 	obj->win.fggc, 0, 0, obj->win.size.width, obj->win.size.height, 0, 0);
		XFlush(obj->win.disp);
	}
/*	sas_draw_scroll(obj);*/

	/* ラベルを書く */
	sas_draw_label(obj,0,obj->nlbl);

	/* 選択エリアを表示 */
	if( obj->view.ssel >= 0 )
		sas_fill_area(obj);

	/* ヘアラインカーソル */
	if( obj->view.cson )
		sas_cross_hair(obj, obj);

	/* da 再生中ならマーカ */
	if( obj->view.marker >= 0 )
		sas_marker(obj, obj->view.marker);
	if( obj->view.markerstr[0] )
		sas_markerstr(obj, obj->view.markerstr);

	return;
}

/* =================================================================
波形またはスペクトルとスケールを再描画する
redraw != 0 なら再描画するが、それ以外はコピーしたものを再表示
================================================================= */
int sas_draw_wave(SasProp *obj)
{
	extern void sas_draw_mfcc_spectrogram(SasProp*);
	/* NO FILE TO DRAW */
	if( obj->file.stat == 0 ){
		XFlush(obj->win.disp);
		return( 0 );
	}

	/* DRAW WAVE by TWO WAYS */
	if( obj->file.type == 'P' /* plot */ ){
		sas_draw_plot(obj);
	}
	else { /* type == {C S L F D} */
		if( obj->view.mode == MODE_WAVE ){
			if( obj->view.spp > 8.0 )
				sas_fast_wave(obj);
			else
				sas_slow_wave(obj);
		}
		else { /* SPECTROGRAM TYPE */
			if( obj->view.mode == MODE_MFCC )
				sas_draw_mfcc_spectrogram(obj);
			else if( obj->view.mode == MODE_SPECTRO )
				sas_draw_spectrogram(obj);
			if( obj->view.pitch == 1 )
				sas_draw_pitch(obj);
		}
	}
	obj->view.redraw = 0;

	return 0;
}

/* =================================================================
ウィンドウをクリア
================================================================= */
void sas_clear_win(SasProp *obj)
{
	int w, h;
	h = obj->win.size.height;
	w = obj->win.size.width;
	XFillRectangle(obj->win.disp, obj->win.win, obj->win.bggc, 0, 0, w, h);
	XFillRectangle(obj->win.disp, obj->win.pix, obj->win.bggc, 0, 0, w, h);
}

/* =================================================================
グラフの種類を順に切替える
================================================================= */
void sas_change_mode(SasProp *obj)
{
	if( obj->view.mode != MODE_SPECTRO ) obj->view.mode = MODE_SPECTRO;
	else obj->view.mode = MODE_WAVE;
	obj->view.redraw = 1;
	sas_redraw(obj);
}

void sas_change_mode2(SasProp *obj)
{
	if( obj->view.mode != MODE_MFCC ) obj->view.mode = MODE_MFCC;
	else obj->view.mode = MODE_WAVE;
	obj->view.redraw = 1;
	sas_redraw(obj);
}

/* =================================================================
スケールの種類を順に切替える
================================================================= */
void sas_change_xscale(SasProp *obj)
{
	(obj->view.xscale) ++;
	if( obj->view.xscale >= SCALE_NO )
		obj->view.xscale = SCALE_TIME;
	obj->view.redraw = 1;
	sas_redraw(obj);
}

void sas_change_fscale(SasProp *obj)
{
	if(obj->view.fscale == FSC_LINEAR) obj->view.fscale = FSC_MEL;
	else if(obj->view.fscale == FSC_MEL) obj->view.fscale = FSC_LINEAR;
	obj->view.redraw = 1;
	sas_redraw(obj);
}

/* =================================================================
スケールの最適TIC間隔を計算
  range:             値の範囲
  width:             表示幅
  maxpix:            TIC間がこれを越えないピクセル数
  mintic:            表示範囲にTIC数がこれより少ないと困る
  tic, tic10, ticlb  最適TIC間隔
================================================================= */
void sas_calc_tic(
	float range, int width, int maxpix, int mintics,
	int *tic, int *tic10, int *ticlb)
{
	double vpp;
	int ltic;

	vpp = range / width;                           /* value per pixel */
	ltic = (int)log10((double)vpp*maxpix);         /* log10(amp/10pix) */
	*tic = (int)pow((double)10.0,(double)ltic);    /* amp/maxpix */
	if( *tic < 1 ) *tic = 1;                       /* 1 10 100 1000 ... */
	*tic10 = (*tic)*10;
	if( (int)((*tic)*5/vpp) < maxpix )
		*tic *= 5;                                  /* 5 50 500 5000 ... */
	else if( (int)((*tic)*2/vpp) < maxpix )
		*tic *= 2;                                  /* 2 20 200 2000 ... */
	*ticlb = *tic10*10;
	if( range/(*ticlb) < mintics ) *ticlb = *tic10*5;
	if( range/(*ticlb) < mintics ) *ticlb = *tic10*2;
	if( range/(*ticlb) < mintics ) *ticlb = *tic10;
	if( range/(*ticlb) < mintics ) *ticlb = *tic10/2;
	if( range/(*ticlb) < mintics ) *ticlb = *tic10/5;
	if( range/(*ticlb) < mintics ) *ticlb = *tic;
}

/* =================================================================
スケールの最適TIC間隔を計算
  range:             値の範囲
  width:             表示幅
  minpix:            TIC間がこれより細かいと困る
  tic:               1,2,5 のどれか
  index10:           実際のtic 間隔は tic * 10**index10
================================================================= */
void sas_calc_tic_real(
	float range, int width, int minpix, int *tic, int *index10)
{
	double vpp;       /* value per pixel */
	double dinx;      /* index */
	double dtic;      /* double precision tic */

	vpp = range / width;                            /* value per pixel */
	dinx = log10((double)vpp*minpix);               /* log10(value/minpix) */
	if( dinx >= 0.0 )
		*index10 = (int)floor(dinx);
	else
		*index10 = (int)floor(dinx)-1;
	dtic = pow((double)10.0,(double)(*index10));    /* dtic < value/maxpix */
	while( dtic*5 < minpix*vpp ){              /* 計算誤差の修正 */
		dtic *= 10;
		*index10 += 1;
	}
	if( dtic > minpix*vpp ) *tic = 1;
	else if( dtic*2 > minpix*vpp ) *tic = 2;
	else  *tic = 5;
	dtic *= *tic;
//printf("%g/%d=%g log10(%g)=%g index=%d dtic=%g\n",range,width,vpp, vpp*minpix, dinx, *index10, dtic);

}

/* =================================================================
スケールを描画
================================================================= */
void sas_draw_scale(SasProp *obj)
{
	int   ch, ybias, nchan, w, h, sw, sh, lh, ph, hh;
	float ymin, ymax, yrange;
	int   yy, xx, ds, x, y;
	long  s;
	//int   amp;
	int   nview, sview, size;
	int   tic, tic10, ticlb, i10;
	float mul;
	long  imin, imax, i;
	float famp;
	int   smin, smax, tmax;
	float spp, app, tau, freq, toff;
	char  str[200], *unitstr;
	extern  float Mel(float);

	nchan = obj->file.chan;
	h = obj->win.size.height;
	w = obj->win.size.width;
	sw = obj->win.scalew;
	sh = obj->win.scaleh;
	ph = (obj->view.mode!=MODE_WAVE && obj->view.power)? obj->view.powerh: 0;
	lh = (obj->nlbl)? obj->win.lblh: 0;
	hh = h - sh * 2 - lh;

	/* 枠を描画 */
	XDrawRectangle(obj->win.disp,obj->win.win,obj->win.scgc,
		sw,sh,w-1-sw,h-sh*2-lh);
	XDrawRectangle(obj->win.disp,obj->win.pix,obj->win.scgc,
		sw,sh,w-1-sw,h-sh*2-lh);
	if( lh ){
		XDrawLine(obj->win.disp,obj->win.win,obj->win.scgc,
			sw,h-sh,w-1,h-sh);
		XDrawLine(obj->win.disp,obj->win.pix,obj->win.scgc,
			sw,h-sh,w-1,h-sh);
	}

	/* 縦軸 */

	if( obj->view.mode != MODE_WAVE ){
		/* スペクトル周波数目盛 */
		ymax = obj->file.freq/2/obj->view.fzoom;
		ymin = 0;
		yrange = ymax - ymin;
		app = (double)yrange/(hh/nchan-ph);
	}
	else {
		/* 振幅目盛 : 振幅０を描画 */
		ymax = obj->view.ymax;
		ymin = obj->view.ymin;
		yrange = ymax - ymin;
		app = (double)yrange/(hh/nchan);           /* amp/pixel */
	}
	(void)app;

	sas_calc_tic_real( yrange, hh/nchan-ph, 3, &tic, &i10);
	mul = pow(10.0,(double)i10);
	imax = ymax / mul;
	imin = (int)((ymin / mul) / tic) * tic;
	if( imin*mul < ymin )
		imin += tic;
	tic10 = 10;
	ticlb = tic*10;  /* max 50 */
	if( ticlb*mul >= yrange/2 ) ticlb = 20;
	if( ticlb*mul >= yrange/2 ) ticlb = 10;
	if( ticlb*mul >= yrange/2 ) ticlb =  5;
	if( ticlb*mul >= yrange/2 ) ticlb =  2;
	if( ticlb*mul >= yrange/2 ) ticlb =  1;

/*
printf("ymax=%g  ymin=%g  ",ymax, ymin);
printf("i10 %3d  tic %3d ticlb %3d\n",i10,tic,ticlb);
fflush(stdout);
*/

	for( ch=0; ch<nchan; ch++ ){
		ybias = sh + hh*(ch+1)/nchan - ph;
		for( i = imin; i <= imax; i += tic ){
			famp = i * mul;
			if( obj->view.mode != MODE_WAVE && obj->view.fscale == FSC_MEL )
				y = ybias - (double)(hh/nchan-ph)
				  * (Mel((float)famp)-ymin) / Mel((float)yrange);
			else
				y = ybias - (double)(hh/nchan-ph)*(famp-ymin)/yrange;
			if( i%tic10 == 0 ) x = 6;
			else if( i%(tic10/2) == 0 ) x = 4;
			else x = 3;
			if( i%ticlb == 0 ){
				char fmt[32];
				if(i10<-4 || i10>3) strcpy(fmt,"%6.0E");
				else if(i10<0) sprintf(fmt, "%%.%df",-i10);
				else strcpy(fmt,"%.0f");
				sprintf(str,fmt,famp);
				yy = y + obj->win.fonta/2;
				if ( ch==0 || y - obj->win.fonta/2 >= ybias - hh/nchan ){
					xx = XTextWidth(obj->win.font_struct, str, strlen(str));
					XDrawString(obj->win.disp, obj->win.win, obj->win.scgc,
						obj->win.scalew-xx-1, yy, str, strlen(str) );
					XDrawString(obj->win.disp, obj->win.pix, obj->win.scgc,
						obj->win.scalew-xx-1, yy, str, strlen(str) );
				}
			}
			if( i==0 ||
			  ( i%ticlb == 0 && obj->view.mode != MODE_WAVE ) ){
				/* スペクトルの時はラベルと同時に横線を入れる */
				XDrawLine(obj->win.disp,obj->win.win,obj->win.scgc, sw,y, w,y);
				XDrawLine(obj->win.disp,obj->win.pix,obj->win.scgc, sw,y, w,y);
			}
			else {
				XDrawLine(obj->win.disp,obj->win.win,obj->win.scgc,
					sw, y, sw+x, y);
				XDrawLine(obj->win.disp,obj->win.pix,obj->win.scgc,
					sw, y, sw+x, y);
			}
		}
	}
	XFlush(obj->win.disp);

	/* 横軸 */

	nview = obj->view.nview;
	if( nview <= 1 ) return;
	sview = obj->view.sview;

	if( obj->file.type == 'P' )
		size = obj->view.sview + obj->view.nview;   /* 上限を窓の端とする */
	else
		size = obj->file.size;      /* 上限をファイルの終りまでとする */

	if( obj->view.xscale == SCALE_TIME || obj->view.xscale == SCALE_HMS ){
		/* 時間目盛 */
		if( obj->view.xscale == SCALE_HMS ) unitstr = " HMS";
		else unitstr = " SEC";
		freq = obj->file.freq;	/* Hz */
		tau = 10000.0 / freq;		/* dms/sample (dms=0.1ms) */
		spp = tau*nview / (w-sw-1);	/* dms/pixel */

		sas_calc_tic( tau*nview, w-sw-1, 8, 3, &tic, &tic10, &ticlb);
#if 1
		/*
			5000->3600(1h),,,, (18/25)
			1000->900(15m),2000->1800(30m) (9/10)
			500s->600s(10m)  (6/5)
			200s->300s (3/2)
			50s->60s,100s->120s (6/5)
			20s->30s (3/2)
			,,,1,2,5,10s->そのまま
		*/
#define TICCONVHMS(t) \
		(t>=50000000)?t/50*36: \
		((t>=10000000)?t/10*9: \
		((t>= 5000000)?t/5*6: \
		((t>= 2000000)?t/2*3: \
		((t>=  500000)?t/5*6: \
		((t>=  200000)?t/2*3: \
		(t))))))
		if( obj->view.xscale == SCALE_HMS ){
			tic = TICCONVHMS(tic);
			tic10 = TICCONVHMS(tic10);
			ticlb = TICCONVHMS(ticlb);
		}
#endif
		smin = (((int)(tau*sview))/tic)*tic; /* dms tic start */
		toff = smin*freq/10000 - sview; /* sample offset of initial tic */
		smax = (int)(tau * size);                 /* maximum dms in the file */
		tmax = (int)(tau * (nview-toff));         /* maximum dms in the view */
	}
	else if( obj->view.xscale == SCALE_SAMPLE ){
		/* サンプル目盛 */
		unitstr = " SMPL";
		spp = obj->view.spp;                        /* sample/pixel */

		sas_calc_tic(nview, w-sw-1, 8, 3, &tic, &tic10, &ticlb);

		smin = ((int)((sview+tic-1)/tic))*tic;
		toff = smin - sview; /* sample */
		smax = size;
		tmax = nview - toff;
	}
	else if( obj->view.xscale == SCALE_FRAME ){
		/* フレーム目盛 */
		unitstr = "FRAME";
		freq = (int)(obj->file.freq * obj->view.frameskip / 1000);
		tau = 1.0 / freq;
		spp = tau * nview / (w-sw-1);

		sas_calc_tic(tau*nview, w-sw-1, 8, 3, &tic, &tic10, &ticlb);

		smin = ((int)((tau*sview +tic-tau)/tic))*tic;
		toff = (int)(smin*freq) - sview; /* sample */
		smax = (int)(tau * size);
		tmax = (int)(tau * (nview - toff));
	}
	else return;

	XDrawString(obj->win.disp, obj->win.win,obj->win.scgc,
			2, h-1, unitstr,strlen(unitstr));
	XDrawString(obj->win.disp, obj->win.pix,obj->win.scgc,
			2, h-1, unitstr,strlen(unitstr));

	yy = obj->win.size.height - sh;
	for(s=smin,ds=0; s<smax && ds<tmax; ds+=tic,s+=tic){
		x = (int)(ds/spp) + sw + toff/obj->view.spp;
		if( x > w-1 ) break;
		if( x < sw ) continue;
		if( (s%tic10)==0 ) y = 5;
		else if( (s%tic10)==tic10/2 ) y = 3;
		else y = 2;
		if( s%ticlb == 0 ){
			if( obj->view.xscale == SCALE_HMS ){
				int h,m,dmsec;
				m = ((int)s/10000)/60; dmsec = s - m*60*10000;
				h = m/60;  m = m - h*60;

				if( h ) sprintf(str,"%d:%d:%g",h,m,(double)dmsec/10000);
				else if( m ) sprintf(str,"%d:%g",m,(double)dmsec/10000);
				else sprintf(str,"%g",(double)dmsec/10000);
			}
			else if( obj->view.xscale == SCALE_TIME )
				sprintf(str,"%g",(double)s/10000); /* 0.1ms -> 1s */
			else if( obj->view.xscale == SCALE_SAMPLE )
				sprintf(str,"%ld",s);
			else 
				sprintf(str,"%ld",s);
			XDrawString(obj->win.disp, obj->win.win, obj->win.scgc,
				x+1, h-1, str, strlen(str) );
			XDrawString(obj->win.disp, obj->win.pix, obj->win.scgc,
				x+1, h-1, str, strlen(str) );
			XDrawLine(obj->win.disp,obj->win.win,obj->win.scgc,
				x, h, x, yy-y);
			XDrawLine(obj->win.disp,obj->win.pix,obj->win.scgc,
				x, h, x, yy-y);
		}
		else {
			XDrawLine(obj->win.disp,obj->win.win,obj->win.scgc,
				x, yy, x, yy-y);
			XDrawLine(obj->win.disp,obj->win.pix,obj->win.scgc,
				x, yy, x, yy-y);
		}
		/* 最後のサンプルから一つだけ目盛を書いて終る */
		if( s%(tic10) == 0 && s >= smax )
			break;
	}

	XFlush(obj->win.disp);
	return;
}

void sas_draw_scroll(SasProp *obj)
{
}

void sas_draw_tags(SasProp *obj)
{
}

/* =================================================================
選択位置を文字で表示
	描画も消去も同じ XOR で描く
================================================================= */
void sas_print_sel(SasProp *obj)
{
	char	str[60];
	int	    a,x;
	float	sms, nms;
	sms = 1000.0 * obj->view.ssel / obj->file.freq;
	nms = 1000.0 * obj->view.nsel / obj->file.freq;
	sprintf(str, "%sSEL:%6d %7.1fms  LEN:%6d %7.1fms",
		(obj->view.nosync)?"*":" ",
		obj->view.ssel, sms, obj->view.nsel, nms);
	a = XTextWidth(obj->win.font_struct, str, strlen(str));
	(void)a;
	x = obj->win.scalew + 1;
	XDrawString(obj->win.disp, obj->win.win, obj->win.mkgc,
				x, obj->win.scaleh-1, str, strlen(str) );
}

void sas_copy_string(SasProp* obj)
{
	char str[128];
	sprintf(str,"%d",obj->view.ssel);
	XStoreBytes(obj->win.disp, str, strlen(str));
}


/* =================================================================
カーソルの位置を文字で表示
	描画も消去も同じ XOR で描く
================================================================= */
void sas_print_position(SasProp *obj)
{
	char	str[60];
	int	a,x;
	float	ms;

	ms = 1000.0 * obj->view.cssmpl / obj->file.freq;
	if( obj->view.mode != MODE_WAVE )
		sprintf(str, "CURR:%6d %7.1fms  FRQ:%6d",
			obj->view.cssmpl, ms, (int)obj->view.csfreq);
	else
		sprintf(str, "CURR:%6d %7.1fms  AMP:%6g",
			obj->view.cssmpl, ms, obj->view.csamp);
	a = XTextWidth(obj->win.font_struct, str, strlen(str));
	x = obj->win.size.width - a - 1;
	XDrawString(obj->win.disp, obj->win.win, obj->win.csgc,
				x, obj->win.scaleh-1, str, strlen(str) );
}

/* =================================================================
マーカー位置を設定
	描画も消去も同じ XOR で描く。区別は呼ぶ側でする
	sample はファイル中のサンプル位置
	-1 で呼ぶと消す
================================================================= */
void sas_marker(SasProp *obj, int sample)
{
	int x;
	obj->view.marker = sample;
	if( sample < 0 ) return;
	sample -= obj->view.sview;
	if( sample >= 0 && sample <= obj->view.nview ){
		x = sample / obj->view.spp + obj->win.scalew;
		XDrawLine(obj->win.disp, obj->win.win, obj->win.mkgc,
		    x, obj->win.scaleh,
		    x, obj->win.size.height - obj->win.scaleh);
		XFlush(obj->win.disp);
	}
}

/* =================================================================
左下隅に表示する文字を設定
	描画も消去も同じ XOR で描く。区別は呼ぶ側でする
================================================================= */
void sas_markerstr(SasProp *obj, char *str)
{
	strcpy(obj->view.markerstr, str);
	if( str[0] ){
		XDrawString(obj->win.disp, obj->win.win, obj->win.mkgc,
		obj->win.scalew + 1, obj->win.size.height - obj->win.scaleh - 1,
		str, strlen(str));
		XFlush(obj->win.disp);
	}
}

/* =================================================================
クロスヘアラインカーソル
	描画も消去も同じ XOR で描く
	ついでにカーソルの位置を表示する
================================================================= */
void sas_cross_hair(SasProp *obj, SasProp *master)
{
	int   smpl;
	float  amp;
	int   scaleh, lblh, hrange, hbase, powh;
	int   nchan, x, y, ch;
	float ymin, ymax, yrange;
	int   frange, f;
	extern float  Mel(float);

	/* TACK X TO THE SAMPLE */
	if( obj->file.freq == master->file.freq )
		smpl = obj->view.cssmpl = master->view.cssmpl;
	else
		smpl = obj->view.cssmpl
		     = (int)((double)master->view.cssmpl*obj->file.freq/master->file.freq);
	x = obj->view.csx = (smpl - obj->view.sview) / obj->view.spp
			+ obj->win.scalew;
	if( x >= obj->win.scalew && x < obj->win.size.width )
		XDrawLine(obj->win.disp, obj->win.win, obj->win.csgc,
			x, obj->win.scaleh,
			x, obj->win.size.height - obj->win.scaleh);

	/* TACK Y TO ... */
	scaleh = obj->win.scaleh;
	lblh = (obj->nlbl)?obj->win.lblh:0;
	powh = (obj->view.power)?obj->view.powerh:0;
	hrange = obj->win.size.height - scaleh*2 - lblh;
	hbase = scaleh;
	nchan = obj->file.chan;
	if( obj->view.mode != MODE_WAVE ){
		/* TACK Y TO THE FREQ */
		frange = obj->file.freq/2/obj->view.fzoom;
		f = obj->view.csfreq = master->view.csfreq;
		for( ch=0; ch<obj->file.chan; ch++ ){
			if( obj->view.fscale == FSC_MEL )
				y = obj->view.csy = hbase + hrange*(ch+1)/nchan - powh
				  - (hrange/nchan-powh)
				  *  Mel((float)f) / Mel((float)frange);
			else
				y = obj->view.csy = hbase + hrange*(ch+1)/nchan - powh
				  - (hrange/nchan-powh) * f / frange;
			if( y >= hbase && y <= hbase + hrange)
				XDrawLine(obj->win.disp, obj->win.win, obj->win.csgc,
				obj->win.scalew, y, obj->win.size.width, y);
		}
	}else{
		/* TACK Y TO THE AMP */
		amp = obj->view.csamp = master->view.csamp;
		ymax = obj->view.ymax;
		ymin = obj->view.ymin;
		yrange = ymax - ymin;
		for( ch=0; ch<obj->file.chan; ch++ ){
			y = obj->view.csy = hbase
				+ hrange*(ch+(ymax-amp)/yrange)/nchan;
			if( y >= hbase && y <= hbase + hrange)
				XDrawLine(obj->win.disp, obj->win.win, obj->win.csgc,
				obj->win.scalew, y, obj->win.size.width, y);
		}
	}
	sas_print_position(obj);
	XFlush(obj->win.disp);
}

/* =================================================================
マウスの位置 x に最も近いサンプル番号(ファイル先頭からの)を求める
================================================================= */
int sas_x_to_sample(SasProp *obj, int x)
{
	int min, max, smpl;
#if 1
	min = -obj->file.offset;
#else
	min = 0;
#endif
	max = obj->file.size;
	smpl = (int)(obj->view.spp * (x + obj->view.csbias - obj->win.scalew) + 0.5);
	smpl += obj->view.sview;
	if( obj->file.type != 'P' ){
		if( smpl <  min ) smpl = min;
		if( smpl >= max ) smpl = max - 1;
	}
	return smpl;
}

/* =================================================================
マウスの位置 y に最も近い振幅の値を求める
================================================================= */
float sas_y_to_amp(SasProp *obj, int y, int *ch)
{
	int   nchan, scaleh, lblh, hrange;
	float amp, ymin, ymax;
	int   chh;

	nchan = obj->file.chan;
	ymax = obj->view.ymax;
	ymin = obj->view.ymin;
	scaleh = obj->win.scaleh;
	lblh = (obj->nlbl)?obj->win.lblh:0;
	hrange = obj->win.size.height - scaleh*2 - lblh;
	chh = hrange / nchan;

	if( y < scaleh ) { *ch = 0; return (int)ymax; }
	if( y > hrange+scaleh ) { *ch = nchan-1; return (int)ymin; }

	*ch  = ((y-scaleh)/chh);
	amp = ymax + ((y-scaleh)%chh) * (float)(ymin-ymax)/chh;
	if( strchr("LSC",obj->file.type)){
		amp = rintf(amp);
	}
	amp = (amp>ymax)?ymax: ((amp<ymin)?ymin:amp);
	return amp;
}

/* =================================================================
マウスの位置 y に最も近い周波数の値を求める
================================================================= */
int sas_y_to_freq(SasProp *obj, int y, int *ch)
{
	int   nchan, scaleh, powh;
	float melf;
	/* float ymin, ymax, yrange;*/
	int   lblh, hmax, hrange, frange, freq;
	int   chh;
	extern float  Mel(float);
	extern float  MelInv(float);

	nchan = obj->file.chan;
	//ymax = obj->view.ymax;
	//ymin = obj->view.ymin;
	//yrange = ymax - ymin;
	scaleh = obj->win.scaleh;
	lblh = (obj->nlbl)?obj->win.lblh:0;
	powh = (obj->view.power)?obj->view.powerh:0;
	hmax = obj->win.size.height - scaleh - lblh;
	hrange = obj->win.size.height - scaleh*2 - lblh;
	chh = hrange / nchan; /* include powh */
	frange = obj->file.freq/2/obj->view.fzoom;

	if( y < scaleh ) { *ch = 0; return frange; }
	if( y > scaleh+hrange ) { *ch = nchan-1; return 0; }

	*ch  = ((y-scaleh)/chh);
	if( *ch >= nchan ) *ch = nchan - 1;
	if( obj->view.fscale == FSC_MEL ){
		melf = (((hmax-y)%chh)-powh) * Mel((float)frange)/(chh-powh);
		freq = MelInv(melf);
	}
	else {
		freq = (((hmax-y)%chh)-powh) * (float)frange/(chh-powh);
	}
  	if( freq < 0 ) freq = 0;
	if( freq > frange ) freq = frange;
	return freq;
}

/* =================================================================
クロスヘアラインカーソルの位置をマウスの位置 x, y から決める
	サンプルのある場所にのみ置く事ができる
================================================================= */
void sas_move_cursor(SasProp *obj, int x, int y)
{
	int ch;

	/* RAW MOUSE POSITION mx,my */
	obj->view.mx = x;
	obj->view.my = y;
	if( x < obj->win.scalew ) x = obj->win.scalew;
	if( x >= obj->win.size.width ) x = obj->win.size.width-1;

	/* GET SAMPLE POSITION */
	obj->view.cssmpl = sas_x_to_sample(obj,x);

	/* GET CH and AMP or FREQ POSITION */
	if( obj->view.mode != MODE_WAVE ){
		obj->view.csfreq = sas_y_to_freq(obj, y, &ch);
		obj->view.csamp = 0;
	} else {
		obj->view.csfreq = 0;
		obj->view.csamp = sas_y_to_amp(obj, y, &ch);
	}
}



/* =================================================================
整数値の符号を返す
================================================================= */
int sign(int i)
{
	if( i<0 ) return(-1);
	if( i>0 ) return(1);
	return(0);
}

/* =================================================================
矩形領域(x1,y1)-(x2,y2)を塗る。座標は x1>x2, y1>y2 でもよい
	普通のXFillRectagleは (x,y,width,height)を指定すると、
	(x,y)-(x+w-1,y+h-1)を塗るので、計算が面倒臭い
================================================================= */
void sas_fill_rect(SasProp *obj, int x1, int y1, int x2, int y2)
{
	int w, h;
	w = obj->win.scalew;
	h = obj->win.scaleh;
	if( x1 < w ) x1 = w;
	if( x2 < w ) x2 = w;
	if( y1 < h ) y1 = h;
	if( y2 < h ) y2 = h;
	w = obj->win.size.width;
	h = obj->win.size.height;
	if( x1 > w ) x1 = w;
	if( x2 > w ) x2 = w;
	if( y1 > h ) y1 = h;
	if( y2 > h ) y2 = h;
	w = x2 - x1;
	h = y2 - y1;
	if( w < 0 ){ x1 = x2; w = -w; }
	if( h < 0 ){ y1 = y2; h = -h; }
	XFillRectangle(obj->win.disp, obj->win.win, obj->win.mkgc, x1, y1, w+1, h+1);
}

/* =================================================================
ch および周波数 hi, lo に対応する y 座標を得る
================================================================= */
int sas_freq_to_y(SasProp *obj, int freq, int ch)
{
	int    h0 = obj->win.scaleh;
	int    hr = obj->win.size.height - obj->win.scaleh * 2;
	int    nchan = obj->file.chan;
	int    niq = obj->file.freq/2;
	int    powh = (obj->view.power)?obj->view.powerh:0;
	float  fzoom = obj->view.fzoom;
	int    y;
	extern  float Mel(float);

	if( obj->nlbl )
		hr -= obj->win.lblh;

	if( freq < 0 ) freq = 0;
	else if( freq > niq ) freq = niq;

	if( obj->view.fscale == FSC_MEL ){
		y = h0 - powh +
		    ( (hr * (ch+1)) -
		      (hr - powh * nchan) * Mel((float)freq) / Mel((float)niq / fzoom)
		    ) / nchan;
	}
	else {
		y = h0 - powh +
		    ( (hr * (ch+1)) -
		      (hr - powh * nchan) * freq * fzoom / niq
		    ) / nchan;
	}
	/* その心は */
	/* h0 + (hr / nchan * (ch+1)) - powh - ((hr / nchan - powh) * freq / niq) */

	return  y;
}

/* =================================================================
リバース表示領域全体の描画、消去を XOR で行う
	現在の ssel, nsel の値から表示する
	sfrom,sto は使っていない(これらはマウスで選択動作中のみ有効)
================================================================= */
void sas_fill_area(SasProp *obj)
{
	int s, e, w, l, h, scalew, ch; 
	w = obj->win.size.width;
	scalew = obj->win.scalew;
	/* 表示位置 sample をもとに計算 */
	s = obj->view.ssel;
	e = obj->view.ssel + obj->view.nsel; /* これでいいかな +1か-1 */
	if( s > e ) { s=s+e; e=s-e; s=s-e; }
	/* 表示エリアに重なる部分があれば表示 */
	if( (s < obj->view.sview + obj->view.nview)
	  && (e > obj->view.sview) ){
		s = (s - obj->view.sview) / obj->view.spp + scalew;
		e = (e - obj->view.sview) / obj->view.spp + scalew;
		if( s < scalew ) s = scalew;
		if( e > w-1 ) e = w-1;
		if( obj->view.mode == MODE_WAVE ||
			obj->view.efreq < 0 || obj->view.sfreq < 0 || !obj->view.fsel ){
			/* 波形または全スペクトル帯域 */
			h = obj->win.scaleh;
			l = obj->win.size.height - obj->win.scaleh;
			sas_fill_rect(obj, s, h, e, l);
		}
		else {
			/* スペクトルの一部分を選択 */
			for(ch=0; ch<obj->file.chan; ch++){
				h = sas_freq_to_y(obj, obj->view.efreq, ch);
				l = sas_freq_to_y(obj, obj->view.sfreq, ch);
				sas_fill_rect(obj, s, h, e, l);
			}
		}
	}
	sas_print_sel(obj);
	XFlush(obj->win.disp);
}

/* =================================================================
選択領域の開始位置をサンプルで指定
	サンプルの位置はカーソルの位置などで既に計算済み
	サンプル数 nsel=0 でリバース表示
	何も選んでいない状態 nsel=-1 へは戻れない
周波数を選択する freq
	ただし、fsel = 0 の時は全帯域とする
================================================================= */
void  sas_sel_start(SasProp *obj, int smpl, int freq, int fsel)
{
	int  x, scalew, w, y;
	scalew = obj->win.scalew;
	w = obj->win.size.width;
	/* 古い反転表示を消去 */
	if( obj->view.ssel >= 0 )
		sas_fill_area(obj);

	/* 表示位置 */
	x = (smpl - obj->view.sview) / obj->view.spp + scalew;
	x = (x<scalew)?scalew:((x>=w)?w-1:x);
	obj->view.sfrom = x;
	obj->view.sto = x;
	y = sas_freq_to_y(obj, freq, 0);
	obj->view.ffrom = y;
	obj->view.fto = y; 

	/* 選択されたサンプル */
	obj->view.ssel = smpl;
	obj->view.nsel = 0;
	obj->view.sfreq = freq;  /* (-1)もそのまま */
	obj->view.efreq = freq;  /* (-1)もそのまま */
	obj->view.fsel = fsel;  /* これによって表示がかわる */

	/* 新しい場所を反転(多ch) */
	sas_fill_area(obj);

/*printf("%5d %5d %5d %5d\r",
obj->view.sfrom,obj->view.sto,obj->view.ssel,obj->view.nsel);
*/
/*
printf("\r(%d %4d %4d)[%4d %4d]",obj->view.fsel,obj->view.ffrom,obj->view.fto,obj->view.sfreq,obj->view.efreq);
fflush(stdout);
*/
}

/* =================================================================
選択領域の終了位置をサンプルで指定
	選択領域のリバース表示をする
fsel を受けるが、途中では変えないようにしている
================================================================= */
void  sas_sel_end(SasProp *obj, int smpl, int freq, int fsel)
{
	int  x, scalew, w, esmpl;
	int  xmotion, past, curr, d3, d4; 
	int  y, ymotion, ch, h, l;

	/* 消す */
	sas_print_sel(obj);

	/* X 表示位置の移動 obj->view.sto -> x */
	scalew = obj->win.scalew;
	w = obj->win.size.width;
	x = (smpl - obj->view.sview) / obj->view.spp + obj->win.scalew;
	x = (x<scalew)?scalew:((x>=w)?w-1:x);
	xmotion = sign(x - obj->view.sto);		/* 動いた方向 */

	/* 横方向 */
	if( xmotion ){
		past = sign(obj->view.sto - obj->view.sfrom); /* かつての方向 */
		curr = sign(x - obj->view.sfrom);	/* こんどの方向 */
		d3 = ( xmotion*past >= 0 )? xmotion: 0;	/* 延びた場合のずれ */
		d4 = ( xmotion*curr >  0 )? 0: -xmotion;	/* 縮んだ場合のずれ */
		if( obj->view.mode == MODE_WAVE || ! obj->view.fsel ){
			h = obj->win.scaleh;
			l = obj->win.size.height - obj->win.scaleh;
			sas_fill_rect(obj, obj->view.sto+d3, h, x+d4, l);
			if( past*curr < 0 )			/* 裏返った時、始点位置を */
				sas_fill_rect(obj, obj->view.sfrom, h, obj->view.sfrom, l);
		}
		else {
			for(ch=0; ch<obj->file.chan; ch++){
				h = sas_freq_to_y(obj, obj->view.efreq, ch);
				l = sas_freq_to_y(obj, obj->view.sfreq, ch);
				if( h > l ) { h = h+l; l = h-l;  h = h-l; }
				sas_fill_rect(obj, obj->view.sto+d3, h, x+d4, l);
				if( past*curr < 0 )			/* 裏返った時、始点位置を再描画 */
					sas_fill_rect(obj, obj->view.sfrom, h, obj->view.sfrom, l);
			}
		}
	}
	obj->view.sto = x;

	/* Y 表示位置の移動 obj->view.ffrom -> y */
	y = sas_freq_to_y(obj, freq, 0);
	ymotion = sign(y - obj->view.fto);
	if( obj->view.mode != MODE_WAVE && obj->view.fsel && ymotion ){
		int   x1, x2;
		x1 = obj->view.sfrom;
		x2 = obj->view.sto;
		if( x1 > x2 ){ x1 = x1+x2; x2 = x1-x2; x1 = x1-x2; }
		for(ch=0; ch<obj->file.chan; ch++){
			int   y0 = sas_freq_to_y(obj, obj->view.sfreq, ch);
			int   y1 = sas_freq_to_y(obj, obj->view.efreq, ch);
			int   y2 = sas_freq_to_y(obj, freq, ch);
			past = sign(y1 - y0); /* かつての方向 */
			curr = sign(y2 - y0);	/* こんどの方向 */
			ymotion = sign(y2 - y1);
			d3 = ( ymotion*past >= 0 )? ymotion: 0;	/* 延びた場合のずれ */
			d4 = ( ymotion*curr >  0 )? 0: -ymotion;	/* 縮んだ場合のずれ */
				sas_fill_rect(obj, x1, y1+d3, x2, y2+d4);
				if( past*curr < 0 )			/* 裏返った時、始点位置を再描画 */
					sas_fill_rect(obj, x1, y0, x2, y0);
		}
	}
	obj->view.fto = y;

	XFlush(obj->win.disp);

	/* サンプル選択値を計算 */
	//int size = obj->file.size;
	//esmpl = (smpl<0)?0:((smpl>=size)?size-1:smpl);
	esmpl = smpl; 
	obj->view.nsel = esmpl - obj->view.ssel;

	/* 周波数選択値を計算 */
	obj->view.efreq = freq;

	/* 表示 */
	sas_print_sel(obj);
/*
printf("x %5d %5d  smpl%5d %5d\n",
obj->view.sfrom,obj->view.sto,obj->view.ssel,obj->view.nsel);
fflush(stdout);
*/
/*
printf("\r(%d %4d %4d)[%4d %4d]",obj->view.fsel,obj->view.ffrom,obj->view.fto,obj->view.sfreq,obj->view.efreq);
fflush(stdout);
*/

}

/* =================================================================
スケールを調整
ボタンが押されるまでは内側なら１外側なら０を返すだけ
ボタンが押されて変更中の時は１
================================================================= */
#define HANDLE   10
int  sas_move_scale(SasProp *obj, int x, int y)
{
	int    h1, h2, inside;
	float  range, motion;

	h1 = obj->win.size.height - obj->win.scaleh;
	if( obj->nlbl ) h1 -= obj->win.lblh;
	h2 = obj->win.scaleh;

	/* カーソル変更 */
	if( (x < HANDLE && h1 > y && y > h2) )
		inside = 1;
	else
		inside = 0;

	/* 押されてない */
	if( !obj->view.b1press ){
		obj->view.dragmode = DR_NONE;
		return inside;
	}

	/* Pressの瞬間 */
	if( !obj->view.b1mot ){
		if( inside ){
			if( global_ctrl ) obj->view.dragmode = DR_VZOOM;
			else obj->view.dragmode = DR_VMOVE;
		}
		return inside;
	}

	/* 範囲外からDragして入って来た 時 */
	if( obj->view.dragmode == DR_NONE ){
		return 0;
	}

	/* 変更Drag時 */
	if( obj->view.mode != MODE_WAVE ){
		if( obj->view.dragmode == DR_VZOOM || obj->view.dragmode == DR_ZOOM ){
			int my = obj->view.my;
			float old = obj->view.fzoom;
			if( y > h1 - 10 ) y = h1 - 10;
			if( my > h1 - 10 ) my = h1 - 10;
				obj->view.fzoom *= (double)(h1 - y) / (h1 - my);
			if( obj->view.fzoom < 1 ) obj->view.fzoom = 1;
			if( obj->view.fzoom > 1000 ) obj->view.fzoom = 1000;
			if( obj->view.fzoom == old ) return 1;
		}
		else return 0;
	}
	else {
		range = obj->view.ymax - obj->view.ymin;
		motion = (double)range * (y - obj->view.my) / (h1 - h2);
		if( obj->view.dragmode == DR_VMOVE || obj->view.dragmode == DR_MOVE ){
			obj->view.ymin += motion;
			obj->view.ymax += motion;
		}
		else if( obj->view.dragmode == DR_VZOOM || obj->view.dragmode == DR_ZOOM ){
			int my = obj->view.my;
			if( y > h1 - 10 ) y = h1 - 10;
			if( my > h1 - 10 ) my = h1 - 10;
			obj->view.ymax = obj->view.ymin + range * (h1 - my) / (h1 - y);
		}
		else return 0;
	}

	obj->view.redraw = 1;
	sas_redraw(obj);

	return 1;
}

/* =================================================================
メニュー処理
================================================================= */
#define   NUM(ppp)  (sizeof(ppp)/sizeof(char*))

/* ファイル属性のメニュー */
void sas_property_menu(SasProp *obj)
{
	/* ファイル */
	static char  offsetStr[50];
	static char  *filetxt[] = {"frequency","endian","channel","dataType",offsetStr,"reset"};
	static int    fileid = -1;
	int           i, j, id;
	Display      *disp = obj->win.disp;
	int           screen = obj->win.screen;

	/* トップメニュー選択 */
	sprintf(offsetStr,"offset (%d)",obj->file.offset);
	id = simple_menu(disp, screen, NUM(filetxt), filetxt, fileid);
	if( id >= 0 ) fileid = id;

	/* サブメニューいろいろ */
	if( id == 0 ){
		/* 周波数 */
		static char *freqtxt[] =
		    { "8000","10000","11025","12000",
		     "16000","20000","22050","24000", "44100","48000"};
		for( i=0; i<NUM(freqtxt); i++ )
			if( obj->file.freq == atoi(freqtxt[i]) ) break;
		if( i == NUM(freqtxt) ) i = -1;
		id = radio_menu(disp, screen, NUM(freqtxt), freqtxt, &i);
		if( id >= 0 ){
			j = atoi(freqtxt[id]);
			if( obj->file.freq != j ){
				obj->file.freq = j;
				obj->view.redraw = 1;
				sas_redraw(obj);
			}
		}
	}
	else if( id == 1 ){
		/* ENDIAN */
		static char *endtxt[2] = {"Little","Big"};
		i = (obj->file.endian == 'B')?1 : 0;
		id = radio_menu(disp, screen, 2, endtxt, &i);
		if( id >= 0 && obj->file.endian != endtxt[id][0] ){
			obj->file.endian = endtxt[id][0];
			obj->view.redraw = 1;
			sas_redraw(obj);
		}
	}
	else if( id == 2 ){
		/* CHAN */
		static char *chtxt[] = {"1","2","3","4","5","6","7","8","9","10","11","12"};
		for(i=0;i<NUM(chtxt); i++)
			if(obj->file.chan==atoi(chtxt[i])) break;
		if( i >= NUM(chtxt) ) i = -1;
		id = radio_menu(disp, screen, NUM(chtxt), chtxt, &i);
		if( id >= 0 && obj->file.chan != atoi(chtxt[id]) ){
			obj->file.chan = atoi(chtxt[id]);
			sas_file_open(obj, obj->file.name);
			/*obj->view.redraw = 1;
			sas_redraw(obj);*/
		}
	}
	else if( id == 3 ){
		/* TYPE */
		static char *typetxt[] = {"UChar","Char","Short","Long","Float","Double","Ascii","Plot"};
		for(i=0;i<NUM(typetxt);i++)
			if(obj->file.type == typetxt[i][0]) break;
		if( i == NUM(typetxt) ) i = -1;
		id = radio_menu(disp, screen, NUM(typetxt), typetxt, &i);
		if( id >= 0 && obj->file.type != typetxt[i][0] ){
			obj->file.type = typetxt[i][0];
			sas_file_open(obj, obj->file.name);
		}
	}
	else if( id == 4 ){
		obj->file.offset += obj->view.ssel;
		obj->file.size -= obj->view.ssel;
		obj->view.ssel = 0;
		obj->view.redraw = 1;
		sas_redraw(obj);
	}
	else if( id == 5 ){
		obj->file.type = 'N';
		obj->view.ssel += obj->file.offset;
		obj->file.offset = 0;
		sas_file_open(obj, obj->file.name);
	}
}

void sas_para_menu(SasProp *obj)
{
	static char  *paratxt[] = {
	  /* 0,1,2 */
	  "frameSize","frameSkip", "frameSizeWB",
	  /* 3,4,5 */
	  "frameSkipWB", "frameSizeNB","frameSkipNB",
	  /* 6,7,8,9 */
	  "pitchMin","pitchMax","pitchThr","pitchNum",
	  /* 10,11,12,13 */
	  "pitchRoot","contrast(sPow)","saturation(sGain)","grayscale"
	};
	static int    paraid = 0;
	Display      *disp = obj->win.disp;
	int           screen = obj->win.screen;
	float         val;
	char         **txt;
	int           mode, num, id, ret;

	mode = simple_menu(disp, screen, NUM(paratxt), paratxt, paraid);
	if( mode < 0 ) return;
	paraid = mode;

	if( mode >= 0 && mode <= 5 ){
		/* SIZE と SKIP 共通化 */
		static char *sizetxt[] =
		  {"2.5ms","5ms","10ms","15ms","20ms","30ms","40ms","100ms","200ms","400ms","1000ms","2000ms","4000ms"};
		static char *skiptxt[] =
		  {"0.1ms","0.5ms","1ms","2ms","2.5ms","5ms","7.5ms","10ms","15ms","20ms","50ms","500ms","1000ms","2000ms"};

		if     ( mode == 0 )
			{ val = obj->view.framesize; txt = sizetxt; num = NUM(sizetxt); }
		else if( mode == 1 )
			{ val = obj->view.frameskip; txt = skiptxt; num = NUM(skiptxt); }
		else if( mode == 2 )
			{ val = obj->view.framesizewb; txt = sizetxt; num = NUM(sizetxt); }
		else if( mode == 3 )
			{ val = obj->view.frameskipwb; txt = skiptxt; num = NUM(skiptxt); }
		else if( mode == 4 )
			{ val = obj->view.framesizenb; txt = sizetxt; num = NUM(sizetxt); }
		else if( mode == 5 )
			{ val = obj->view.frameskipnb; txt = skiptxt; num = NUM(skiptxt); }
		else return;
	
		/* メニュー */
		for(id=0;id<num;id++)
				if( val == (float)atof(txt[id]) ) break;
		if( id == num ) id = -1;
		ret = radio_menu(disp, screen, num, txt, &id);
		if( ret < 0 || id < 0 ) return;

		val = atof(txt[id]);
		if     ( mode == 0 ){ obj->view.framesize = val; }
		else if( mode == 1 ){ obj->view.frameskip = val; }
		else if( mode == 2 ){ obj->view.framesizewb = val; }
		else if( mode == 3 ){ obj->view.frameskipwb = val; }
		else if( mode == 4 ){ obj->view.framesizenb = val; }
		else if( mode == 5 ){ obj->view.frameskipnb = val; }
	}
	else if( mode == 6 ){
		/* PITCH MIN */
		static char *mintxt[] = {"20","40","80","160"};
		for(id=0;id<NUM(mintxt);id++)
			if( obj->view.pitchmin == atoi(mintxt[id]) ) break;
		if( id == NUM(mintxt) ) id = -1;
		ret = radio_menu(disp, screen, NUM(mintxt), mintxt, &id);
		if( ret < 0 || id < 0 ) return;
		obj->view.pitchmin = atoi(mintxt[id]);
	}
	else if( mode == 7 ){
		/* PITCH MAX */
		static char *maxtxt[] = {"250","500(default)","1000","2000","6000"};
		for(id=0;id<NUM(maxtxt);id++)
			if( obj->view.pitchmax == atoi(maxtxt[id]) ) break;
		if( id == NUM(maxtxt) ) id = -1;
		ret = radio_menu(disp, screen, NUM(maxtxt), maxtxt, &id);
		if( ret < 0 || id < 0 ) return;
		obj->view.pitchmax = atoi(maxtxt[id]);
	}
	else if( mode == 8 ){
		/* pitch Thres */
		static char *txt[] = {"0.01","0.04","0.08","0.12(default)","0.16","0.20","0.24"};
		for(id=0;id<NUM(txt);id++){
			if( obj->view.pitchthr == (float)atof(txt[id]) ) break;
		}
		if( id == NUM(txt) ) id = -1;
		ret = radio_menu(disp, screen, NUM(txt), txt, &id);
		if( ret < 0 || id < 0 ) return;
		obj->view.pitchthr = atof(txt[id]);
	}
	else if( mode == 9 ){
		/* pitch Num */
		static char *txt[] = {"1","2","3","4","5"};
		for(id=0;id<NUM(txt);id++){
			if( obj->view.pitchnum == atoi(txt[id]) ) break;
		}
		if( id == NUM(txt) ) id = -1;
		ret = radio_menu(disp, screen, NUM(txt), txt, &id);
		if( ret < 0 || id < 0 ) return;
		obj->view.pitchnum = atoi(txt[id]);
	}
	else if( mode == 10 ){
		/* pitch Root */
		static char *txt[] = {"0(log)","1(pow)","2(amp)","3(default)","4"};
		for(id=0;id<NUM(txt);id++){
			if( obj->view.pitchroot == (float)atof(txt[id]) ) break;
		}
		if( id == NUM(txt) ) id = -1;
		ret = radio_menu(disp, screen, NUM(txt), txt, &id);
		if( ret < 0 || id < 0 ) return;
		obj->view.pitchroot = atof(txt[id]);
	}
	else if( mode == 11 ){
		/* spow */
		static char *txt[] = {"0.15","0.25","0.35(default)","0.45","0.55", "0.65"};
		for(id=0;id<NUM(txt);id++){
			if( obj->view.spow == (float)atof(txt[id]) ) break;
		}
		if( id == NUM(txt) ) id = -1;
		ret = radio_menu(disp, screen, NUM(txt), txt, &id);
		if( ret < 0 || id < 0 ) return;
		obj->view.spow = atof(txt[id]);
	}
	else if( mode == 12 ){
		/* sgain */
		static char *txt[] = {"200","500","1000","2000(default)","4000","8000","16000"};
		for(id=0;id<NUM(txt);id++)
			if( obj->view.sgain == (float)atof(txt[id]) ) break;
		if( id == NUM(txt) ) id = -1;
		ret = radio_menu(disp, screen, NUM(txt), txt, &id);
		if( ret < 0 || id < 0 ) return;
		obj->view.sgain = atof(txt[id]);
	}
	else if( mode == 13 ){
		/* grayscale */
		static char *txt[] = {"dither","grayscale","color"};
		id = obj->view.grayscale;
		ret = radio_menu(disp, screen, NUM(txt), txt, &id);
		if( ret < 0 || id < 0 ) return;
		obj->view.grayscale = id;
	}
	obj->view.redraw = 1;
	sas_redraw(obj);
}

void sas_mode_menu(SasProp *obj)
{
	static char *modetxt[] = {
	"WAVE        (m)",
	"FFT_WB      (m)",
	"FFT_NB      (P)",
	"FFT_NB ZOOM (p)",
	"MFCC        (M)",
	"[showPower] (o)",
	"[showPitch] (i)",
	"[MelScale]",
	"FFT option..."};
	static int   modeid = -1;
	char         modeflg[10];
	int          i, id;

	/* current mode to set */
	if     ( obj->view.mode == MODE_WAVE ) id = 0; /* WAVE */
	else if( obj->view.mode == MODE_MFCC ) id = 4; /* MFCC */
	else if( obj->view.narrow == 0 )       id = 1; /* MODE_SPECTRO WIDEBAND */
	else if( obj->view.fzoom == 1.0 )      id = 2; /* MODE_SPECTRO NARROWBAND */
	else                                   id = 3; /* MODE_SPECTRO ZOOM */

	/* flags */
	for( i=0; i<NUM(modetxt); i++ ) modeflg[i] = 0;
	modeflg[id] = 1;
	if( obj->view.power ) modeflg[5] = 1;
	if( obj->view.pitch ) modeflg[6] = 1;
	if( obj->view.fscale == FSC_MEL ) modeflg[7] = 1;

	id = toggle_menu(obj->win.disp, obj->win.screen,
	                 NUM(modetxt), modetxt, &modeid, modeflg);

	if( id >= 0 ){
		if( id == 8 )
		{
			sas_para_menu(obj);
			return;
		}
		else if( id == 7 ) obj->view.fscale = (obj->view.fscale==FSC_MEL)? FSC_LINEAR: FSC_MEL;
		else if( id == 6 ) obj->view.pitch = (obj->view.pitch)?0:1;
		else if( id == 5 ) obj->view.power = (obj->view.power)?0:1;
		else if( id == 4 )
			obj->view.mode = MODE_MFCC;
		else if( id >= 1 && id <= 3 ){
			obj->view.mode = (id==0)?MODE_WAVE:MODE_SPECTRO;
			obj->view.narrow = (id<2)?0:1;
			obj->view.fzoom = (id<3)?1.0:(float)obj->file.freq/2/obj->view.pitchmax;
		}
		else if( id == 0 ){
			obj->view.mode = MODE_WAVE;
		}
		obj->view.redraw = 1;
		sas_redraw(obj);
	}
}

/* 基本メニュー */
void sas_menu(SasProp *obj)
{
	extern void create_slave_window(SasProp *obj);
	extern void create_ana_window(int anamode);
	extern void sync_windows(SasProp *obj);
#define N00  13
	static char *txt00[N00] = {
		"property...    ",           /*  0 */
		"mode...        ",           /*  1 */
		"FFT option...  ",           /*  2 */
		"H_scale...  (s)",           /*  3 */
		"H_zoom...      ",           /*  4 */
		"V_gain...      ",           /*  5 */
		"redraw     (^L)",           /*  6 */
		"sasWindow   (n)",           /*  7 */
		"anaWindow   (0)",           /*  8 */
		"synchronize (y)",           /*  9 */
		"noSync      (l)",           /* 10 toggle */
		"playbackLoop   ",           /* 11 toggle */
		"save ...       ",           /* 12 */
	};
	static char  flg00[N00];
	static int   id00 = 0;
	int          id, id0;
	int          i, j, f;
	Display      *disp = obj->win.disp;
	int          screen = obj->win.screen;

	/* SET TOGGLE FLAGS */
	for( i=0; i<N00; i++ ) flg00[i] = 0;
	if( obj->view.nosync ) flg00[10] = 1;
	if( obj->view.daloop ) flg00[11] = 1;

	/* MAIN MENU */
	id0 = toggle_menu(disp, screen, N00, txt00, &id00, flg00);
	/*if( id0 >= 0 ) id00 = id0; *//* automatically in toggle_menu */

	if( id0 == 0 ){
		sas_property_menu(obj);
	}
	else if( id0 == 1 ){
		sas_mode_menu(obj);
	}
	else if( id0 == 2 ){
		sas_para_menu(obj);
	}
	else if( id0 == 3 ){
		/* 時間スケール */
		static char *hscaletxt[4] = {"SECOND","H:M:S","SAMPLE","FRAME"};
		id = obj->view.xscale - SCALE_TIME;
		radio_menu(disp, screen, 4, hscaletxt, &id);
		if( id >= 0 ){
			obj->view.xscale = SCALE_TIME + id;
			obj->view.redraw = 1;
			sas_redraw(obj);
		}
	}
	else if( id0 == 4 ){
		/* 時間ズーム */
		static char *hzoomtxt[8] =
			{"zoomUp   (z)","zoomDown (Z)",
			 "ALL      (a)","1min","30sec","5sec","1sec","100ms"};
		i = obj->view.nview;
		j = obj->file.size;
		f = obj->file.freq;
		id = -1;
		if     ( i == j ) id=2;
		else if( i == f*60 )  id=3;
		else if( i == f*30 )  id=4;
		else if( i == f*5  )  id=5;
		else if( i == f    )  id=6;
		else if( i == f/10 )  id=7;
		radio_menu(disp, screen, 8, hzoomtxt, &id);
		if( id >= 0 ) {
			switch(id){
			case 0: sas_zoom_up(obj); break;
			case 1: sas_zoom_down(obj); break;
			case 2: sas_set_view(obj,obj->view.sview,j); break;
			case 3: sas_set_view(obj,obj->view.sview,f*60); break;
			case 4: sas_set_view(obj,obj->view.sview,f*30); break;
			case 5: sas_set_view(obj,obj->view.sview,f*5); break;
			case 6: sas_set_view(obj,obj->view.sview,f); break;
			case 7: sas_set_view(obj,obj->view.sview,f/10); break;
			default: break;
			}
		}
	}
	else if( id0 == 5 ){
		/* 振幅ズーム */
		static char *vzoomtxt[15] =
			{"gainUp   (g)", "gainDown (G)",
			 "1x","2x","4x","8x","16x","32x","64x","128x","256x","512x","1024x","2048x","4096x"};
		float  range, center;
		range = obj->view.ymax - obj->view.ymin;
		center = ( obj->view.ymax + obj->view.ymin ) / 2;
		for(id=2; id<15; id++ )
			if( range == 65536 / atof(vzoomtxt[id]) ) break;
		if( id == 15 ) id = -1;
		radio_menu(disp, screen, 15, vzoomtxt, &id);
		if( id == 0 ) sas_gain_up(obj);
		else if( id == 1 ) sas_gain_down(obj);
		else if( id >= 2 ){
			range = 65536 / atof(vzoomtxt[id]);
			obj->view.ymax = center + range/2;
			obj->view.ymin = center - range/2;
			obj->view.redraw = 1;
			sas_move_cursor(obj, obj->view.mx, obj->view.my);
			sas_redraw(obj);
		}
	}
	else if( id0 == 6 ){
		/* REDRAW */
		sas_redraw(obj);
	}
	else if( id0 == 7 ){
		/* NEW WINDOW */
		create_slave_window(obj);
	}
	else if( id0 == 8 ){
		/* NEW WINDOW */
		create_ana_window(MODE_FFTSPECT);
	}
	else if( id0 == 9 ){
		/* SYNC OTHRES */
		sync_windows(obj);
	}
	else if( id0 == 10 ){
		/* NOSYNC */
		sas_print_sel(obj);
		obj->view.nosync = (obj->view.nosync)?0:1;
		sas_print_sel(obj);
	}
	else if( id0 == 11 ){
		obj->view.daloop = (obj->view.daloop)?0:1;
	}
	else if( id0 == 12 ){
		sas_save_area(obj);
	}
}

/* =================================================================
カーソルの形を変える
================================================================= */
void sas_change_cursor(SasProp *obj, int type)
{
	if( obj->win.csmode != type ) {
		XDefineCursor(obj->win.disp, obj->win.win, obj->win.csfont[type]);
		obj->win.csmode = type;
	}
}

/* =================================================================
最後の Motion イベントまで読みとばす
これがないと Motion イベントが溜って表示が鈍臭くなる
================================================================= */
void sas_skip_motions(XEvent *event)
{
		while(XEventsQueued(event->xmotion.display,QueuedAfterReading)>0){
			XEvent ahead;
			XPeekEvent(event->xmotion.display,&ahead);
			if( ahead.type == MotionNotify ){
			    if( ahead.xmotion.window == event->xmotion.window )
					XNextEvent(event->xmotion.display,event);
				else
					XNextEvent(event->xmotion.display,&ahead);
					/* 何故か別の窓でもmotionが出るのだ XF86では */
			}
			else if( ahead.type == NoExpose ||
			         ahead.type == MappingNotify ||
			         ahead.type >= 0x100 )
				XNextEvent(event->xmotion.display,&ahead);
				/* 読み飛ばす */
			else
				break;
		}
}

/* =================================================================
波形ウィンドウ上のイベントを処理するディスパッチャー
================================================================= */
void sas_dispatch(SasProp *obj, XEvent *event)
{
	KeySym keysym;
	XComposeStatus status;
	char str[128];
	int  tmp;
	int  tacksmpl;
	extern void create_slave_window(SasProp *obj);
	extern void close_window(SasProp *obj);
	extern void sync_windows(SasProp *obj);
	extern void sync_labels(SasProp *obj);
	extern void sel_start_windows(SasProp *obj);
	extern void sel_end_windows(SasProp *obj);
	extern void create_ana_window(int anamode);

	switch( event->type ){
#ifdef WM_CLOSE
	case ClientMessage:
		if(//event->xclient.message_type == WM_PROTOCOLS &&
		event->xclient.format == 32 &&
		event->xclient.data.l[0] == WM_DELETE_WINDOW) {
/*			printf("ClientMessage %ld\n",event->xclient.message_type);
			fflush(stdout);
*/			close_window(obj);
		}
		break;
#endif
	case Expose:
		if( event->xexpose.count != 0 ) break;
		sas_redraw(obj);
		break;
	case LeaveNotify: 
		/* ERASE CURSOR */
		if( obj->view.cson )
			sas_cross_hair(obj, obj);
		obj->view.cson = 0;
		/* メタキー押したままLeaveしたらKey Release出ないらしいので */
		/* 自分でリセットする */
		break;

	case EnterNotify: 
	case MotionNotify:
		sas_skip_motions(event); /* 最後の Motion まで読みとばす */

		if( obj->view.b1press )
			obj->view.b1mot = 1;

		tacksmpl = -1;

		if( obj->mlbl != 0 ){
			/* ラベル変更中 カーソル変える必要無い */
			sas_label_move(obj,event->xmotion.x);
			sync_labels(obj);
		}
		else if( sas_move_scale(obj,event->xmotion.x, event->xmotion.y) )
			/* スケール領域内 || スケール変更中の時に１を返す */
			sas_change_cursor(obj,CS_UD); /* 上下矢印 */
		else {
			int clbl, clblm;  /* clblm {1:start 2:end 3:end+start 4:label} */
			/* カーソル近傍ラベル番号を調べる */
			tmp = sas_xy_to_lbl(obj,event->xmotion.x,event->xmotion.y, &clbl, &clblm);
			if( tmp >= 0 ){
				if( clblm&3 ){
					/* 境界線 */
					sas_change_cursor(obj,CS_LR); /* 左右矢印 */
					/* カーソル位置を境界線にタックする */
					event->xmotion.x = sas_lbl_to_x(obj,clbl,clblm);
					if( clblm == 1 )
						tacksmpl = obj->lbl[clbl].start * obj->file.freq / 1000;
					else
						tacksmpl = obj->lbl[clbl].end * obj->file.freq / 1000;
				}
				else if( clblm==4 ){
					sas_change_cursor(obj,CS_TEXT); /* ラベル文字 */
				}
				else
					sas_change_cursor(obj,CS_ARROW); /* 一般 */
			}
			else
				sas_change_cursor(obj,CS_NORMAL);
		}

		/* ERASE CURSOR */
		if( obj->view.cson )
			sas_cross_hair(obj, obj);

		/* MOVE CURSOR */
		sas_move_cursor(obj, event->xmotion.x, event->xmotion.y);
		if( tacksmpl >= 0 ) obj->view.cssmpl = tacksmpl;

		/* PUT CURSOR */
		sas_cross_hair(obj, obj);
		obj->view.cson = 1;

		/* SELECT AREA */
		if( obj->view.b1sel ) {
			if( obj->view.mode == MODE_WAVE )
				sas_sel_end(obj, obj->view.cssmpl, -1, 0);
			else
				sas_sel_end(obj, obj->view.cssmpl, obj->view.csfreq, obj->view.fsel);
			sel_end_windows(obj); /* shift の判断はこの中でする */
		}

#if 0
		/* もしも選択エリアの上にいたら、位置を動かす事ができる */
		/* というのはまだだよ */
#endif

		break;
	case ButtonPress:
		if( event->xbutton.button == 1 ) {
			int  scl, clbl, clblm;
			obj->view.b1mot = 0;
			obj->view.b1press = 1;
			scl = sas_move_scale(obj,event->xmotion.x, event->xmotion.y);
			/* スケール変更中ならラベルは修正しない */
			if( !scl ){
				tmp = sas_xy_to_lbl(obj,event->xbutton.x,event->xbutton.y, &clbl, &clblm);
				/* tmp -1:ラベルエリア内 -2:エリア外 0〜nlbl:境界あり */
				/* clbl:選択中のラベル番号 */
				/* clblm:選択情報(1:始点 2:終点 3:終点+次始点 4:ラベル) */
				if( tmp >= 0 && clblm && clbl==obj->clbl && clblm==obj->clblm ){
					/* 選択中のものを再度掴んだら編集 */
					if( clblm&3 ){
						/* 選択中を掴むと編集モード */
						sas_label_grab(obj);/* 何もしてない */
						obj->mlbl = 1; /* 編集開始 */
					}
					else if( clblm == 4 && obj->std ){
						/* ラベルを２回掴んだら編集 */
						/* 手抜きでとりあえずコンソールから */
						char str[100], *p;
						fprintf(stderr,"label: ");
						if( (p=fgets(str,100,stdin)) ) {
							if( (p=strchr(str,'\n')) ) *p = 0;
							sas_label_edit(obj,str);
							sync_labels(obj);
						}
					}
				}
				else if( tmp < 0 || (clblm&3) ){
					/* 区間外区間内か区間外で特に境界やラベルじゃない */
					/* 選択中でなく、境界線を掴んだら、区間選択開始(ラベル範囲でなくても) */
					if( tmp < -1 && obj->clbl >= 0 ){
						/* 掴んでいて、エリア外なら ungrab */
						obj->clbl = obj->clblm = -1;
						obj->dlbl = obj->albl = -1;
						sas_redraw(obj);
					}
					if( obj->view.mode == MODE_WAVE )
						sas_sel_start(obj, obj->view.cssmpl, -1, 0);
					else
						sas_sel_start(obj, obj->view.cssmpl, obj->view.csfreq, global_ctrl);
					obj->view.b1sel = 1;
					sel_start_windows(obj); /* shift の判断はこの中でする */
				}
			}
		}
		else if( event->xbutton.button == 2 ) {
			sas_menu(obj);
			/* obj->view.b2press = 1; */
		}
		else if( event->xbutton.button == 3 ) {
			obj->view.b3press = 1;
		}
		break;

	case ButtonRelease:
		if( event->xbutton.button == 1 ) {
			int clbl, clblm;
			if( obj->mlbl != 0 ){ /* 始点変更中だった */
				sas_label_release(obj);
				obj->mlbl = 0; /* 編集終了 */
			}
			tmp = sas_xy_to_lbl(obj,event->xbutton.x,event->xbutton.y,&clbl,&clblm);
			/* tmp = -1:エリア内 -2:エリア外 0〜nlbl:境界あり */
			/* 離した位置で近いラベル選択する */
			/* しかしマウスポインターを動かした後は選択しないようにしている */
			if( tmp >= -1 && !obj->view.b1mot ){
				int old  = obj->clbl;
				int oldm = obj->clblm;
				/* 既に選択されていたら、前の選択を解除 */
				if( old >= 0 ){
					sas_draw_label(obj,old,old+1);
					obj->clbl = -1; /* リセット */
					obj->clblm = 0; /* リセット */
					sas_draw_label(obj,old,old+1);
				}
				/* 新しい選択を表示 */
				if( clbl >= 0 ){
					sas_draw_label(obj,tmp,tmp+1);
					obj->clbl  = clbl;  /* セット */
					obj->clblm = clblm; /* セット */
					sas_draw_label(obj,tmp,tmp+1);
					/* UNDO バッファ */
					if( old != clbl || oldm != clblm )
						sas_label_keep(obj);
					if( ! (clblm & 3) ){
						/* 境界線以外のラベル領域を選んでいたら */
						if( ! obj->view.b1mot ){
							/* ラベルを掴んだら区間選択 2001.8.28 */
							sas_sel_start(obj, obj->lbl[tmp].start * obj->file.freq/1000, 0, 0);
							sel_start_windows(obj);
							sas_sel_end(obj, obj->lbl[tmp].end * obj->file.freq/1000, -1, 0);
							sel_end_windows(obj);
						}
					}
				}
			}
			obj->view.b1sel = 0;
			obj->view.b1mot = 0;
			obj->view.b1press = 0;
			/* 押されていない状態でのカーソルの位置に合わせて */
			if( sas_move_scale(obj,event->xmotion.x, event->xmotion.y) )
				sas_change_cursor(obj,CS_UD);
			else if( tmp >= 0 ){
				if( clblm &3 )
					sas_change_cursor(obj,CS_LR);
				else if( clblm == 4 )
					sas_change_cursor(obj,CS_TEXT);
				else
					sas_change_cursor(obj,CS_ARROW);
			}
			else
				sas_change_cursor(obj,CS_NORMAL);
		}
		else if( event->xbutton.button == 2 ) {
			obj->view.b2press = 0;
		}
		else if( event->xbutton.button == 3 ) {
			obj->view.b3press = 0;
		}
		break;
	case KeyRelease:
		str[0] = '\0';
		XLookupString(&event->xkey,str,128,&keysym,&status);
		switch(keysym){
		case XK_Shift_L:
		case XK_Shift_R: global_shift = 0;  break;
		case XK_Control_L:
		case XK_Control_R: global_ctrl = 0;  break;
		case XK_Meta_L:
		case XK_Meta_R: global_meta = 0;  break;
		case XK_Alt_L:
		case XK_Alt_R: global_alt = 0;  break;
		default: break;
		}
		break;
	case KeyPress:
		str[0] = '\0';
		XLookupString(&event->xkey,str,128,&keysym,&status);
		//fprintf(stderr,"keysym %02lx\n",keysym);fflush(stderr);
		switch(keysym){
		case XK_Shift_L:
		case XK_Shift_R: global_shift = 1; break;
		case XK_Control_L:
		case XK_Control_R: global_ctrl = 1; break;
		case XK_Meta_L:
		case XK_Meta_R: global_meta = 1; break;
		case XK_Alt_L:
		case XK_Alt_R: global_alt = 1; break;
		case XK_Tab: sas_label_sel(obj,obj->clbl+1,4); break;
		case XK_ISO_Left_Tab: sas_label_sel(obj,obj->clbl-1,4); break;
		default: break;
		}
		switch(str[0]){
		/* analyze window */
		case '0': create_ana_window(MODE_FFTSPECT); break;

		/* コマンド */
		case '/':
			/* 分割 */
			if( obj->clbl < 0 ) break;
			if( obj->clblm & 3 ) /* 境界の時はどっちか不明なので無視 */
				break;
			sas_label_split(obj,obj->clbl);
			sync_labels(obj);
			obj->albl = -1; break;
		case '!':
			/* マージ */
			if( obj->clbl < 1 ) break;
			if( obj->clblm == 2 ) /* 境界の時はその前後をマージ */
				break;
			if( obj->clblm == 3 ) /* 境界の時はその前後をマージ */
				sas_label_merge(obj,obj->clbl+1);
			else  /* 普段は選ばれた区間の左とマージ */
				sas_label_merge(obj,obj->clbl);
			sync_labels(obj);
			obj->dlbl = -1; break;
		case '+':{
			float st, en;
			st = (float)obj->view.ssel/obj->file.freq*1000;
			en = (float)(obj->view.ssel+obj->view.nsel)/obj->file.freq*1000;
			sas_label_add(obj, st, en, st, en, "");
			sync_labels(obj);
			break;
		}
		case 'l':
			sas_print_sel(obj);
			obj->view.nosync = (obj->view.nosync)?0:1;
			sas_print_sel(obj);
			break;
		case 'y': sync_windows(obj); break;
		case 'r': obj->lblarc = obj->lblarc?0:1; sas_redraw(obj); break;
		case 'n': create_slave_window(obj); break;
		case 'M': sas_change_mode2(obj); break;
		case 'm': sas_change_mode(obj); break;
		case 's': sas_change_xscale(obj); break;
		case 'z': sas_zoom_up(obj); break;
		case 'Z': sas_zoom_down(obj); break;
		case 'f': sas_skip_right(obj); break;
		case 'b': sas_skip_left(obj); break;
		case 'F': sas_view_right(obj); break;
		case 'B': sas_view_left(obj); break;
		case 'a': sas_view_all(obj); break;
		case 'g': sas_gain_up(obj); break;
		case 'G': sas_gain_down(obj); break;
		case 'p': sas_fzoom_up(obj); break;
		case 'P': sas_fzoom_down(obj); break;
		case 'i': obj->view.pitch = 1 - obj->view.pitch;
			obj->view.redraw = 1;
			sas_redraw(obj); break;
		case 'o': obj->view.power = 1 - obj->view.power;
			obj->view.redraw = 1;
			sas_redraw(obj); break;

		case 'F'-'@': tmp=(int)obj->view.spp;if(tmp<1)tmp=1;
			sas_set_view(obj,obj->view.sview+tmp,obj->view.nview); break;
		case 'B'-'@': tmp=(int)obj->view.spp;if(tmp<1)tmp=1;
			sas_set_view(obj,obj->view.sview-tmp,obj->view.nview); break;
		case 'Q'-'@': exit(0); break;
		case 'A'-'@': sas_set_view(obj, 0, obj->view.nview);
			break;
		case 'C'-'@': sas_copy_string(obj); break;
		case 'E'-'@': sas_set_view(obj,
			obj->file.size-1-obj->view.nview,
			obj->view.nview); break;
		case 'L'-'@': sas_redraw(obj); break;
		case 'X'-'@': close_window(obj); break;
		case 'U'-'@': sas_label_undo(obj); sync_labels(obj); break;
		case 'H'-'@': /* BS */
		case     127: /* DEL */
			if(obj->clbl>=0 && obj->clblm==4)
			{sas_label_del(obj,obj->clbl); sync_labels(obj);}
			 break;
		default:
			break;
		}
		break;
	case MappingNotify: /*keyboad mapping changes*/
		XRefreshKeyboardMapping(&event->xmapping);
		break;
	default:
		break;
	}
	return;
}

