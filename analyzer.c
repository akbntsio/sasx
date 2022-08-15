/*
	音響分析窓の処理

	波形窓上のカーソル位置を分析する関数 ana_cur_make() は sas の基本
	思想に則り、カーソルが動くたびに波形ファイルを open,seek,read,close
	している。この通り実際にディスクから読んだらエライ事だが、OS が適当
	にキャッシュしてくれる事をアテにしている.
	open,close より分析の方に時間がかかるので、現状でOKとする

	LPC におけるパワーの扱い
	  1. LPC のグラフにおけるパワーはAdjust にすると残差パワーを用いる
	     自己相関(Cor)パワーを使うとFFTスペクトルとずれがでる。

	MFCC グラフの特徴
	  1. MFCC のグラフにおけるパワーはプリエンファシス後の波形パワーである。
	     これはパワーの大きな帯域に強く影響されるため、特に高域の強い入力
	     に対してパワーがかなり大きめにでる傾向がある。
	  2. MFCC 係数には lifter がかかっているため、大域的な傾きや形状に対し
	     て鈍感となる。グラフにする際、平均的なリフタサイズである 
	     (1+lifter/sqrt(2)) で正規化して、他のグラフのレベルに補正している。
	  3. fbank 計算において、FFT総和を総和する三角窓が高域ほど広い為、
	     総和されるパワーが大きくなり、高域の強いスペクトル形状となる。

	MFCC2 における MFCC との違い
	  1. power項 mfcc[0] が波形パワーではなく、Σ{fbank[n]}なので、
	     高さがFFTに近い
	  2. lifter = 0 なので、形状がFFTスペクトルに近い
	  3. fbank の総和される帯域の重みで正規化しているので、形状がより
	     FFTに近い

*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /* usleep() */
#include <math.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include "sas.h"  /* SasObj */
#include "wave.h" /* sas_draw_clip_y */
#include "plot.h" /* plot, gc */
#include "spectro.h" /* fft, hamming */
#include "menu.h"

#ifdef WM_CLOSE
//Atom   WM_PROTOCOLS, WM_DELETE_WINDOW;
extern Atom   WM_DELETE_WINDOW;
#endif

#define DEF_FMIN     50
#define MIN_LFMIN    1.0
#define MIN_FRANGE   log10((double)2)
#define MAX_FMAX     1000000
#define EPS          (1.0e-12)
#define NUM(array)	(sizeof(array)/sizeof(*array))

/* ライン毎に色をローテーションします */
#define NUMCOLOR    8
static char *colorname[NUMCOLOR] =
	{ "#802020","#208020","#202080", "#808080",
	  "#ff0000", "#00ff00", "#0000ff", "#202020"};

/* dashstr は 2byte だけ有効です(fg,bgの順にpixel数を指定).
   なぜか 4byte の文字列としています */
/*
#define NUMDASH     5
static char *dashstr[NUMDASH] =
	{ NULL, "\012\002\377\377", "\006\002\377\377",
	  "\003\001\377\377", "\001\001\377\377" };
*/

static const int anamodelist[] = {
	MODE_FFTSPECT, MODE_LPCSPECT, MODE_LPCMELSPECT,
	MODE_LPCCEPSPECT, MODE_LPCMELCEPSPECT, MODE_MFCCSPECT,
	MODE_MFCCSPECT2, 0
};
static const char *anamodestr[] = {
	"FFT", "LPC", "LPCMEL", "LPCCEP", "LMCC", "MFCC", "MFCC2"
};

/* グローバル変数 */
extern SasProp *g_obj[];
extern int      g_nobj;

/* 分析モードメニューの処理 */
int ana_mode_menu(SasProp *obj)
{
	static char *txt[] =
		{"FFT","LPC","LPCCEP","LPCMEL","LMCC","MFCC","MFCC2"};
	static int   id = 0; /* menu はMODALなので多重呼出はないはず */
	char         flag[NUM(txt)];
	int          i;
	for(i=0;i<NUM(txt);i++)
		flag[i] = (obj->view.mode & (1<<i))? 1:0;
	i = toggle_menu(obj->win.disp, obj->win.screen, NUM(txt), txt, &id, flag); 
	if( i >= 0 )
		id = i;
	obj->view.mode = MODE_ANALYZE;
	for(i=0;i<NUM(txt);i++)
		obj->view.mode |= flag[i]? (1<<i):0;
	return id;
}

/* 分析パラメータメニューの処理 */
int ana_para_menu(SasProp *obj)
{
	static char *txt00[] = {
		"f_Scale...   (s)", /* 0 */
		"win_type...  (h)", /* 1 */
		"win_size...     ", /* 2 */
		"win_skip...     ", /* 3 */
		"pre_emph...     ", /* 4 */
		"lpc_order...    ", /* 5 */
		"cep_order...    ", /* 6 */
		"fbank_num...    ", /* 7 */
		"mfcc_lifter...  ", /* 8 */
		"mfcc_tilt...    ", /* 9 */
		"power_mode...   ", /* 10 */
		"noSync       (l)", /* 11 */
		"min_fftsize...  ", /* 12 */
	};
	static int   id00 = -1; /* menu はMODALなので多重呼出はないはず */
	int          id, id0;

	id0 = simple_menu(obj->win.disp, obj->win.screen, NUM(txt00), txt00, id00); 
	if( id0 >= 0 ) id00 = id0;
	if( id0 == 0 ){
		/* FSCALE */
		static char *txt0[3] = {"linear","mel","log"};
		id = (obj->view.fscale==FSC_LINEAR)?0:
			((obj->view.fscale==FSC_MEL)?1:2);
		id = radio_menu(obj->win.disp, obj->win.screen, 3, txt0, &id);
		if( id >= 0 ){
			obj->view.fscale = (id==0)?FSC_LINEAR:
				((id==1)?FSC_MEL:FSC_LOG);
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 1 ){
		/* WINDOW TYPE */
		static char *txt1[2] = {"square","Hamming"};
		id = (obj->view.anawin==SQUARE)?0:1;
		id = radio_menu(obj->win.disp, obj->win.screen, 2, txt1, &id); 
		if( id >= 0 ){
			obj->view.anawin = (id==0)?SQUARE:HAMMING;
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 2 ){
		/* WINDOW SIZE */
		static char *txt2[] =
		 {"5ms","10ms","15ms","20ms","30ms","40ms","50ms","100ms","200ms","400ms","1000ms","2000ms","4000ms","10000ms","single (w)"};
		int          num2 = NUM(txt2);
		for(id=0;id<num2-1;id++)
			if(obj->view.framesize == atof(txt2[id])) break;
		if( id == num2-1 ) id = -1;
		if( obj->view.single ) id = num2-1;
		id = radio_menu(obj->win.disp, obj->win.screen, num2, txt2, &id); 
		if( id >= 0 ){
			if( id == num2-1 )
				obj->view.single = 1;
			else {
				obj->view.single = 0;
				obj->view.framesize = atof(txt2[id]);
			}
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 3 ){
		/* WINDOW SKIP */
		static char *txt3[] =
		 {"2.5ms","5ms","7.5ms","10ms","15ms","20ms","25ms","50ms","100ms","500ms","1000ms","2000ms","5000ms"};
		int num3 = NUM(txt3);
		for(id=0;id<num3;id++)
			if(obj->view.frameskip == atof(txt3[id])) break;
		if( id == num3 ) id = -1;
		id = radio_menu(obj->win.disp, obj->win.screen, num3, txt3, &id); 
		if( id >= 0 ){
			obj->view.frameskip = atof(txt3[id]);
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 4 ){
		/* PREEMPHASIS */
		static char *txt4[8] =
		 {"0.00 (p)","0.90","0.95","0.96","0.97","0.98(default)","0.99","1.00"};
		for(id=0;id<8;id++){
			if(obj->view.anapre == (float)atof(txt4[id])) break;
		}
		if( id == 8 ) id = -1;
		id = radio_menu(obj->win.disp, obj->win.screen, 8, txt4, &id); 
		if( id >= 0 ){
			obj->view.anapre = atof(txt4[id]);
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 5 ){
		/* LPC ORDER */
		static char *txt5[8] = {"8","10","12","14","16(default)","18","20","40"};
		for(id=0;id<8;id++)
			if(obj->view.analpc == atoi(txt5[id])) break;
		if( id == 8 ) id = -1;
		id = radio_menu(obj->win.disp, obj->win.screen, 8, txt5, &id); 
		if( id >= 0 ){
			obj->view.analpc = atoi(txt5[id]);
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 6 ){
		/* CEP ORDER */
		static char *txt6[8] = {"8","10","12(default)","14","16","18","20","24"};
		for(id=0;id<8;id++)
			if(obj->view.anacep == atoi(txt6[id])) break;
		if( id == 8 ) id = -1;
		id = radio_menu(obj->win.disp, obj->win.screen, 8, txt6, &id); 
		if( id >= 0 ){
			obj->view.anacep = atoi(txt6[id]);
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 7 ){
		/* FILTERBANK NUMBER */
		static char *txt7[9] = {"12","16","20","24(default)","28","32","36","40","48"};
		for(id=0;id<9;id++)
			if(obj->view.anafbnum == atoi(txt7[id])) break;
		if( id == 9 ) id = -1;
		id = radio_menu(obj->win.disp, obj->win.screen, 9, txt7, &id); 
		if( id >= 0 ){
			obj->view.anafbnum = atoi(txt7[id]);
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 8 ){
		/* MFCC LIFTER */
		static char *txt8[8] = {"0","12","14","16","18","20","22(default)","24"};
		for(id=0;id<8;id++)
			if(obj->view.analifter == atof(txt8[id])) break;
		if( id == 8 ) id = -1;
		id = radio_menu(obj->win.disp, obj->win.screen, 8, txt8, &id); 
		if( id >= 0 ){
			obj->view.analifter = atof(txt8[id]);
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 9 ){
		/* MFCC TILT */
		static char *txt9[2] = {"as is","normalize"};
		id = obj->view.mfcctilt;
		id = radio_menu(obj->win.disp, obj->win.screen, 2, txt9, &id); 
		if( id >= 0 ){
			obj->view.mfcctilt = id;
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 10 ){
		/* POWER MODE */
		static char *txt10[2] = {"autocor","adjusted"};
		id = obj->view.powmode;
		id = radio_menu(obj->win.disp, obj->win.screen, 2, txt10, &id); 
		if( id >= 0 ){
			obj->view.powmode = id;
			obj->view.redraw = 1;
		}
	}
	else if( id0 == 11 ){
		/* TOGGLE SYNC */
		obj->view.nosync = (obj->view.nosync)?0:1;
		if( obj->view.nosync )
			ana_draw_condition(obj);
		else 
			obj->view.redraw = 1;
	}
	else if( id0 == 12 ){
		/* FFT SIZE */
		static char *txt[] = {"64","128","256","512","1024","2048","4096"};
		for(id=0;id<NUM(txt);id++)
			if(obj->view.fftmin == atoi(txt[id])) break;
		if( id == NUM(txt) ) id = -1;
		id = radio_menu(obj->win.disp, obj->win.screen, NUM(txt), txt, &id); 
		if( id >= 0 ){
			obj->view.fftmin = atoi(txt[id]);
			obj->view.redraw = 1;
		}
	}
	return  id0;
}

/* ウィンドウにだけ描画する。WAVEの場合はpixにも同時に描画していた */
void ana_draw_clip_y_win(SasProp *obj, GC gc, int x1, int y1, int x2, int y2,
	int ymin, int ymax)
{
	int   xx1, xx2, yy1, yy2;
	if( !sas_clip_y( x1, y1, x2, y2, ymin, ymax, &xx1, &yy1, &xx2, &yy2) )
		return;
	XDrawLine(obj->win.disp,obj->win.win,gc, xx1, yy1, xx2, yy2);
}

void ana_draw_clip_y_pix(SasProp *obj, GC gc, int x1, int y1, int x2, int y2,
	int ymin, int ymax)
{
	int   xx1, xx2, yy1, yy2;
	if( !sas_clip_y( x1, y1, x2, y2, ymin, ymax, &xx1, &yy1, &xx2, &yy2) )
		return;
	XDrawLine(obj->win.disp,obj->win.pix,gc, xx1, yy1, xx2, yy2);
}

/* 分析用の窓を作成 */
void *ana_create( Display *disp, int anamode )
{
	SasProp *obj;
	obj = sas_create( disp );

	/* ana 用に設定しなおし */
	obj->view.mode = anamode;
	strcpy(obj->win.name, "Analyze : sasx");
	obj->win.size.flags = USSize;
	obj->win.size.x = 100;
	obj->win.size.y = 100;
	obj->win.size.width = 512;
	obj->win.size.height = 300;

	obj->view.fmin = 0;
	obj->view.fmax = -1;  /* 窓作成する時に調べる */
	obj->view.lfmin = DEF_FMIN;
	obj->view.lfmax = -1;
	obj->view.lmin = log10((double)obj->view.lfmin);
	obj->view.lmax = -1;
	obj->view.mmin = 0;
	obj->view.mmax = -1;
	obj->view.dbmin = -10;
	obj->view.dbmax = 100;

	obj->view.coordx = 0;
	obj->view.coordy = 0;
	obj->view.coordpow = 0;
	obj->view.curch = 0;
	obj->view.coordsize = 0;
	obj->view.anach = 0;
	obj->view.anasize[0] = 0;
	obj->view.anaon = 0;
	obj->view.anapre = 0.98;
	obj->view.anapref = 1;
	obj->view.analagbw = 0.01;
	obj->view.analpc = 16;
	obj->view.anacep = 12;
	obj->view.anafbnum = 24;
	obj->view.analifter = 22;
	obj->view.single = 0;
	obj->view.fftmin = 512;

	obj->view.dragmode = DR_NONE;
	obj->view.fscale = FSC_LINEAR; /* LINEAR,LOG,MEL */
	obj->view.anawarp = 0.38;  /* 0.37:12k */
	obj->view.anawin = HAMMING; /* HAMMING, SQURARE */
	obj->view.anapow = 1; /* display power */
	obj->view.powmode = 0; /* 0:cor[0]  1:調整パワー */
	obj->view.mfcctilt = 0; /* MFCC傾き補正 0:なし 1:あり */
#ifdef ANA_THREAD
	obj->view.tstate = 0;
	obj->view.tquit  = 0;
#endif

	return obj;
}

void ana_realize( SasProp *obj )
{
	sprintf(obj->win.name, "Analyze : sasx[%d]",obj->win.id);
	sas_realize( obj );
	sas_check_resize(obj); /* pix を確保するため */
	return;
}

void ana_destroy( SasProp *obj )
{
	if( obj->view.coordsize && obj->view.coordx )
		free( obj->view.coordx );
	if( obj->view.coordsize && obj->view.coordy )
		free( obj->view.coordy );
	sas_destroy( obj );
}


/* リサイズを検知 */
int ana_check_resize(SasProp *obj)
{
	return sas_check_resize(obj);
}

/* 画面クリア */
void ana_clear_win(SasProp *obj)
{
	int w, h;
	h = obj->win.size.height;
	w = obj->win.size.width;
	XFillRectangle(obj->win.disp, obj->win.pix, obj->win.bggc, 0, 0, w, h);
}

/* 最大のサンプリング周波数/2 を求める */
float ana_get_fmax(SasProp *obj)
{
	int  i;
	float fmax;
	fmax = 0;
	for( i=0; i<g_nobj; i++ ){
		if( g_obj[i] && g_obj[i]->file.type != 'P'
				&& !(g_obj[i]->view.mode & MODE_ANALYZE)
				&& fmax < g_obj[i]->file.freq )
			fmax = g_obj[i]->file.freq;
	}
	if( fmax == 0 )
		fmax = 12000;
	return fmax / 2;
}

/* Mel周波数変換 freq(Hz) -> freq(Mel) */
float Mel(float freq)
{
	return 1127.0 * log(1.0+freq/700.0);
}

float MelInv(float mf)
{
	return (exp( mf / 1127.0 ) - 1.0) * 700;
}

/* 0--PI を 0--PI にマップする */
/* warp を -warp にすると逆変換になるよ */
float MelWarp(float warp, float w)
{
	return (w - 2 * atan2(warp*sin(w), 1.0+warp*cos(w)));
}

/* 対数スケール用の lmin, lmax を計算 */
void  init_fmax(SasProp *obj)
{
		obj->view.fmax = ana_get_fmax(obj);
		obj->view.lfmax = obj->view.fmax;
		if( obj->view.lfmax < obj->view.lfmin + MIN_FRANGE )
			obj->view.lfmax = obj->view.lfmin + MIN_FRANGE;
		obj->view.lmax = log10((double)obj->view.lfmax);
		obj->view.mmax = Mel((float)obj->view.fmax);
}

/* 周波数から x 座標へ */
int  ana_f2x(SasProp *obj, int wd, float f)
{
	int x;
	if( obj->view.fscale == FSC_LOG ){
		if( f < MIN_LFMIN ) f = (float)MIN_LFMIN;
		x = wd * (log10((double)f) - obj->view.lmin)
			/ (obj->view.lmax - obj->view.lmin);
	}
	else if( obj->view.fscale == FSC_MEL ){
		x = wd * (Mel((float)f) - obj->view.mmin)
			/ (obj->view.mmax - obj->view.mmin);
	}
	else { /* LINEAR */
		x = wd * (f - obj->view.fmin)
			/ (obj->view.fmax - obj->view.fmin);
	}
	return x;
}

/* スケールを描画 */
void ana_draw_scale(SasProp *obj)
{
	int    x, y, wd, ht, w;
	int    i, l;
	char   str[32];
	int    tic, tic10, ticlb, i10, idb, imax, imin;
	double mul, db;

	/* 枠 */
	wd = obj->win.size.width - obj->win.scalew;
	ht = obj->win.size.height - obj->win.scaleh*2;
	XDrawRectangle(obj->win.disp, obj->win.pix, obj->win.scgc,
		obj->win.scalew, obj->win.scaleh, wd-1, ht);


	/* 縦軸 */
	//sas_calc_tic(obj->view.dbmax - obj->view.dbmin, ht, 8, 3, &tic, &tic10, &ticlb);
	sas_calc_tic_real(obj->view.dbmax - obj->view.dbmin, ht, 3, &tic, &i10);
	tic10 = 10;
	ticlb = tic * 10;
	mul = pow(10.0,(double)i10);
	imax = obj->view.dbmax / mul;
	imin = obj->view.dbmin / mul;
	x = obj->win.scalew;
	idb = (int)(imin/tic) * tic;
	if( idb < imin ) idb += tic;
	for( ; idb <= imax; idb += tic ){
		db = idb * mul;
		y = obj->win.size.height - obj->win.scaleh
			- ht * (db - obj->view.dbmin) / (obj->view.dbmax - obj->view.dbmin);

		if( (int)idb%tic10 == 0 ) l=6;
		else if( (int)idb%(tic10/2) == 0 ) l=4;
		else l=3;
		XDrawLine(obj->win.disp, obj->win.pix, obj->win.scgc, x, y, x+l, y);

		if( (int)idb%ticlb == 0 ){
			char fmt[32];
			sprintf(fmt, "%%.%df",(i10<-1)?(-i10-1):0);
			sprintf(str,fmt,db);
			w = XTextWidth(obj->win.font_struct, str, strlen(str));
			XDrawString(obj->win.disp, obj->win.pix, obj->win.scgc,
				obj->win.scalew-w-2, y+obj->win.fonta/2, str, strlen(str));
			XDrawLine(obj->win.disp, obj->win.pix, obj->win.grgc, x, y, x+wd, y);
		}
	}

	/* 初めての時は最大のサンプリング周波数から表示の最大周波数を計算 */
	if( obj->view.fmax < 0 )
		init_fmax(obj); /* 最初にスケールを描くのでここでやっておく */

	/* 横軸 対数 */
	if( obj->view.fscale == FSC_LOG ){
		int    j;
		float  fmin, fmax, lmin, lmax, ltic;
		fmin = obj->view.lfmin;
		fmax = obj->view.lfmax;
		lmin = obj->view.lmin;
		lmax = obj->view.lmax;
		y = obj->win.size.height - obj->win.scaleh;
		for( i=1; i<=fmax; i*=10 ){
			for( j=1; j<10; j++ ){
				if( i*j < fmin || i*j > fmax ) continue;
				ltic = log10((double)i*j);
				x = obj->win.scalew + wd * (ltic-lmin) / (lmax-lmin);
				if( x < obj->win.scalew ) continue;
				if( j==1 || j==5 || j==2 ){
					if( i>= 1000 )
						sprintf(str,"%dk", i*j/1000);
					else
						sprintf(str,"%d", i*j);
					w = XTextWidth(obj->win.font_struct, str, strlen(str));
					XDrawString(obj->win.disp, obj->win.pix, obj->win.scgc,
						x-w/2, obj->win.size.height-1, str, strlen(str));
					XDrawLine(obj->win.disp, obj->win.pix, obj->win.grgc, x, y, x, y-ht);
				}

				if( j==1 || j==5 ) l=5;
				else            l=3;
				XDrawLine(obj->win.disp, obj->win.pix, obj->win.scgc, x, y, x, y-l);
			}
		}
	}
	else { /* 横軸 リニア,MEL */
		sas_calc_tic((float)obj->view.fmax - obj->view.fmin, wd, 8, 3, &tic, &tic10, &ticlb);
		if( ticlb > tic*10 ) ticlb = tic*10;
		y = obj->win.size.height - obj->win.scaleh;
		i = (int)(obj->view.fmin/tic) * tic;
		if( i < obj->view.fmin ) i += tic;
		for( ; i<=obj->view.fmax; i+=tic ){
			if( obj->view.fscale == FSC_MEL ){
				x = obj->win.scalew + wd *
				(Mel((float)i) - obj->view.mmin) / (obj->view.mmax - obj->view.mmin);
			}
			else{ /* FSC_LINEAR */
				x = obj->win.scalew +
				wd * (i-obj->view.fmin) / (obj->view.fmax-obj->view.fmin);
			}


			if( i%ticlb == 0 ){
				if( i>=1000 ) sprintf(str,"%gk", (float)i/1000);
				else sprintf(str,"%d", i);
				w = XTextWidth(obj->win.font_struct, str, strlen(str));
				XDrawString(obj->win.disp, obj->win.pix, obj->win.scgc,
					x-w/2, obj->win.size.height-1, str, strlen(str));
				XDrawLine(obj->win.disp, obj->win.pix, obj->win.grgc, x, y, x, y-ht);
			}

			if( i%tic10 == 0 ) l=5;
			else if( i%(tic10/2) == 0 ) l=3;
			else l=2;
			XDrawLine(obj->win.disp, obj->win.pix, obj->win.scgc, x, y, x, y-l);
		}
	}
}

/* FFTスペクトルを１個表示 */
void ana_draw_fftspect(SasProp *obj, GC gc, SasProp *src)
{
	int        start, len, nchan, fftsize, ffti;
	FILE       *fp=0;
	double     *buf=0;
	double     *xr=0, *xi=0, *av=0;
	int        size, bsize, skip, rept, swap;	// skip は 1/1000 sample
	int        ch, i, j, r, imin, imax;
	double     pow, avepow;

	swap  = (src->file.endian == obj->view.cpuendian)? 0: 1;
	nchan = src->file.chan;
	start = src->view.ssel;
	len   = src->view.nsel;
	if(len<0) {len = -len; start -= len;}
	if( obj->view.single ) /* 一括FFT */
		bsize = len;
	else
		bsize = (double)obj->view.framesize * src->file.freq / 1000; /* 繰り返して平均 */
	skip  = (double)obj->view.frameskip * src->file.freq;

	if( len < bsize && !obj->view.single ){
		len = bsize; rept = 1;
	}
	else rept = 1 + (len-bsize)*1000/skip;


	for( ffti=2, fftsize=4; fftsize<obj->view.fftmin; ffti++, fftsize*=2 );
	for(                  ; fftsize<bsize; ffti++, fftsize*=2 );
	/* for( ffti=5, fftsize=32; fftsize<bsize; ffti++, fftsize*=2 ) ; */

	if( !(buf=malloc(sizeof(double)*len*nchan)) ) ;
	else if( !(xr=malloc(sizeof(double)*fftsize*2)) ) ;
	else if( !(xi=malloc(sizeof(double)*fftsize*2)) ) ;
	else if( !(av=malloc(sizeof(double)*(fftsize/2+1))) ) ;
	else if( !(fp=fopen(src->file.name,"r")) ) ;
	else if( (size=read_double(fp, src->file.hsize, src->file.offset, src->file.type,nchan,swap, start,len,buf)) <= 0 ) ;
	else {
		int   wd, ht, by, x, y, ox=0, oy=0;
		char  str[1024];
		float fmin, fmax;

		wd = obj->win.size.width - obj->win.scalew;
		ht = obj->win.size.height - obj->win.scaleh*2;
		by = obj->win.scaleh + ht;

		if( obj->view.fscale == FSC_LOG ){
			fmin = obj->view.lfmin;
			fmax = obj->view.lfmax;
		}
		else {
			fmin = obj->view.fmin;
			fmax = obj->view.fmax;
		}
		imin = (double)fftsize * fmin / src->file.freq;
		imax = (double)fftsize * fmax / src->file.freq + 1;
		if( imin < 0 ) imin = 0;
		if( imax > fftsize/2 ) imax = fftsize/2;

		for( ch=0; ch<nchan; ch++, obj->view.nline++ ){

#ifdef ANA_THREAD
				if( obj->view.tquit ) break;
#endif

			sas_change_gc(obj, gc, colorname[obj->view.nline%NUMCOLOR], 0);

#if 0
			/* 凡例を描く */
			sprintf(str,"%s [%d.%d]%-6s %4.0f~%4.0fms(%dx%d)",
				src->file.name, src->win.id, ch, "FFT",
				start*1000.0/src->file.freq, (start+len)*1000.0/src->file.freq,
				bsize, rept);
			y = (obj->view.nline+2) * obj->win.scaleh;
			x = obj->win.size.width - XTextWidth(obj->win.font_struct,str,strlen(str)) - 1;
			XDrawString(obj->win.disp, obj->win.pix, gc, x, y, str, strlen(str));
#endif

			/* グラフを描く */
			memset( av, 0, sizeof(double)*(fftsize/2+1) );
			avepow = 0;
			for( r=0; r<rept; r++ ){
#ifdef ANA_THREAD
				if( obj->view.tquit ) break;
#endif
				for( i=0, j=r*skip/1000; i<bsize && j<size; i++, j++ ){
					xr[i] = buf[j*nchan+ch];
					xi[i] = 0;
				}
				for( j=i; j<fftsize; j++ )
					xr[j] = xi[j] = 0;
				if( obj->view.anapref )
					pre_emphasis(obj->view.anapre, xr, i);
				if( obj->view.anawin == HAMMING )
					hamming(xr, bsize); /* 注：サンプルが不足したら窓短くなる */
				if( obj->view.anapow ){
					/* fft の前にパワー計算 */
					for( pow=0,i=0; i<bsize; i++ )
						pow += xr[i] * xr[i];
					pow /= bsize;
					if( pow < EPS ) pow = EPS;
					pow = 10.0 * log10(pow);
					avepow += pow;
				}
				fft( xr, xi, ffti, fftsize );
				for( i=0; i<=fftsize/2; i++ ){
					xr[i] = xr[i]*xr[i] + xi[i]*xi[i];
					xr[i] /= bsize;
					if( xr[i] < EPS ) xr[i] = EPS;
					xr[i] = 10.0 * log10(xr[i]);
					av[i] += xr[i];
				}
			}
			if( obj->view.anapow && r>0 ){
				avepow /= r;
			}

#ifdef ANA_THREAD
				if( obj->view.tquit ) break;
#endif

			for( i=imin; i <= imax; i++ ){
				float f;
				av[i] /= rept;
				f = src->file.freq * (double)i / (double)fftsize;
				x = obj->win.scalew + ana_f2x(obj, wd, f);
				y = by - ht * (av[i] - obj->view.dbmin)
					/ (obj->view.dbmax - obj->view.dbmin);
				if( i > imin )
					ana_draw_clip_y_pix(obj, gc, ox,oy, x,y, obj->win.scaleh, by );
				ox = x; oy = y;
			}
			if( obj->view.anapow ){
				/* パワーを表示 */
				x = obj->win.scalew;
				y = by - ht * (avepow - obj->view.dbmin)
					/ (obj->view.dbmax - obj->view.dbmin);
				ana_draw_clip_y_pix(obj, gc, x-20,y, x,y, obj->win.scaleh, by);
				ana_draw_clip_y_pix(obj, gc, x-10,y-2, x,y, obj->win.scaleh, by);
/*
				ana_draw_clip_y_pix(obj, gc, x-10,y+2, x,y, obj->win.scaleh, by);
*/

				/* 凡例 */
				sprintf(str,"%6.1fdB %s [%d.%d]%-6s %4.0f~%4.0fms(%dx%d)",
					avepow, src->file.name, src->win.id, ch, "FFT",
					start*1000.0/src->file.freq, (start+len)*1000.0/src->file.freq,
					bsize, rept);
			}
			else {
				/* 凡例 */
				sprintf(str,"%s [%d.%d]%-6s %4.0f~%4.0fms(%dx%d)",
					src->file.name, src->win.id, ch, "FFT",
					start*1000.0/src->file.freq, (start+len)*1000.0/src->file.freq,
					bsize, rept);
			}

			/* 凡例を描く */
			y = (obj->view.nline+2) * obj->win.scaleh;
			x = obj->win.size.width - XTextWidth(obj->win.font_struct,str,strlen(str)) - 1;
			XDrawString(obj->win.disp, obj->win.pix, gc, x, y, str, strlen(str));

		}/* ch */
	}/* draw or not */

	if( fp ) fclose(fp);
	if( av ) free(av);
	if( xi ) free(xi);
	if( xr ) free(xr);
	if( buf ) free(buf);
}


/* テスト用 MFCCスペクトルを１個表示 */
void ana_draw_mfccspect_sub(SasProp *obj, GC gc, SasProp *src, char *tagstr,
	int lifter, int tilt, int powmode)
{
	int        start, len, nchan, fftsize, ffti, order, xrsize;
	FILE       *fp=0;
	double     *buf=0;
	float      *mfbpf=0;
	float      *mfcc=0;
	double     *xr=0, *xi=0, *av=0;
	int        size, bsize, skip, rept, swap;	// skip は 1/1000 sample
	int        ch, i, j, r, imin, imax;
	int        fbnum;
	float      melmax;
	extern void Wav2MFCC_E_D(short *wave, float *mfcc, long sampsize,
		long sfreq, int fbank_num, int raw_e, float preEmph,
		int mfcc_dim, int lifter, float *mfbpf);
	extern int fbankpow;
	extern int fbanknormalize;

	swap  = (src->file.endian == obj->view.cpuendian)? 0: 1;
	nchan = src->file.chan;
	start = src->view.ssel;
	len   = src->view.nsel;
	if(len<0) {len = -len; start -= len;}
	if( obj->view.single ) /* 一括LPC */
		bsize = len;
	else
		bsize = (double)obj->view.framesize * src->file.freq / 1000; /* 繰り返して平均 */
	skip  = (double)obj->view.frameskip * src->file.freq;

	if( len < bsize && !obj->view.single ){
		len = bsize; rept = 1;
	}
	else rept = 1 + (len-bsize)*1000/skip;

	fbnum = obj->view.anafbnum;
	/*lifter = obj->view.analifter;*/
	/* for( ffti=2, fftsize=4; fftsize<bsize; ffti++, fftsize*=2 ) ; */
	for( ffti=2, fftsize=4; fftsize<obj->view.fftmin; ffti++, fftsize*=2 );
	for(                  ; fftsize<bsize; ffti++, fftsize*=2 );

	/* MFCC での帯域まとめによる増加を平均的に押える。あくまで表示を揃える為 */
	/* さらに、分析窓サイズでの正規化処理をやってないのでここでやる */

	/* CEP->スペクトルへの変換用FFTサイズは固定 */
	ffti = 9;
	fftsize = 512;
	order = obj->view.anacep;
	xrsize = (fftsize*2>bsize)?fftsize*2:bsize;

	if( !(buf=malloc(sizeof(double)*len*nchan)) ) ;
	else if( !(mfcc=malloc(sizeof(float)*(order+1))) ) ;
	else if( !(mfbpf=malloc(sizeof(float)*(fbnum+1))) ) ;
	else if( !(xr=malloc(sizeof(double)*xrsize)) ) ;
	else if( !(xi=malloc(sizeof(double)*xrsize)) ) ;
	else if( !(av=malloc(sizeof(double)*(fftsize/2+1))) ) ;
	else if( !(fp=fopen(src->file.name,"r")) ) ;
	else if( (size=read_double(fp, src->file.hsize, src->file.offset, src->file.type,nchan,swap, start,len,buf)) <= 0 ) ;
	else {
		int   wd, ht, by, x, y, ox=0, oy=0;
		char  str[1024], modestr[16];
		int   fmin, fmax;
		float scale, scale2, shift;
		short *wave = (short *)xr;  /* 一時的に借りる */

		wd = obj->win.size.width - obj->win.scalew;
		ht = obj->win.size.height - obj->win.scaleh*2;
		by = obj->win.scaleh + ht;

		if( obj->view.fscale == FSC_LOG ){
			fmin = obj->view.lfmin;
			fmax = obj->view.lfmax;
		}
		else {
			fmin = obj->view.fmin;
			fmax = obj->view.fmax;
		}
		melmax = Mel((float)src->file.freq/2);
		imin = fftsize/2 * Mel((float)fmin) / melmax;
		imax = fftsize/2 * Mel((float)fmax) / melmax + 1;
		if( imin < 0 ) imin = 0;
		if( imax > fftsize/2 ) imax = fftsize/2;

		/* fbankpow の時 */
		/* norm of Fbank[i] = log(sqrt(P[i])) */
		/* 10log10(P) =                                               */
		/* 20/log(10)/fbN/Lft *(log(sqrt(P*L))*fbN*Lft - log(sqrt(L))*fbN*Lft) */
		/* ^^^^^scale^^^^^^^^   +++++++++mfcc+++++++++   ^^^^^^^^shift^^^^^^^ */
		/* 自己相関の時 */
		/* 10log10(P) = 10/log(10) * ((log(P*L)) - log(L)) */
		/* パワー項にはリフタは関係ない */
		if( powmode == 0 ) {
			scale = 10.0/log(10.0);
			shift = log((double)bsize);
		}
		else {
			scale = 20.0/log(10.0)/sqrt((double)fbnum);
			shift = log((double)bsize)/2*sqrt((double)fbnum);
		}
		scale2 = 20.0/log(10.0)/sqrt((double)fbnum)/(1+lifter/sqrt(2.0));
					/* log->20log10,  (/fbnum)DCTによるgainをもどす */
					/* (/1+lifter)lifterによるgainを戻す  (*5)見ためのため */

		for( ch=0; ch<nchan; ch++, obj->view.nline++ ){

#ifdef ANA_THREAD
			if( obj->view.tquit ) break;
#endif

			sas_change_gc(obj, gc, colorname[obj->view.nline%NUMCOLOR], 0);

			/* 凡例を描く */
			strcpy(modestr,tagstr);
			sprintf(str,"%s [%d.%d]%-6s %4.0f~%4.0fms(%dx%d)",
				src->file.name, src->win.id, ch, modestr,
				start*1000.0/src->file.freq, (start+len)*1000.0/src->file.freq,
				bsize, rept);
			y = (obj->view.nline+2) * obj->win.scaleh;
			x = obj->win.size.width - XTextWidth(obj->win.font_struct,str,strlen(str)) - 1;
			XDrawString(obj->win.disp, obj->win.pix, gc, x, y, str, strlen(str));

			/* グラフを描く */
			memset( av, 0, sizeof(double)*(fftsize/2+1) );
			for( r=0; r<rept; r++ ){
#ifdef ANA_THREAD
				if( obj->view.tquit ) break;
#endif
				for( i=0, j=r*skip/1000; i<bsize && j<size; i++, j++ )
					wave[i] = buf[j*nchan+ch];
				for( ; i<bsize; i++ )
					wave[i] = 0;
				fbankpow = (powmode==1)?1:0;      /* 1:パワー項は Σ{fbank} */
				fbanknormalize = (tilt)?1:0;  /* Fbank窓のサイズをキャンセル */
				Wav2MFCC_E_D(wave, mfcc, bsize, src->file.freq, fbnum, 0,
					obj->view.anapre*obj->view.anapref, order, lifter, mfbpf);
				fbankpow = 0;
				fbanknormalize = 0;

				memset( xi, 0, sizeof(double)*fftsize ); 
				memset( xr, 0, sizeof(double)*fftsize ); 

				/* FFTCEPのレベルに補正 */
				for( i=0; i<=order; i++ )
					xr[i] = xr[fftsize-i] = scale2 * mfcc[i];
				xr[0] = scale * (mfcc[0] - shift);

				fft( xr, xi, ffti, fftsize );
				for( i=0; i<=fftsize/2; i++ ){
					av[i] += xr[i];
				}
			}

#ifdef ANA_THREAD
			if( obj->view.tquit ) break;
#endif

			for( i=imin; i<=imax; i++ ){
				float  f;
				av[i] /= rept;
				f = MelInv(melmax * i / (fftsize / 2));   /* mel->lin */
				x = obj->win.scalew + ana_f2x(obj, wd, f); /* lin->graph */
				y = by - ht * (av[i] - obj->view.dbmin)
					/ (obj->view.dbmax - obj->view.dbmin);
				if( i>imin )
					ana_draw_clip_y_pix(obj, gc, ox,oy, x,y, obj->win.scaleh, by );
				ox = x; oy = y;
			}
		}
	}

	if( fp ) fclose(fp);
	if( av ) free(av);
	if( xi ) free(xi);
	if( xr ) free(xr);
	if( mfbpf ) free(mfbpf);
	if( mfcc ) free(mfcc);
	if( buf ) free(buf);
}

/* MFCCスペクトルを１個表示 */
void ana_draw_mfccspect(SasProp *obj, GC gc, SasProp *src)
{
	ana_draw_mfccspect_sub(obj, gc, src, "MFCC",
	obj->view.analifter, obj->view.mfcctilt, obj->view.powmode);
}

/* スペクトル忠実版 MFCCスペクトルを１個表示 */
void ana_draw_mfccspect2(SasProp *obj, GC gc, SasProp *src)
{
	ana_draw_mfccspect_sub(obj, gc, src, "MFCC2",
	0/*lifter*/, 1/*tilt*/, 1/*powmode*/);
}


/* LPCスペクトルを１個表示 */
void ana_draw_lpcspect(SasProp *obj, GC gc, SasProp *src, float warp, int docep)
{
	int        start, len, nchan, fftsize, ffti, order, order2, xrsize;
	FILE       *fp=0;
	double     *buf=0;
	double     *xr=0, *xi=0, *av=0;
	int        size, bsize, skip, rept, swap;	// skip は 1/1000 sample
	int        ch, i, j, r, imin, imax;

	swap  = (src->file.endian == obj->view.cpuendian)? 0: 1;
	nchan = src->file.chan;
	start = src->view.ssel;
	len   = src->view.nsel;
	if(len<0) {len = -len; start -= len;}
	if( obj->view.single ) /* 一括LPC */
		bsize = len;
	else
		bsize = (double)obj->view.framesize * src->file.freq / 1000; /* 繰り返して平均 */
	skip  = (double)obj->view.frameskip * src->file.freq;

	if( len < bsize && !obj->view.single ){
		len = bsize; rept = 1;
	}
	else rept = 1 + (len-bsize)*1000/skip;

	/* FFTサイズは固定 */
/*	for( ffti=2, fftsize=4; fftsize<len; ffti++, fftsize*=2 ) ; */
/*	ffti = 9; fftsize = 512; */
	for( ffti=2, fftsize=4; fftsize<obj->view.fftmin; ffti++, fftsize*=2 );
	for(                  ; fftsize<obj->view.analpc*2; ffti++, fftsize*=2 );
	for(                  ; fftsize<obj->view.anacep*2; ffti++, fftsize*=2 );

	order = obj->view.analpc;
	order2 = obj->view.anacep;
	if( !docep ) order2 = order;
	xrsize = (fftsize*2>bsize)?fftsize*2:bsize;

	if( !(buf=malloc(sizeof(double)*len*nchan)) ) ;
	else if( !(xr=malloc(sizeof(double)*xrsize)) ) ;
	else if( !(xi=malloc(sizeof(double)*xrsize)) ) ;
	else if( !(av=malloc(sizeof(double)*(fftsize/2+1))) ) ;
	else if( !(fp=fopen(src->file.name,"r")) ) ;
	else if( (size=read_double(fp, src->file.hsize, src->file.offset, src->file.type,nchan,swap, start,len,buf)) <= 0 ) ;
	else {
		int   wd, ht, by, x, y, ox=0, oy=0;
		char  str[1024], modestr[16];
		int   fmin, fmax;
		float resid, power, scale, shift;

		wd = obj->win.size.width - obj->win.scalew;
		ht = obj->win.size.height - obj->win.scaleh*2;
		by = obj->win.scaleh + ht;

		if( obj->view.fscale == FSC_LOG ){
			fmin = obj->view.lfmin;
			fmax = obj->view.lfmax;
		}
		else {
			fmin = obj->view.fmin;
			fmax = obj->view.fmax;
		}
		if( warp != 0 ){
			imin = fftsize * MelWarp(-warp, M_PI*2*fmin/src->file.freq) / 2 / M_PI;
			imax = fftsize * MelWarp(-warp, M_PI*2*fmax/src->file.freq) / 2 / M_PI + 1;
		}
		else {
			imin = (double)fftsize * fmin / src->file.freq;
			imax = (double)fftsize * fmax / src->file.freq + 1;
		}
		if( imin < 0 ) imin = 0;
		if( imax > fftsize/2 ) imax = fftsize/2;

		scale = 10.0/log(10.0);     /* fftcep{10log10(P*len)} / lpccep{log(P)} の差 */
		shift = 10.0*log10((double)bsize); /* パワー項の正規化用 */

		for( ch=0; ch<nchan; ch++, obj->view.nline++ ){

#ifdef ANA_THREAD
			if( obj->view.tquit ) break;
#endif

			sas_change_gc(obj, gc, colorname[obj->view.nline%NUMCOLOR], 0);

			/* 凡例を描く */
			strcpy(modestr,"LPC");
			if( warp != 0.0 ) strcat(modestr,"MEL");
			if( docep ) strcat(modestr,"CEP");
			if( warp != 0.0 && docep ) strcpy(modestr,"LMCC");
			sprintf(str,"%s [%d.%d]%-6s %4.0f~%4.0fms(%dx%d)",
				src->file.name, src->win.id, ch, modestr,
				start*1000.0/src->file.freq, (start+len)*1000.0/src->file.freq,
				bsize, rept);
			y = (obj->view.nline+2) * obj->win.scaleh;
			x = obj->win.size.width - XTextWidth(obj->win.font_struct,str,strlen(str)) - 1;
			XDrawString(obj->win.disp, obj->win.pix, gc, x, y, str, strlen(str));

			/* グラフを描く */
			memset( av, 0, sizeof(double)*(fftsize/2+1) );
			for( r=0; r<rept; r++ ){
#ifdef ANA_THREAD
				if( obj->view.tquit ) break;
#endif
				for( i=0, j=r*skip/1000; i<bsize && j<size; i++, j++ )
					xr[i] = buf[j*nchan+ch];
				for( ; i<bsize; i++ )
					xr[i] = 0;
				if( obj->view.anapref )
					pre_emphasis(obj->view.anapre, xr, i);
				for( j=i;j<bsize; j++ ) xr[j] = 0;
				if( obj->view.anawin == HAMMING )
					hamming( xr, bsize );
				autocor( xr, xi, bsize, order );
				if( xi[0] < EPS ) xi[0] = EPS;
				for( i=1; i<=order; i++ ) xi[i] /= xi[0];
				xi[0] /= bsize; /* サンプルあたりのパワー */
				lagwin( xi, order, obj->view.analagbw );
				resid = coralf( order, xi, xr );
				if( resid < 0.0 ) resid = 1.0;
				if( obj->view.powmode == 0 ) resid = 1.0;
				power = 10.0 * log10( resid * xi[0] );
				if( warp != 0.0 )
					warp_alpha(xr, order, order2, warp);
				if( docep ){
					alfcep(xr, order, xi, order2); /* xr -> xi */
					memset( xr, 0, sizeof(double)*fftsize ); 
					for( i=1; i<=order2; i++ )
						xr[i] = xr[fftsize-i] = xi[i] * scale;
					xr[0] = power;
					memset( xi, 0, sizeof(double)*fftsize ); 
//printf("lpcc");
//for(i=0;i<order2;i++)printf(" %.2f",xr[i]);
//printf("\n");fflush(stdout);
				}
				else{
					xr[0] = 1.0;
					power -= shift; /* パワー項の正規化 *log(1/bsize) */
					for( i=0; i<=order; i++ )
						xr[i] /= (order + 1);
					memset( xi, 0, sizeof(double)*fftsize ); 
					memset( &xr[order+1], 0, sizeof(double)*(fftsize-order-1) );
				}
				fft( xr, xi, ffti, fftsize );
				for( i=0; i<=fftsize/2; i++ ){
					if( ! docep ){
						xr[i] = xr[i]*xr[i] + xi[i]*xi[i];
						if( xr[i] < EPS ) xr[i] = EPS;
						xr[i] = power - 10.0 * log10(xr[i]);
						/* LPC は FFT の後でlog(パワー)項を足す */
						/* CEP は FFT の前に足してあるのでループしない */
						/* 10log10(サンプルあたりの残差パワー * フィルタゲイン) */
					}
					av[i] += xr[i];
				}
			}

#ifdef ANA_THREAD
			if( obj->view.tquit ) break;
#endif

			for( i=imin; i<=imax; i++ ){
				float  f;
				av[i] /= rept;
				if( warp == 0.0 ){
					f = (float)src->file.freq * i / fftsize;
				}
				else{
					double w = M_PI * i / fftsize * 2;
					f = (w - 2 * atan2(warp*sin(w), 1.0+warp*cos(w)))
				  	* src->file.freq / M_PI / 2;
				}
				x = obj->win.scalew + ana_f2x(obj, wd, f);
				y = by - ht * (av[i] - obj->view.dbmin)
					/ (obj->view.dbmax - obj->view.dbmin);
				if( i>imin )
					ana_draw_clip_y_pix(obj, gc, ox,oy, x,y, obj->win.scaleh, by );
				ox = x; oy = y;
			}
		}
	}

	if( fp ) fclose(fp);
	if( av ) free(av);
	if( xi ) free(xi);
	if( xr ) free(xr);
	if( buf ) free(buf);
}



/* 全時系列窓を対象にスペクトル分析グラフを描く */
void ana_draw_spect(SasProp *obj)
{
	int        i;
	GC         gc;

	/* その時点の時系列オブジェクトで、場所が選ばれているのは分析する */
	obj->view.nline = 0;
	gc = sas_make_gc(obj, colorname[0]);
	for( i=0; i<g_nobj; i++ ){
		if( ! g_obj[i] ) continue;
		if( g_obj[i]->file.type == 'P' ) continue;
		if( (g_obj[i]->view.mode & MODE_ANALYZE) ) continue;
		if( g_obj[i]->view.nsel == 0 ) continue;

#ifdef ANA_THREAD
		if( obj->view.tquit ) break;
#endif /* ANA_THREAD */
		if( (obj->view.mode & MODE_FFTSPECT) == MODE_FFTSPECT )
			ana_draw_fftspect(obj, gc, g_obj[i]);
#ifdef ANA_THREAD
		if( obj->view.tquit ) break;
#endif /* ANA_THREAD */
		if( (obj->view.mode & MODE_LPCSPECT) == MODE_LPCSPECT )
			ana_draw_lpcspect(obj, gc, g_obj[i], 0.0/*nowarp*/, 0/*nocep*/);
#ifdef ANA_THREAD
		if( obj->view.tquit ) break;
#endif /* ANA_THREAD */
		if( (obj->view.mode & MODE_LPCMELSPECT) == MODE_LPCMELSPECT )
			ana_draw_lpcspect(obj, gc, g_obj[i], obj->view.anawarp, 0/*nocep*/);
#ifdef ANA_THREAD
		if( obj->view.tquit ) break;
#endif /* ANA_THREAD */
		if( (obj->view.mode & MODE_LPCCEPSPECT) == MODE_LPCCEPSPECT )
			ana_draw_lpcspect(obj, gc, g_obj[i], 0.0/*nowarp*/, 1)/*docep*/;
#ifdef ANA_THREAD
		if( obj->view.tquit ) break;
#endif /* ANA_THREAD */
		if( (obj->view.mode & MODE_LPCMELCEPSPECT) == MODE_LPCMELCEPSPECT )
			ana_draw_lpcspect(obj, gc, g_obj[i], obj->view.anawarp, 1/*docep*/);
#ifdef ANA_THREAD
		if( obj->view.tquit ) break;
#endif /* ANA_THREAD */
		if( (obj->view.mode & MODE_MFCCSPECT) == MODE_MFCCSPECT )
			ana_draw_mfccspect(obj, gc, g_obj[i]);
		if( (obj->view.mode & MODE_MFCCSPECT2) == MODE_MFCCSPECT2 )
			ana_draw_mfccspect2(obj, gc, g_obj[i]);
	}
	sas_free_gc(obj, gc);
	XFlush(obj->win.disp);
}

/* 条件を表示 */
void ana_draw_condition(SasProp *obj)
{
	char  str[128];
	if( obj->view.single )
		sprintf(str,"%s %s [single] PE %.2f, LP %d, WRP %g, CEP %d, FB %d, LFT %d, PW %s",
			(obj->view.nosync)?"*":" ",
			(obj->view.anawin==HAMMING)?"Ham":"Squ",
			obj->view.anapre*obj->view.anapref,
			obj->view.analpc, obj->view.anawarp, obj->view.anacep,
			obj->view.anafbnum, obj->view.analifter,
			(obj->view.powmode)?"Adj":"Cor"
		);
	else
		sprintf(str,"%s %s [%g,%g]ms, PE %.2f, LP %d, WRP %g, CEP %d, FB %d, LFT %d, PW %s",
			(obj->view.nosync)?"*":" ",
			(obj->view.anawin==HAMMING)?"Ham":"Squ",
			obj->view.framesize, obj->view.frameskip,
			obj->view.anapre*obj->view.anapref,
			obj->view.analpc, obj->view.anawarp, obj->view.anacep,
			obj->view.anafbnum, obj->view.analifter,
			(obj->view.powmode)?"Adj":"Cor"
		);
	XDrawString(obj->win.disp, obj->win.pix, obj->win.fggc,
		obj->win.scalew+2, obj->win.scaleh-1, str, strlen(str));
}

/* ana ウィンドウへは直接描かないので */
/* ウィンドウにコピーして表示する */
void ana_redraw_copyArea(SasProp *obj)
{
	XCopyArea(obj->win.disp, obj->win.pix, obj->win.win,
		obj->win.fggc, 0, 0, obj->win.size.width, obj->win.size.height, 0, 0);
	XFlush(obj->win.disp);
	if( obj->view.anaon )
		ana_cur_draw(obj);        /* 再描画 */
	if( obj->view.cson )
		ana_cross_hair(obj, obj); /* 再描画 */
}

#ifdef ANA_THREAD

/* THREAD MAIN */
void ana_draw_spect_tmain(SasProp *obj)
{
	obj->view.tstate = 2; /* 1のままでもよいが、形式的に変更 */
	ana_draw_spect(obj);
	obj->view.cson = 0;
	if( !obj->view.tquit ){   /* 中断した時は描画せず、見易さをとる */
		ana_redraw_copyArea(obj);
		obj->view.redraw = 0;
	}
	obj->view.tstate = 0;
	return;
}

/* THREAD CONTROL */
void ana_draw_spect_tstart(SasProp *obj)
{
	void   *vp;
	if( obj->view.tstate ){
		obj->view.tquit = 1;
		while( obj->view.tstate )
			usleep(1000);
		pthread_join(obj->view.thread, &vp);
		obj->view.tquit = 0;
	}
	obj->view.tstate = 1;
	pthread_create( &obj->view.thread, (pthread_attr_t*)NULL,
		(void*)ana_draw_spect_tmain, obj);
}

#endif /*     ANA_THREAD */

/* 音響分析結果を再描画 */
void ana_redraw(SasProp *obj)
{
	if( ! obj->win.win ) return;

	ana_check_resize(obj);    /* resize してたら redraw=1 になる */

	/* カーソル位置の分析ではきれいにアニメーションしたいから */
	/* この窓はwinに直接描かず, pix に描いてから張り付ける */
	if( obj->view.redraw ){
		ana_clear_win(obj);
		ana_draw_scale(obj);
		ana_draw_condition(obj);
#ifdef ANA_THREAD
		ana_draw_spect_tstart(obj);
#else
		ana_draw_spect(obj);
#endif
	} 

	/* 張り付ける */
#ifdef ANA_THREAD
	if( !obj->view.tstate ){ /* スレッド描画中はここでは描かない */
		ana_redraw_copyArea(obj);
	}
#else /* ANA_THREAD */
	ana_redraw_copyArea(obj); /* スレッド無しなら常にコピー */
	obj->view.redraw = 0;
#endif /* ANA_THREAD */

	return;
}

/* カーソル位置の文字で表示 */
void ana_print_position(SasProp *obj)
{
	char        str[60];
	int         x, w;
	sprintf(str, "(%.1f %.1f)", obj->view.csfreq, obj->view.csamp);
	w = XTextWidth(obj->win.font_struct, str,strlen(str));
	if( obj->view.csx < obj->win.size.width - w )
		x = obj->view.csx + 2;
	else
		x = obj->view.csx - w;
	XDrawString(obj->win.disp, obj->win.win, obj->win.csgc,
/*		obj->win.scalew+5, obj->win.size.height - obj->win.scaleh - 4,*/
		x, obj->view.csy - 2,
		str, strlen(str));
}

/* カーソル描画 */
void ana_cross_hair(SasProp *obj, SasProp *master)
{
	int  x, y;
	/* 横 */
	y = obj->win.size.height - obj->win.scaleh -
		(obj->win.size.height - obj->win.scaleh*2) *
		(master->view.csamp - obj->view.dbmin) /
			(obj->view.dbmax - obj->view.dbmin);
	if(  y < obj->win.scaleh ) y = obj->win.scaleh;
	if(  y > obj->win.size.height - obj->win.scaleh ) y = obj->win.size.height - obj->win.scaleh;
	XDrawLine(obj->win.disp, obj->win.win, obj->win.csgc,
		obj->win.scalew, y, obj->win.size.width, y);

	/* 縦 */
	x = obj->win.scalew + ana_f2x(obj,
		obj->win.size.width - obj->win.scalew, master->view.csfreq);
	if(  x < obj->win.scalew ) x = obj->win.scalew;
	if(  x > obj->win.size.width ) x = obj->win.size.width;
	XDrawLine(obj->win.disp, obj->win.win, obj->win.csgc,
		x, obj->win.scaleh, x, obj->win.size.height - obj->win.scaleh);

	/* 内部状態コピー */
	obj->view.csamp = master->view.csamp;
	obj->view.csfreq = master->view.csfreq;
	obj->view.csx = x;
	obj->view.csy = y;

	/* 位置を表示 */
	ana_print_position(obj);

	XFlush(obj->win.disp);
}

/* カーソル移動 */
void ana_move_cursor(SasProp *obj, int x, int y)
{
	/* RAW MOUSE POSITION mx, my */
	obj->view.mx = x;
	obj->view.my = y;

	obj->view.csamp = obj->view.dbmax -
		(obj->view.dbmax - obj->view.dbmin) *
		(y - obj->win.scaleh) / (obj->win.size.height - obj->win.scaleh*2);
	if( obj->view.csamp < obj->view.dbmin )
		obj->view.csamp = obj->view.dbmin;
	else if( obj->view.csamp > obj->view.dbmax )
		obj->view.csamp = obj->view.dbmax;

	if( obj->view.fscale == FSC_LOG ){
		float  lmin, lmax, lf;
		lmin = obj->view.lmin;
		lmax = obj->view.lmax;
		lf = lmin + (lmax - lmin) *
			(x - obj->win.scalew) / (obj->win.size.width - obj->win.scalew);
		obj->view.csfreq = pow(10.0, lf);
	}
	else if( obj->view.fscale == FSC_MEL ){
		float mf;
		mf = obj->view.mmin + (obj->view.mmax - obj->view.mmin) *
			(x - obj->win.scalew) / (obj->win.size.width - obj->win.scalew);
		obj->view.csfreq = (MelInv(mf)+0.5);
	}
	else{
		obj->view.csfreq = (
			(float)obj->view.fmin +
			(float)(obj->view.fmax - obj->view.fmin) * 
			(float)(x - obj->win.scalew) / (float)(obj->win.size.width - obj->win.scalew)
		);
	}
	if( obj->view.csfreq < 0 )
		obj->view.csfreq = 0;
/*	else if( obj->view.csfreq > obj->view.fmax  )
		obj->view.csfreq = obj->view.fmax ;
*/
}

/* 波形のカーソル位置の移動に合わせてグラフ描画 */
void ana_cur_move(SasProp *obj, SasProp *cur)
{

	/* 初めての時は最大のサンプリング周波数から表示の最大周波数を計算 */
	/* 窓を最初に開けた時に redraw の前に呼ばれるかもしれない */
	if( obj->view.fmax < 0 ) return;

	if( obj->view.anaon ){
		ana_cur_draw(obj); /* 消す */
		obj->view.anaon = 0;
		obj->view.cssmpl = -1;
	}
	if( cur->view.cson && !cur->view.b1sel 
		&& cur->view.mx >= cur->win.scalew/2 ){
		/* 選択中は現在位置のスペクトルを表示しない。見にくいので */
#if 0
		int i;
		for( i=0; i<g_nobj; i++ ){
			if( g_obj[i]==cur ||
			( !(g_obj[i]->view.mode & MODE_ANALYZE) && global_shift  ))
				ana_cur_make(obj, g_obj[i]);
		}
#endif
		ana_cur_make(obj, cur);
		ana_cur_draw(obj);
		obj->view.anaon = 1;
		obj->view.cssmpl = cur->view.cssmpl;
	}
}

/* ===============================================================*/
/* 信号処理ツール */
/* ===============================================================*/

/* プリエンファシス --  coef は正の値で普通 */
void pre_emphasis(float coef, double *buf, int len)
{
	while( --len > 0 )
		buf[len] -= coef * buf[len-1];
}

/* 自己相関を求める */
void autocor(double *wav, double *cor, int wsize, int order)
{
	int  i, k;
	for( k=0; k<=order; k++ )
		for( i=k,cor[k]=0.0; i<wsize; i++ )
			cor[k] += wav[i] * wav[i-k];
}

/* ラグ窓 */
void lagwin(double *cor, int order, float bw)
{
	int           i;
	double        param;
#ifdef ANA_THREAD
	double        lag;
	param = 0.5 * log((double)0.5) / log(cos((double)0.5 * M_PI * bw));
	lag = 1.0;
	for( i=1; i<=order; i++ ){
		lag = lag * (param-i+1) / (param+i);
		cor[i] *= lag;
	}
#else /* ANA_THREAD */
	static float  lagbw=0;
	static int    lagorder=0;
	static float *lag=0;

	if( order <= 0 || bw <= 0 )
		return;
	if( lag && lagorder != order )
		{free(lag); lag=0; lagbw=0; lagorder=-1;}
	if( !lag  && !(lag=malloc(sizeof(float)*(order+1))) ){
		fprintf(stderr,"can't alloc lag[%d]\n",order);
		return;
	}
	if( lagbw != bw ){
		lagbw = bw;
		lagorder = order;
		param = 0.5 * log((double)0.5) / log(cos((double)0.5 * M_PI * bw));
//printf("%f\n",param);fflush(stdout);
		lag[0] = 1.0;
		for( i=1; i<=order; i++ )
			lag[i] = lag[i-1] * (param-i+1) / (param+i);
	}
	for( i=1; i<=order; i++ )
		cor[i] *= lag[i];
#endif /* ANA_THREAD */
}

/* --------------------------------------------------------------------
自己相関からαパラメータを求める Levinson-Durbin 法
    cor[0]の内容は見ずに１とみなす。cor[1]からcor[order]が必要
    α[0]＝１，α[1]〜α[order]がもとまる
	残差パワーを返す
-------------------------------------------------------------------- */
double coralf(int order, double *cor, double *alf)
{
	int      i, j;
	double   ref, alfsave, resid;

	ref = cor[1];
	alf[1] = (-ref);
	alf[0] = 1.0;
	resid = 1.0-ref*ref;
	for( i=2; i<=order; i++ ) {
		ref = cor[i];
		for( j=1; j<i; j++ )
			ref += alf[j]*cor[i-j];
		ref /= resid;
		alf[i] = (-ref);
		for( j=1; 2*j<=i; j++ ) {
			alfsave = alf[j];
			alf[j] = alfsave-ref*alf[i-j];
			if(2*j != i)
				alf[i-j] -= ref*alfsave;
		}
		resid *= 1.0-ref*ref;
	}
	return resid;
}

/* LPC 係数をワープする */
void warp_alpha(double *alpha, int order, int order2, float warp)
{
	int    i, j;
	double pre, pre2, div, tmp[128];
	if( order >= 128 ) return;

	pre = alpha[order];
	tmp[0] = 0.0;
	for( i=1; i<=order; i++ ){
		tmp[i] = (1-warp*warp)*pre + warp*tmp[i-1];
		pre = alpha[order-i] + warp*pre;
	}

	div = pre;
	alpha[1] = tmp[order]/div;

	for( j=2; j<=order2; j++ ){
		pre = tmp[0];
		tmp[0] = -warp * tmp[0];
		for( i=1; i<=order; i++ ){
			pre2 = tmp[i];
			tmp[i] = pre + warp * (tmp[i-1] - tmp[i]);
			pre = pre2;
		}
		alpha[j] = tmp[order]/div;
	}
}

/* LPC から CEPへ */
void alfcep(double *alf, int aorder, double *cep, int corder)
{
	double   ss;
	int      i, m;

	if( aorder > corder ) aorder = corder;
	cep[1] = -alf[1];
	for( m=2; m<=aorder; m++ ) {
		ss = -alf[m] * m;
		for( i=1; i<m; i++ )
			ss -= alf[i] * cep[m-i];
		cep[m] = ss;
	}
	/* CEP order > LPC order のとき続きを求める */
	for( m=aorder+1; m<=corder; m++ ) {
		ss = 0.0;
		for( i=1; i<=aorder; i++ )
			ss -= alf[i] * cep[m-i];
		cep[m] = ss;
	}
	for( m=2; m<=corder; m++ )
		cep[m] /= m;
}

/* =================================================
カーソル分析ルーチン
================================================= */
#define        MAXBSIZE   (8192*2)


/* カーソル分析用の MFCC 分析 (各種オプション付) */
/* cur から obj->view.coord{x,y},  に */
int make_mfccspect_sub(SasProp *obj, SasProp *cur, double *buf, int len,
	int fftsize, int ffti, int imin, int imax, int n, float melmax,
	int lifter, int tilt, int powmode)
{
	int        wd, ht, by, order, nchan, ch, i, x, y;
	double     xr[MAXBSIZE*2+1], xi[MAXBSIZE*2+1];
	double     scale, scale2, shift;
	extern void Wav2MFCC_E_D(short *wave, float *mfcc, long sampsize,
	           long sfreq, int fbank_num, int raw_e, float preEmph,
	           int mfcc_dim, int lifter, float *mfbpf);
	int        fbnum;
	short      *wave;
	float      *mfcc;
	float      *mfbpf;
	extern int fbankpow;
	extern int fbanknormalize;

	wd = obj->win.size.width - obj->win.scalew;
	ht = obj->win.size.height - obj->win.scaleh*2;
	by = obj->win.scaleh + ht;
	nchan = cur->file.chan;
	order = obj->view.anacep;
	if( order > MAXBSIZE ) order = MAXBSIZE;
	fbnum = obj->view.anafbnum;
	if( fbnum > MAXBSIZE ) fbnum = MAXBSIZE;

	/*lifter = obj->view.analifter;*/
	if( powmode == 0 ){
		/* 自己相関 */
		scale = 10.0/log(10.0);
		shift = log((double)len);
	}
	else {
		/* フィルタバンク */
		scale = 20.0/log(10.0)/sqrt((double)fbnum);
		shift = log((double)len)/2*sqrt((double)fbnum);
	}
	scale2 = 20.0/log(10.0)/sqrt((double)fbnum)/(1+lifter/sqrt(2.0));
	wave = (short *)xr; /* 使い回し */
	mfcc = (float *)xi; /* 使い回し */
	mfbpf = (float *)xr; /* 使い回し (wave を壊す) */

	for( ch=0; ch<nchan; ch++ ){
		for( i=0; i<len; i++ )
			wave[i] = buf[i*nchan+ch];
		fbankpow = (powmode)?1:0;
		fbanknormalize = (tilt)?1:0;
		Wav2MFCC_E_D(wave, mfcc, len, cur->file.freq, fbnum, 0,
			obj->view.anapre*obj->view.anapref, order, lifter, mfbpf);
		fbankpow = 0;
		fbanknormalize = 0;
		memset(xr, 0, sizeof(double)*fftsize);
		for( i=1; i<=order; i++ )
			xr[i] = xr[fftsize-i] = scale2 * mfcc[i];
		xr[0] = scale * (mfcc[0] - shift);
		memset(xi, 0, sizeof(double)*fftsize);

		fft( xr, xi, ffti, fftsize );
		for( i=imin; i<=imax; i++ ){
			float f;
				f = MelInv(melmax * i / (fftsize / 2));
			x = obj->win.scalew + ana_f2x(obj, wd, f);
			obj->view.coordx[n] = x;  /* 項目3 */

			y = by - ht * (xr[i]-obj->view.dbmin)
				/ (obj->view.dbmax - obj->view.dbmin);
			obj->view.coordy[n] = y;  /* 項目3 */
			n ++;
		}
	}
	return n;
}

int make_mfccspect(SasProp *obj, SasProp *cur, double *buf, int len,
	int fftsize, int ffti, int imin, int imax, int n, float melmax)
{
	int     status;
	status = make_mfccspect_sub(obj, cur, buf, len, fftsize, ffti, imin, imax, n, melmax,
			obj->view.analifter, obj->view.mfcctilt, obj->view.powmode);
	return status;
}

int make_mfccspect2(SasProp *obj, SasProp *cur, double *buf, int len,
	int fftsize, int ffti, int imin, int imax, int n, float melmax)
{
	int     status;
	status = make_mfccspect_sub(obj, cur, buf, len, fftsize, ffti, imin, imax, n, melmax,
			0/*lifter*/, 1/*mfcctilt*/, 1/*powmode*/);

	return status;
}


/* カーソル分析用の LPC 分析 */
/* cur から obj->view.coord{x,y},  に */
int make_lpcspect(SasProp *obj, SasProp *cur, double *buf, int len,
	int fftsize, int ffti, int imin, int imax, int n, float warp, int docep)
{
	int        wd, ht, by, order, order2, nchan, ch, i, x, y;
	double     xr[MAXBSIZE*2+1], xi[MAXBSIZE*2+1];
	double     resid, power, scale, shift;

	wd = obj->win.size.width - obj->win.scalew;
	ht = obj->win.size.height - obj->win.scaleh*2;
	by = obj->win.scaleh + ht;
	nchan = cur->file.chan;
	order = obj->view.analpc;
	order2 = obj->view.anacep;
	if( order > MAXBSIZE/nchan ) order = MAXBSIZE/nchan;
	if( !docep ) order2 = order;

	scale = 10.0/log(10.0); /* fftcep{10log10(P*len)} / lpccep{log(P)} の差 */
	shift = 10.0*log10((double)len); /* パワー項を正規化 */

	for( ch=0; ch<nchan; ch++ ){
		for( i=0; i<len; i++ )
			xr[i] = buf[i*nchan+ch];
		if( obj->view.anapref )
			pre_emphasis(obj->view.anapre, xr, len);
		if( obj->view.anawin == HAMMING )
			hamming( xr, len );
		autocor( xr, xi, len, order );
		if( xi[0] < EPS ) xi[0] = EPS;
		for( i=1; i<=order; i++ ) xi[i] /= xi[0];
		xi[0] /= len;   /* サンプルあたりのパワー */
		lagwin( xi, order, obj->view.analagbw );
		resid = coralf( order, xi, xr ); 
		if( resid <= 0.0 ) resid = 1.0;
		if( obj->view.powmode == 0 ) resid = 1.0;
		power = 10.0 * log10( resid * xi[0] );
		if( warp != 0.0 )
			warp_alpha(xr, order, order2, warp); /* xr -> xr */
		if( docep ){
			alfcep(xr, order, xi, order2); /* xr -> xi */
			memset( xr, 0, sizeof(double)*fftsize ); 
			for( i=1; i<=order2; i++ )
				xr[i] = xr[fftsize-i] = xi[i] * scale;
			xr[0] = power;
			memset( xi, 0, sizeof(double)*fftsize ); 
		}
		else{
			xr[0] = 1.0;
			power -= shift;
			for( i=0; i<=order; i++ )
				xr[i] /= (order + 1);
			memset( xi, 0, sizeof(double)*fftsize ); 
			memset( &xr[order+1], 0, sizeof(double)*(fftsize-order-1) );
		}
		fft( xr, xi, ffti, fftsize );
		for( i=imin; i<=imax; i++ ){
			float f;
			if( warp == 0.0 ){
				f = (float)cur->file.freq * i / fftsize;
			}
			else{
				double w = M_PI * i / fftsize * 2;
				f = (w - 2 * atan2(warp*sin(w), 1.0+warp*cos(w)))
				  * cur->file.freq / M_PI / 2;
			}
			x = obj->win.scalew + ana_f2x(obj, wd, f);
			obj->view.coordx[n] = x;  /* 項目3 */

			if( !docep ){
				xr[i] = xr[i]*xr[i] + xi[i]*xi[i];
				if( xr[i] < EPS ) xr[i] = EPS;
				xr[i] = power - 10.0 * log10(xr[i]);
			}
			y = by - ht * (xr[i]-obj->view.dbmin)
				/ (obj->view.dbmax - obj->view.dbmin);
			obj->view.coordy[n] = y;  /* 項目3 */
			n ++;
		}
	}
	return n;
}

/* カーソル分析用の FFT 分析 */
/* cur から obj->view.coord{x,y},  に */
int make_fftspect(SasProp *obj, SasProp *cur, double *buf, int len,
	int fftsize, int ffti, int imin, int imax, int n)
{
	int        wd, ht, by, nchan, ch, i, x, y;
	double     xr[MAXBSIZE*2], xi[MAXBSIZE*2];
	double     pow;

	wd = obj->win.size.width - obj->win.scalew;
	ht = obj->win.size.height - obj->win.scaleh*2;
	by = obj->win.scaleh + ht;
	nchan = cur->file.chan;

	for(ch=0; ch<nchan; ch++){
		for( i=0; i<len; i++ ){
			xr[i] = buf[i*nchan+ch];
			xi[i] = 0.0;
		}
		if( obj->view.anapref )
			pre_emphasis(obj->view.anapre, xr, len);
		for( i=len; i<fftsize; i++ )
			xr[i] = xi[i] = 0.0;
		if( obj->view.anawin == HAMMING )
			hamming(xr, len);

		/* ここでパワー計算 */
		for( pow=0,i=0; i<len; i++ )
			pow += xr[i] *xr[i];
		pow = 10.0 * log10(pow/len);
		obj->view.coordpow[ch] = by - ht * (pow - obj->view.dbmin)
				/ (obj->view.dbmax - obj->view.dbmin);

		fft( xr, xi, ffti, fftsize );
		for( i=imin; i<=imax; i++ ){
			float f;
			f = (float)cur->file.freq * i / fftsize;
			x = obj->win.scalew + ana_f2x(obj, wd, f);
			obj->view.coordx[n] = x;

			xr[i] = xr[i]*xr[i] + xi[i]*xi[i];
			xr[i] /= len;
			if( xr[i] < EPS ) xr[i] = EPS;
			xr[i] = 10.0 * log10(xr[i]);
			y = by - ht * (xr[i]-obj->view.dbmin)
				/ (obj->view.dbmax - obj->view.dbmin);
			obj->view.coordy[n] = y;  /* 項目3 */
			n++;
		}
	}
	return n;
}

/* ====================================================*/
/* cur のカーソル位置を分析 */
/* ====================================================*/
void ana_cur_make(SasProp *obj, SasProp *cur)
{
	FILE       *fp;
	double     buf[MAXBSIZE]; /* あまり速く malloc free したくないというだけ */
	long       start, len, n;
	int        nchan, nline, size, i, swap;
	int        fftsize, ffti;
	int        fmin, fmax, imin, imax, iwmin, iwmax, immax, immin;
	float      melmax;

	/* ファイル開ける */
	if( (fp=fopen(cur->file.name,"r"))==0 )
		return;

	/* 読み込み位置とサイズ */
	len   = obj->view.framesize * cur->file.freq / 1000;
	start = cur->view.cssmpl - len/2;
	if( start < 0 ) {
		len += start;  /* ファイルの頭からはみ出したら短くする */
		start = 0;
	}
	nchan = cur->file.chan;
	if( len >= MAXBSIZE/nchan/2 ) /* 2 は LPC と FFT 両方のため */
		len = MAXBSIZE/nchan/2;  /* 現状は長すぎると削る */
	swap  = (cur->file.endian == obj->view.cpuendian)? 0: 1;

	n = read_double(fp, cur->file.hsize, cur->file.offset, cur->file.type, nchan, swap, start, len, buf);
	fclose(fp);
	if( n <= 0 ) return;
	if( n < len )
		memset(&buf[n*nchan], 0, sizeof(double)*nchan*(len-n));

	/* 分析設定 fftsize は分析手法によらず同じ */
//	for( ffti=2, fftsize=4; fftsize<len; ffti++, fftsize*=2 ) ;
	for( ffti=2, fftsize=4; fftsize<obj->view.fftmin; ffti++, fftsize*=2 ) ;
	for(                  ; fftsize<len; ffti++, fftsize*=2 ) ;
	if( obj->view.fscale == FSC_LOG ){
		fmin = obj->view.lfmin;
		fmax = obj->view.lfmax;
	}
	else{
		fmin = obj->view.fmin;
		fmax = obj->view.fmax;
	}
	imin = (double)fftsize * fmin / cur->file.freq;
	imax = (double)fftsize * fmax / cur->file.freq + 1;
	if( imin < 0 ) imin = 0;
	if( imax > fftsize/2 ) imax = fftsize/2;
	iwmin = fftsize * MelWarp(-obj->view.anawarp, M_PI*2*fmin/cur->file.freq) / 2 / M_PI;
	iwmax = fftsize * MelWarp(-obj->view.anawarp, M_PI*2*fmax/cur->file.freq) / 2 / M_PI + 1;
	if( iwmin < 0 ) iwmin = 0;
	if( iwmax > fftsize/2 ) iwmax = fftsize/2;
	melmax = Mel((float)cur->file.freq/2);
	immin = fftsize/2 * Mel((float)fmin) / melmax;
	immax = fftsize/2 * Mel((float)fmax) / melmax + 1;
	if( immin < 0 ) immin = 0;
	if( immax > fftsize/2 ) immax = fftsize/2;

	/* anach:ライン数, anasize:各ポイント数 を計算 */
	nline = 0;
	for( n=0; anamodelist[n]; n++ ){
		if( nline + nchan >= MAX_ANACH ) break;
		if( (obj->view.mode & anamodelist[n]) == anamodelist[n] ){
			for( i=0; i<nchan; i++, nline++ ){
				if( anamodelist[n] == MODE_MFCCSPECT )
					obj->view.anasize[nline] = immax - immin + 1;
				else if( anamodelist[n] == MODE_LPCMELSPECT
					  || anamodelist[n] == MODE_LPCMELCEPSPECT )
					obj->view.anasize[nline] = iwmax - iwmin + 1;
				else
					obj->view.anasize[nline] = imax - imin + 1;
				obj->view.anamode[nline] = n;
			}
		}
	}
	obj->view.anach = nline;
	obj->view.anafreq = cur->file.freq;
	for( size=0,i=0; i<nline; i++ )
		size += obj->view.anasize[i];
	if( obj->view.coordsize < size ){
		if( obj->view.coordsize > 0 ){
			obj->view.coordx = realloc(obj->view.coordx, sizeof(int) * size);
			obj->view.coordy = realloc(obj->view.coordy, sizeof(int) * size);
		}
		else{
			obj->view.coordx = malloc(sizeof(int) * size);
			obj->view.coordy = malloc(sizeof(int) * size);
		}
		obj->view.coordsize = size;
	}

	/* パワー用	*/
	if( obj->view.curch != cur->file.chan ){
		if( obj->view.coordpow )
			free( obj->view.coordpow );
		obj->view.coordpow = malloc(sizeof(int) * cur->file.chan);
		obj->view.curch = cur->file.chan;
	}

	/* グラフ作成 */
	n = 0; 
	if( (obj->view.mode & MODE_FFTSPECT) == MODE_FFTSPECT ){
		n = make_fftspect(obj, cur, buf, len, fftsize, ffti, imin, imax, n);
	}
	if( (obj->view.mode & MODE_LPCSPECT) == MODE_LPCSPECT ){
		n = make_lpcspect(obj, cur, buf, len, fftsize, ffti,
			imin, imax, n, 0.0/*nowarp*/, 0);
	}
	if( (obj->view.mode & MODE_LPCMELSPECT) == MODE_LPCMELSPECT ){
		n = make_lpcspect(obj, cur, buf, len, fftsize, ffti,
			iwmin, iwmax, n, obj->view.anawarp, 0);
	}
	if( (obj->view.mode & MODE_LPCCEPSPECT) == MODE_LPCCEPSPECT ){
		n = make_lpcspect(obj, cur, buf, len, fftsize, ffti,
			imin, imax, n, 0.0/*nowarp*/, 1);
	}
	if( (obj->view.mode & MODE_LPCMELCEPSPECT) == MODE_LPCMELCEPSPECT ){
		n = make_lpcspect(obj, cur, buf, len, fftsize, ffti,
			iwmin, iwmax, n, obj->view.anawarp, 1);
	}
	if( (obj->view.mode & MODE_MFCCSPECT) == MODE_MFCCSPECT ){
		n = make_mfccspect(obj, cur, buf, len, fftsize, ffti,
			immin, immax, n, melmax);
	}
	if( (obj->view.mode & MODE_MFCCSPECT2) == MODE_MFCCSPECT2 ){
		n = make_mfccspect2(obj, cur, buf, len, fftsize, ffti,
			immin, immax, n, melmax);
	}
}

/* カーソル位置のスペクトラムを描画 */
void ana_cur_draw(SasProp *obj)
{
	int   ch, i, n, x, y, ox, oy;
	int   ht, by, w, modeindex;
	GC         gc;
	XGCValues  gcv;
	char  *str;

	ht = obj->win.size.height - obj->win.scaleh*2;
	by = obj->win.scaleh + ht;

	gc = sas_make_gc(obj, colorname[0]);
	gcv.function = GXxor;
	XChangeGC(obj->win.disp, gc, GCFunction, &gcv);

	/* ここでの ch は総ライン数(ch数 x 分析手法数) */
	n=0;
	for(ch=0; ch<obj->view.anach; ch++){
		sas_change_gc(obj, gc, colorname[ch%NUMCOLOR], 0);
		ox = 0; oy = 0;
		for( i=0; i<obj->view.anasize[ch]; i++ ){
			x = obj->view.coordx[n];
			y = obj->view.coordy[n];
			if( i>0 )
				ana_draw_clip_y_win(obj, gc, ox,oy, x,y, obj->win.scaleh, by );
			ox = x; oy = y;
			n++;
		}
		/* 分析凡例表示 */
		modeindex = obj->view.anamode[ch];
		str  = (char *)anamodestr[modeindex];
		w = XTextWidth(obj->win.font_struct, str, strlen(str));
		XDrawString(obj->win.disp, obj->win.win, gc,
				obj->win.size.width-w, by - obj->win.fonta*ch, str, strlen(str));

		/* パワー表示 */
		if( anamodelist[modeindex] == MODE_FFTSPECT ){
			ana_draw_clip_y_win(obj, gc,
				obj->win.scalew, obj->view.coordpow[ch],
				obj->win.scalew-20,obj->view.coordpow[ch],
				obj->win.scaleh, by);
			ana_draw_clip_y_win(obj, gc,
				obj->win.scalew, obj->view.coordpow[ch],
				obj->win.scalew-10,obj->view.coordpow[ch]+2,
				obj->win.scaleh, by);
		}
	}
	XFlush(obj->win.disp);
	sas_free_gc(obj, gc);
}

/* ==========================================================================
分析窓のズーム
========================================================================== */
void ana_zoom(SasProp *obj, int x, int y, float ratio)
{
	int      w1, w2, h0, h1, h2;
	int      fc, fr;
	float    dbc, dbr;
	w1 = obj->win.scalew;
	w2 = obj->win.size.width;
	h0 = obj->win.size.height;
	h1 = h0 - obj->win.scaleh;
	h2 = obj->win.scaleh;
	/* 縦 */
	if( h1 > y && y > h2 ){
		dbr = obj->view.dbmax - obj->view.dbmin;
		dbc = obj->view.dbmin + dbr * (h1-y) / (h1-h2);
		dbc -= ((obj->view.dbmax + obj->view.dbmin)/2 - dbc) * ratio / 2;
		obj->view.dbmin = dbc - (dbc - obj->view.dbmin) * ratio;
		obj->view.dbmax = dbc + (obj->view.dbmax - dbc) * ratio;
		if( ratio > 1.0 ){
			obj->view.dbmin = -10;
			obj->view.dbmax = 100;
		}
	}
	/* 横 */
	if( w1 < x && x < w2 ){
		if( obj->view.fscale == FSC_LOG ){
			if( ratio > 1.0 ){
				obj->view.lfmin = DEF_FMIN;
				obj->view.lmin  = log10((double)obj->view.lfmin);
				obj->view.lfmax = ana_get_fmax(obj);
				obj->view.lmax = log10((double)obj->view.lfmax);
			}
		}
		else{ /* LINEAR, MEL */
			fr = obj->view.fmax - obj->view.fmin;
			fc = obj->view.fmin + fr * (x-w1) / (w2-w1);
			fc -= ((obj->view.fmax + obj->view.fmin)/2 - fc) * ratio / 2;
			obj->view.fmin = fc - (fc - obj->view.fmin) * ratio;
			obj->view.fmax = fc + (obj->view.fmax - fc) * ratio;
			if( obj->view.fmin < 0 ) {
				obj->view.fmax = obj->view.fmax - obj->view.fmin;
				obj->view.fmin = 0;
			}
			if( obj->view.fmax - obj->view.fmin < 5 )
				obj->view.fmax = obj->view.fmin + 5;
			if( ratio > 1.0 ){
				obj->view.fmin = 0;
				obj->view.fmax = ana_get_fmax(obj);
			}
			obj->view.mmin = Mel(obj->view.fmin);
			obj->view.mmax = Mel(obj->view.fmax);
		}
	}

	obj->view.redraw = 1;
	obj->view.cson = 0;
	ana_redraw(obj);
}

/* ==========================================================================
分析窓の移動,ズーム
========================================================================== */
#define  SAFE   10   /* SAFE: 極端な動きを押える */
void ana_move_scale(SasProp *obj, int x, int y)
{
	int      w1, w2, h0, h1, h2;
	int      fc, fr;
	float    dbc, dbr;
	int      ox, oy;
	int      mode;
	w1 = obj->win.scalew;
	w2 = obj->win.size.width;
	h0 = obj->win.size.height;
	h1 = h0 - obj->win.scaleh;
	h2 = obj->win.scaleh;

	if( !obj->view.b1mot ){ /* 1回め */
		if( h1 > y && y > h2 && x < w1 ){  /* 縦スケール */
			if( global_ctrl ) obj->view.dragmode = DR_VZOOM;
			else obj->view.dragmode = DR_VMOVE;
		}
		else if( h1 < y && w1 < x && x < w2 ){ /* 横スケール */
			if( global_ctrl ) obj->view.dragmode = DR_HZOOM;
			else obj->view.dragmode = DR_HMOVE;
		}
		else if( h1 > y && y > h2 && w1 < x && x < w2 ){ /* グラフ内 */
			if( global_ctrl ) obj->view.dragmode = DR_ZOOM;
			else obj->view.dragmode = DR_MOVE;
		}
		else
			obj->view.dragmode = DR_NONE;
		return;
	}
	/* view.mx, view.my は１回前のカーソル位置 */

	mode = obj->view.dragmode;

	/* 縦軸 */
	dbr = obj->view.dbmax - obj->view.dbmin;
	dbc = dbr * (y - obj->view.my) / (h1 - h2);
	if( mode == DR_MOVE || mode == DR_VMOVE ){
		obj->view.dbmin += dbc;
		obj->view.dbmax += dbc;
	}
	else if( mode == DR_VZOOM || mode == DR_ZOOM ){
		if( y > h1 - SAFE ) y = h1 - SAFE;
		oy = obj->view.my;
		if( oy > h1 - SAFE ) oy = h1 - SAFE;
		obj->view.dbmax = obj->view.dbmin + dbr * (h1 - oy) / (h1 - y);
	}

	/* 横軸 */
	if( obj->view.fscale == FSC_LOG ){
		float lr, lc;
		lr = obj->view.lmax - obj->view.lmin;
		lc = lr * (x - obj->view.mx) / (w2 - w1); /* 増減 */
		if( mode == DR_MOVE || mode == DR_HMOVE ){
			obj->view.lmin -= lc;
			obj->view.lmax -= lc;
			if( obj->view.lmin < log10((double)MIN_LFMIN) ){
				obj->view.lmin = log10((double)MIN_LFMIN);
				obj->view.lmax = obj->view.lmin + lr;
			}
			if( obj->view.lmax > log10((double)MAX_FMAX) ){
				obj->view.lmax = log10((double)MAX_FMAX);
				obj->view.lmin = obj->view.lmax - lr;
			}
			obj->view.lfmin = pow(10.0, obj->view.lmin);
			obj->view.lfmax = pow(10.0, obj->view.lmax);
		}
		else if( mode == DR_HZOOM || mode == DR_ZOOM ){
			if( x < w1 + SAFE ) x = w1 + SAFE;
			ox = obj->view.mx;
			if( ox < w1 + SAFE ) ox = w1 + SAFE;
			obj->view.lmax = obj->view.lmin + lr * (ox - w1) / (x - w1);
			if( obj->view.lmax < obj->view.lmin + MIN_FRANGE )
				obj->view.lmax = obj->view.lmin + MIN_FRANGE;
			if( obj->view.lmax > log10((double)MAX_FMAX) ){
				obj->view.lmax = log10((double)MAX_FMAX);
			}
			obj->view.lfmax = pow(10.0, obj->view.lmax);
		}
	}
	else { /* LINEAR, MEL */
		fr = obj->view.fmax - obj->view.fmin;
		fc = fr * (x - obj->view.mx) / (w2 - w1);
		if( mode == DR_MOVE || mode == DR_HMOVE ){
			obj->view.fmax -= fc;
			obj->view.fmin -= fc;
			if( obj->view.fmin < 0 ){
				obj->view.fmax -= obj->view.fmin;
				obj->view.fmin = 0;
			}
			if( obj->view.fmax > MAX_FMAX ){
				obj->view.fmin -= (obj->view.fmax - MAX_FMAX);
				obj->view.fmax = MAX_FMAX;
			}
		}
		else if( mode == DR_HZOOM || mode == DR_ZOOM ){
			if( x < w1 + SAFE ) x = w1 + SAFE;
			ox = obj->view.mx;
			if( ox < w1 + SAFE ) ox = w1 + SAFE;
			obj->view.fmax = obj->view.fmin + (float)fr * (ox - w1) / (x - w1);
			if( obj->view.fmax - obj->view.fmin < 10 )
				obj->view.fmax = obj->view.fmin + 10;
			if( obj->view.fmax > MAX_FMAX )
				obj->view.fmax = MAX_FMAX;
		}
		obj->view.mmax = Mel((float)obj->view.fmax);
		obj->view.mmin = Mel((float)obj->view.fmin);
	}

	obj->view.redraw = 1;
	ana_redraw(obj);
}

/* ==========================================================================
分析窓のイベント処理
========================================================================== */
void ana_dispatch(SasProp *obj, XEvent *event)
{
	KeySym          keysym;
	XComposeStatus  status;
	char            str[128];
	void            close_window(SasProp *obj);

	switch( event->type ){
#ifdef WM_CLOSE
	case ClientMessage:
		if(//event->xclient.message_type == WM_PROTOCOLS &&
				event->xclient.format == 32 &&
				event->xclient.data.l[0] == WM_DELETE_WINDOW)
			close_window(obj);
		break;
#endif
	case MappingNotify: /* keyboard mapping changes */
		XRefreshKeyboardMapping( &event->xmapping );
		break;

	case Expose:
		if( event->xexpose.count != 0 ) break;
		ana_redraw(obj);
		break;

	case LeaveNotify:
		/* ERASE CURSOR */
		if( obj->view.cson ){
			ana_cross_hair(obj, obj);
		}
		obj->view.cson = 0;
		break;

	case EnterNotify:
	case MotionNotify:
		sas_skip_motions(event);   /* 最後の Motion まで読みとばす */

		if( event->xmotion.x < obj->win.scalew )  /* 縦スケール */
			sas_change_cursor(obj,CS_UD);            /* 上下矢印 */
		else if( event->xmotion.y > obj->win.size.height - obj->win.lblh )
			sas_change_cursor(obj,CS_LR);            /* 左右矢印 */
		else
			sas_change_cursor(obj,CS_NORMAL);        /* 標準 */

		if( obj->view.b1press ){
			obj->view.b1mot = 1;
			ana_move_scale(obj, event->xmotion.x, event->xmotion.y);
		}

		/* ERASE CURSOR */
		if( obj->view.cson )
			ana_cross_hair(obj, obj);
		/* MOVE CURSOR */
		ana_move_cursor(obj, event->xmotion.x, event->xmotion.y);
		/* PUT CURSOR */
		ana_cross_hair(obj, obj);
		obj->view.cson = 1;

		break;

	case ButtonPress:
		if( event->xbutton.button == 1 ){
			if( event->xmotion.x < obj->win.scalew 
					|| event->xmotion.y > obj->win.size.height - obj->win.lblh ) {
				obj->view.b1press = 1;
				obj->view.b1mot = 0;
				obj->view.ox = event->xmotion.x;
				ana_move_scale(obj, event->xmotion.x, event->xmotion.y);
			}
		}
		else if( event->xbutton.button == 2 ){
			if( ana_para_menu(obj) >= 0 ){
				obj->view.redraw = 1;
				ana_redraw(obj);
			}
		}
		else if( event->xbutton.button == 3 ){
			if( event->xmotion.x < obj->win.scalew
			 || event->xmotion.y > obj->win.size.height - obj->win.lblh ) {
				obj->view.b3press = 1;
			}
			else {
				if( ana_mode_menu(obj) >= 0 ){
					obj->view.redraw = 1;
					ana_redraw(obj);
				}
			}
		}
		break;

	case ButtonRelease:
		if( event->xbutton.button == 1 ){
			if( obj->view.b1press == 1 ){
				/*
				if( event->xmotion.x < obj->win.scalew ||
				    event->xmotion.y > obj->win.size.height - obj->win.lblh)
				*/
				if( obj->view.b1mot == 0 ){
					ana_zoom(obj, event->xbutton.x, event->xbutton.y, 0.707);
				}
				else {
					obj->view.redraw = 1;
					ana_redraw(obj);
				}
				obj->view.b1press = 0;
				obj->view.b1mot = 0;
			}
		}
		else if( event->xbutton.button == 2 ){
		}
		else if( event->xbutton.button == 3 ){
			if( obj->view.b3press == 1 ){
				ana_zoom(obj, event->xbutton.x, event->xbutton.y, 1.4142);
				obj->view.b3press = 0;
			}
		}
		break;

	case KeyPress:
		*str = 0;
		XLookupString(&event->xkey,str,128,&keysym,&status);
		switch(keysym){
		case XK_Shift_L:
		case XK_Shift_R: global_shift = 1; break;
		case XK_Control_L:
		case XK_Control_R: global_ctrl = 1; break;
		case XK_Meta_L:
		case XK_Meta_R: global_meta = 1; break;
		case XK_Alt_L:
		case XK_Alt_R: global_alt = 1; break;
		default: break;
		}
		switch(*str){
		/* CTRL KEY */
		case 'L'-'@': obj->view.redraw = 1; ana_redraw(obj); break;
		case 'Q'-'@': exit(0); break;
		case 'X'-'@': close_window(obj); break;
		/* NUMERICAL KEY */
		case '0': obj->view.mode =
			(obj->view.mode ^ MODE_FFTSPECT) | MODE_ANALYZE;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
		case '9': obj->view.mode =
			(obj->view.mode ^ MODE_LPCSPECT) | MODE_ANALYZE;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
		case '8': obj->view.mode =
			(obj->view.mode ^ MODE_LPCMELSPECT) | MODE_ANALYZE;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
		case '7': obj->view.mode =
			(obj->view.mode ^ MODE_LPCCEPSPECT) | MODE_ANALYZE;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
		case '6': obj->view.mode =
			(obj->view.mode ^ MODE_LPCMELCEPSPECT) | MODE_ANALYZE;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
		case '5': obj->view.mode =
			(obj->view.mode ^ MODE_MFCCSPECT) | MODE_ANALYZE;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
		case '4': obj->view.mode =
			(obj->view.mode ^ MODE_MFCCSPECT2) | MODE_ANALYZE;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;

		/* ALPHABETICAL KEY */
		case 'p':
			obj->view.anapref = (obj->view.anapref)?0:1;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
/*
		case 'e':
			obj->view.anapow = (obj->view.anapow)?0:1;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
*/
		case 'l':
			obj->view.nosync = (obj->view.nosync)?0:1;
			if( obj->view.nosync )
				ana_draw_condition(obj);
			else 
				obj->view.redraw = 1;
			ana_redraw(obj);
			break;
		case 's':
			if( obj->view.fscale == FSC_LINEAR ) obj->view.fscale = FSC_MEL;
			else if( obj->view.fscale == FSC_MEL ) obj->view.fscale = FSC_LOG;
			else if( obj->view.fscale == FSC_LOG ) obj->view.fscale = FSC_LINEAR;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
		case 'w': obj->view.single = 1 - obj->view.single;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
		case 'h':
			if( obj->view.anawin == SQUARE ) obj->view.anawin = HAMMING;
			else obj->view.anawin = SQUARE;
			obj->view.redraw = 1;
			ana_redraw(obj);
			break;
		default: break;
		}
		break;

	case KeyRelease:
		*str = 0;
		XLookupString(&event->xkey,str,128,&keysym,&status);
		switch(keysym){
		case XK_Shift_L:
		case XK_Shift_R: global_shift = 0; break;
		case XK_Control_L:
		case XK_Control_R: global_ctrl = 0; break;
		case XK_Meta_L:
		case XK_Meta_R: global_meta = 0; break;
		case XK_Alt_L:
		case XK_Alt_R: global_alt = 0; break;
		default: break;
		}
		break;

	default:
		break;
	}
	return;
}
