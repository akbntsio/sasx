#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include "sas.h"
#include "label.h"
#include "child.h"

#ifndef VERSTR
#define VERSTR "Beta"
#endif

/* 使い方 */
void usage(FILE *fp)
{
fprintf(fp,"\
SPEECH ANALYSIS SYSTEM  Ver.%s\n\
sasx [option] file [[option] file ...]\n\
    -h                   this help message\n\
    -display dispname    X server name                    \n\
    -t {C|S|L|F|D|A|P}   C:char S:short F:float D:double  [N]\n\
                         A:ascii P:plottxt(x,y) N:auto    \n\
    -c chan              # of channels                    [1]\n\
    -e {l|b}             {little|big} endian              [l]\n\
    -f freq              sampling frequency(kHz)          [12]\n\
    -g WxH+X+Y           window geometry                  [512x128+0+0]\n\
    -b pixel             window manager border width      [0]\n\
    -l labelfile         labelfile                        \n\
    -m frameskip         for analysis and scale(ms)       [5]\n\
    -n framesize         for analysis window(ms)          [20]\n\
    -s start,len         select area(sample)              [0,0]\n\
    -v start,len         view area(sample)                all\n\
    -y ymin,ymax         amplitude view area              [-32768,32767]\n\
    -d dbmin,dbmax       power(db) view area              [-10,100]\n\
    -L save=filename     labelfilename to save            [no save]\n\
    -L std[={0|1}]       edit label from stdin            [no edit]\n\
    -L mode={0|1|2|3}    0:dup 1:sft 2:splt 3:splt-dup    [0]\n\
    -L arc[={0|1}]       draw arc around string           [0]\n\
    -L type={0|1|2|3}    0:auto 1:seg 2:atrphm 3:cvi      [0]\n\
    -M [mpoigsfFz]       startup mode(key bindings)       \n\
    -N size,skip         narrowband analysis(ms)          [100.0,10.0]\n\
    -W size,skip         wideband analysis(ms)            [5.0,0.5]\n\
    -V skip(fb),skip(FB) view skip speed(%%)              [100.0,10.0]\n\
    -S spow=val          spectra contrast(0.1,1)          [0.35]\n\
    -S sgain=val         spectra saturation(10-30000)     [2000]\n\
    -S color={0|1|2}     spectra 0:BW 1:gray 2:color      [0]\n\
    -S power={0|1}       spectra 0:off 1:on               [1]\n\
    -S fmax=freq         spectra maxfreq. (0:auto)        [0]\n\
    -S fscale={0|1}      spectra 0:linear 1:mel           [0]\n\
    -S pre=coef          pre-emphasis for spectrogram     [0.95]\n\
    -C name=color        name{fg|bg|sc|cs|mk|dim}         \n\
                         fg:foreground\n\
                         bg:background\n\
                         sc:scale\n\
                         cs:cursor\n\
                         mk:marker\n\
                         dim:dimmed spectrogram\n\
    -C name=pixel        name{mw|mgn|pm}                  [1|4|0]\n\
                         mw:  label_marker_width\n\
                         mgn: grab_margin_width\n\
                         pm:  plot_marker_triangle\n\
    -C font=name         8bit ascii font                  [fixed]\n\
    -C font16=name       16bit japanese font              [k14]\n\
    -a                   create analyze window            \n\
    -A pre=coef          pre_emphasis for analyze         [0.98]\n\
    -A lag=coef          lag window coef for lpc          [0.01]\n\
    -A lpc=order         lpc order for analyze            [16]\n\
    -A warp=coef         warp Coef. in lpc-Mel            [0.38]\n\
    -A cep=order         cep order for analyze            [12]\n\
    -A fbnum=size        MFCC filterbank size             [24]\n\
    -A lifter=size       MFCC lifter length               [22]\n\
\n\
key bindings(time window):\n\
    zZ:timeZoomUp/Down   gG:gainUp/Down        a:All\n\
    ^A:ViewStart         ^E:ViewLast\n\
    FB:ViewArea(100%%)    fb:ViewArea(10%%)   ^F^B:ViewArea(pixel)\n\
     n:new_window         y:SyncViewArea       m:mode(wave/spect)\n\
     M:mode(wave/mfcc)\n\
     i:TogglePitch        o:ToggleEnergy      pP:frequencyZoom(Up/Down)\n\
     r:ToggleDrawArc      Tab:SelNextSeg       s:scale(sec/HMS/smpl/frame)\n\
     l:toggleNoSync        ^Q:quit            ^X:closeWin\n\
     0:CreateAnalysisWin\n\
key bindings(analyze window):\n\
     0:toggleFFT   9:toggleLPC   8:toggleLPCMEL   7:toggleLPCCEP\n\
     6:toggleLPCMELCEP           5:toggleMFCC\n\
     w:toggleAverage-Single      s:scale(linear/mel/log)\n\
     h:toggleHammingWindow       e:toggleEnergy\n\
     l:toggleLockSync\n\
\n\
mouse button bindings(time window):\n\
    MB1:select       shift-MB1:selectSync\n\
    ctrl-MB1onLeftscale:Freq-zoom  ctrl-MB1onSpectrogram:selectRect\n\
    MB2:menu\n\
    MB3:playback\n\
mouse button bindings(analyze window):\n\
    MB1click:zoom*1.4      MB1+drag:move    ctrl+MB1+drag:zoom\n\
    MB2:parameter-menu                 MB3:mode-menu\n\
label edit mode:\n\
    MB1:select,moveSeg    shift-MB1:moveAddSeg    ctrl-MB1:separateSeg\n\
    DEL,BS:delete         u:undo(moveSeg)         !:mergeWithPrevious\n\
    TAB:nextSeg           shift-TAB:prevSeg       +:addLabel\n\
", VERSTR);
}

/* option: create_sas_window に渡すためのオプション */
typedef struct {
	char *dispname;
	char *fname;
	char *geom;
	int  bdskip;	/* window manager border skip */
	int   slave;
	char *font, *font16;
	char *lfile;
	char *lsave;
	int   type, endian, chan;
	int   vstart, vlen, sstart, slen;
	float ymin, ymax;
	int   dbmin, dbmax;
	float vskip, vskips;  /* view skip bounded to fb,FB */
	float skip, size;     /* scale skip, for analysis window */
	float wbskip, wbsize;
	float nbskip, nbsize;
	float freq;
	float pre;
	float anapre;
	float analagbw;
	int   analpc;
	float anawarp;
	int   anacep;
	int   anafbnum;
	int   analifter;
	float spow, sgain, smax;
	int   grayscale;
	int   power;
	int   fscale;
	char *fg,*bg,*cs,*sc,*mk,*dim;
	int   mw;
	int   mgn;
	int   std;
	char *mode;
	int   plotmk;
	int   lblmode;
	int   lblarc;
	int   lbltype;
	int   specdiff;
} Option;

/* sasのオブジェクト管理用 */
#define MAX_OBJ 512
Display *g_disp = NULL;     /* 表示先Xサーバ：基本的にモニタは１個 */
SasProp *g_obj[MAX_OBJ];    /* オブジェクト */
int      g_nobj=0;          /* オブジェクト数 */
Option   option;            /* オプション */

/* オプションの初期化 */
void initialize_option()
{
	option.dispname=":0.0";
	option.fname=NULL;
	option.geom=NULL;
	option.bdskip=0;
	option.slave=0;
	option.font=NULL;
	option.font16=NULL;
	option.lfile=NULL;
	option.lsave=NULL;
	option.type=0;
	option.endian=0;
	option.chan=0;
	option.vstart=-1;
	option.vlen=-1;
	option.sstart=-1;
	option.slen=-1;
	option.ymax=32768;
	option.ymin=-32768;
	option.dbmax=100;
	option.dbmin=-10;
	option.vskip=100;
	option.vskips=10;
	option.skip=0.0;
	option.size=0.0;
	option.wbskip=0.0;
	option.wbsize=0.0;
	option.nbskip=0.0;
	option.nbsize=0.0;
	option.freq=0;
	option.pre=10;		/* 10 デフォルト使用 */
	option.anapre=0.98;	/*  */
	option.analpc=16;
	option.anawarp=0.38;
	option.anacep=12;
	option.anafbnum=24;
	option.analifter=22;
	option.analagbw=0.01;
	option.spow=-1.0;	/* -1 デフォルト使用 */
	option.sgain=-1.0;	/* -1 デフォルト使用 */
	option.smax=-1.0;	/* -1 デフォルト使用 */
	option.grayscale=0; /* 0:BWdither */
	option.power=1;     /* Power 表示 */
	option.fscale=0;    /* 0:linear 1:mel */
	option.fg="";		/* 空文字列 デフォルト */
	option.bg="";
	option.cs="";
	option.sc="";
	option.mk="";
	option.dim="";
	option.mw=0;		/* デフォルトを使う時０ */
	option.mgn=-1;		/* デフォルトを使う時-1 */
	option.std=0;
	option.mode="";
	option.plotmk=0;
	option.lblmode=0;   /* ぱっキング無し */
	option.lblarc=0;   /* ぱっキング無し */
	option.lbltype=LBL_AUTO;   /* 0:auto 1:seg 2:atr 3:other */
	option.specdiff=0;
}


/* 指定したウィンドウの真下の位置を得る */
/* ウィンドウマネージャのつける枠の真下の位置を返す */
void get_slave_xy(SasProp *obj, int *newx, int *newy)
{
	Window win, root, par, *child;
	unsigned int nchild;
	int x, y;
	unsigned int w, h, b, d, newh, neww;
	par = obj->win.win;
	do{
		win = par;
		XQueryTree(obj->win.disp, win, &root, &par, &child, &nchild);
	}while( par != root );
	XGetGeometry(obj->win.disp, win, &root, &x, &y, &w, &h, &b, &d);
	*newx = x;
	*newy = y + h + option.bdskip;
	newh = h;
	neww = w;

#if 1
	/* はみ出したら右隣に移動 */
	win = root;
	XGetGeometry(obj->win.disp, win, &root, &x, &y, &w, &h, &b, &d);
	if( *newy + newh > h ){
		*newy = 0;
		*newx += neww; 
	}
#endif
}


/* オプションに従って新しい窓を開ける */
void create_sas_window(Option *option)
{
	char      *p;
	SasProp   *obj;

	/* 始めての窓を開ける時はディスプレイも開ける */
	if( ! g_disp ){
#ifdef ANA_THREAD
		XInitThreads();
#endif /* ANA_THREAD */
		g_disp = XOpenDisplay(option->dispname);
	}

	/* 多すぎ */
	if( g_nobj >= MAX_OBJ ){
		fprintf(stderr,"too many objects\n");
		return;
	}

	/* まずは窓のオブジェクトを生成する(まだ窓は開かない) */
	obj = g_obj[g_nobj] = sas_create(g_disp);
	obj->win.id = g_nobj;

	/* オプションどおりにオブジェクトを設定する */
	obj->lblmode = option->lblmode;
	obj->lblarc = option->lblarc;
	obj->lbltype = option->lbltype;
	if( option->lfile )    sas_read_label(obj, option->lfile);
	if( option->lsave )    strcpy(obj->lblsave, option->lsave);
	if( option->std )      obj->std=1;
	if( option->type )     obj->file.type = option->type;
	if( option->endian )   obj->file.endian = option->endian;
	if( option->chan )     obj->file.chan = option->chan;
	if( option->freq > 0 ) obj->file.freq = (int)(1000.0001*option->freq);
	if( g_nobj > 0 ) /* デフォルトは真下の位置をセットする */
		get_slave_xy(g_obj[g_nobj-1], &obj->win.size.x, &obj->win.size.y);
	if( option->geom ) {
		char *p;
		sas_parse_geometry(obj, option->geom);
		if( (p=strchr(option->geom,'+')) || (p=strchr(option->geom,'-')) )
			 *p = 0;
		/* 以後はサイズだけを反映して真下に表示を続ける */
	}
	obj->view.ymin    = option->ymin;
	obj->view.ymax    = option->ymax;
	obj->view.vskip   = option->vskip;
	obj->view.vskips  = option->vskips;
	obj->view.grayscale = option->grayscale;
	obj->view.power = option->power;
	obj->view.fscale = option->fscale;
	obj->view.specdiff = option->specdiff;
	if( option->font )   strcpy(obj->win.fontname,option->font);
	if( option->font16 ) strcpy(obj->win.fontname16,option->font16);
	if( option->skip > 0 ) obj->view.frameskip = option->skip;
	if( option->size > 0 ) obj->view.framesize = option->size;
	if( option->wbskip > 0 ) obj->view.frameskipwb = option->wbskip;
	if( option->wbsize > 0 ) obj->view.framesizewb = option->wbsize;
	if( option->nbskip > 0 ) obj->view.frameskipnb = option->nbskip;
	if( option->nbsize > 0 ) obj->view.framesizenb = option->nbsize;
	if( option->spow  > 0.0 ) obj->view.spow = option->spow;
	if( option->sgain > 0.0 ) obj->view.sgain = option->sgain;
	if( option->smax > 0.0 ) obj->view.smax = option->smax;
	if( option->pre != 10.0 ) obj->view.pre = option->pre;
	if( option->fg[0] != 0 )  obj->win.fg = option->fg;
	if( option->bg[0] != 0 )  obj->win.bg = option->bg;
	if( option->cs[0] != 0 )  obj->win.cs = option->cs;
	if( option->sc[0] != 0 )  obj->win.sc = option->sc;
	if( option->mk[0] != 0 )  obj->win.mk = option->mk;
	if( option->dim[0] != 0 ) obj->win.dim = option->dim;
	if( option->mw != 0 )     obj->win.mw = option->mw;
	if( option->mgn != -1 )   obj->win.csmgn = option->mgn;
	if( option->plotmk >= 0 ) obj->win.plotmk = option->plotmk;
	sas_file_open(obj,option->fname);
	for(p=option->mode;*p;p++){
		switch(*p){
		case 'm': sas_change_mode(obj); break;
		case 'M': sas_change_mode2(obj); break;
		case 'p': sas_fzoom_up(obj); break;
		case 'g': sas_gain_up(obj); break;
		case 'i': obj->view.pitch = 1; break;
		case 's': sas_change_xscale(obj); break;
		case 'f': sas_skip_right(obj); break;
		case 'F': sas_view_right(obj); break;
		case 'z': sas_zoom_up(obj); break;
		default: break;
		}
	}
	if(option->vstart>=0){
		if(option->vlen<0) option->vlen = 10000;
	 	sas_set_view(obj, option->vstart, option->vlen);
	}

	/* ここで初めて表示 */
	sas_realize(obj);

	if(option->sstart>=0) {
		sas_sel_start(obj, option->sstart, -1, 0);
		sas_sel_end(obj, option->sstart+option->slen, -1, 0);
	}

	g_nobj++;
	XSync(g_disp,0); /* window manager が早く気づくようにおまじない */
	usleep(50000); /* window manager が枠を表示するのを待つ 50ms */
	/* 早すぎるとwindow manager枠無しのまま次の窓の位置が計算されてしまう */
}

/* 適当に g_obj の空き番号を振る */
int  get_new_id()
{
	int   new;
	for( new=0; new<g_nobj; new++ )
		if( !g_obj[new] )
			break;
	if( new >= MAX_OBJ ){
		fprintf(stderr,"too many objects\n");
		return -1;
	}
	if( new == g_nobj )
		g_nobj++;
	return new;
}

/* 音響分析窓を開ける */
void create_ana_window(int anamode)
{
	int new;
	SasProp *obj;

	/* 始めての窓を開ける時はディスプレイも開ける */
	if( ! g_disp )
		g_disp = XOpenDisplay(option.dispname);

	/* 空き番号を探す */
	if( (new=get_new_id()) < 0 )
		return;

	/* 窓を作る */
	obj = g_obj[new] = ana_create(g_disp, anamode);
	obj->win.id = new;

	/* オプション */
	obj->view.dbmin   = option.dbmin;
	obj->view.dbmax   = option.dbmax;
	if( option.skip > 0 ) obj->view.frameskip = option.skip;
	if( option.size > 0 ) obj->view.framesize = option.size;
	obj->view.anapre   = option.anapre;
	obj->view.analagbw   = option.analagbw;
	obj->view.analpc   = option.analpc;
	obj->view.anawarp   = option.anawarp;
	obj->view.anacep   = option.anacep;
	obj->view.anafbnum = option.anafbnum;
	obj->view.analifter = option.analifter;

	/* 表示 */
	ana_realize(obj);
}

/* 親窓の真下に同じ大きさの窓を開ける */
/* obj が 0 の時は option を使用, 0 以外の時は obj の選択領域 */
void create_slave_window(SasProp *parentObj)
{
	int        new;
	SasProp   *obj;

	/* 空き番号を探す */
	if( (new=get_new_id()) < 0 )
		return;
	
	/* 窓を作る */
	obj = g_obj[new] = sas_create(g_disp);

	/* 属性をセットする */
	memcpy(obj,parentObj,sizeof(SasProp));
	obj->win.id = new;
	obj->win.win = 0;
	obj->win.pix = 0;
	obj->view.cson = 0;
	obj->lbl = 0; /* ラベル構造体だけは空っぽに。後のものはコピーでよい */
	/* 同じオブジェクトを指していてもいいけど一応別オブジェクト */
	/* 変更してるかも知れないので読み込まずにコピー */
	/* ラベルがあればコピー */
	if( parentObj->lbl ){
		sas_copy_label(obj, parentObj);
	}
	get_slave_xy(parentObj, &obj->win.size.x, &obj->win.size.y);
	if( parentObj->file.name[0] )
		sas_file_open(obj,parentObj->file.name);
	if( parentObj->view.nsel <= 0 )
		sas_set_view(obj, parentObj->view.sview, parentObj->view.nview);
	else
		sas_set_view(obj, parentObj->view.ssel, parentObj->view.nsel);
	sas_realize(obj);
	/* 何故かここで描画が行われていない(デバッガ中では描かれる) */
	/* まあ、実害はないのでいいことにする */
}

/* 窓を閉じる */
void close_window(SasProp *obj)
{
	int i;	
	void da(SasProp*);
	for(i=0;i<g_nobj;i++){
		if( g_obj[i] != obj )
			continue;

		/* もしも再生中なら止める */
		if( obj->view.daon ){
			obj->view.b3press = 0;
			da(obj);
		}

		/* 分析表示は消さない */

		/* オブジェクト消去 */
		if( obj->view.mode & MODE_ANALYZE )
			ana_destroy(obj);
		else
			sas_destroy(obj);
		g_obj[i] = (SasProp*)NULL;
		break;
	}
	/* nobj をできるだけ切り詰める */
	for( i=g_nobj; !g_obj[i] && i>=0; i-- )
		g_nobj = i;
	if( g_nobj == 0 ) exit(0);
}


/* -------------------------------------------------------------------------
窓の同期に関する関数郡
*/

/* 全ての窓の表示時刻を指定された窓にそろえる */
void sync_windows(SasProp *obj)
{
	int i, sview, nview;
	for( i=0; i<g_nobj; i++ ){
		if( ! g_obj[i] || g_obj[i] == obj )
			continue;
		if( g_obj[i]->view.mode & MODE_ANALYZE )
			continue;
		if( g_obj[i]->view.nosync )
			continue;
		if( obj->file.freq == g_obj[i]->file.freq ){
			sview = obj->view.sview;
			nview = obj->view.nview;
		}
		else{
			sview = (int)((double)obj->view.sview*g_obj[i]->file.freq/obj->file.freq);
			nview = (int)((double)obj->view.nview*g_obj[i]->file.freq/obj->file.freq);
		}
		sas_set_view(g_obj[i],sview,nview);
	}
}

/* 同じラベルを使っている全ての窓のラベルをそろえる */
/* clbl  は変更されたラベル */
/* clblm は変更された要素   */
/* albl  は追加されたラベル */
/* dlbl  は削除されたラベル */
/* 波形ファイル名付きで作成するので、違うファイルで同じラベルを使うことも可能
   波形が異なるファイルで追加したラベルは表示されないというだけ */
void sync_labels(SasProp *obj)
{
	int i, clbl, clblm;
	char  *p, *str;
	clbl=obj->clbl;
	clblm=obj->clblm;
	for( i=0; i<g_nobj; i++ ){
		if( ! g_obj[i] || g_obj[i] == obj )
			continue; /* ない窓には同期しない。自分自身もスキップ */
		if( strcmp(obj->lblfile,g_obj[i]->lblfile) )
			continue; /* 入力ラベルが異る時は同期しない */
		if( strcmp(obj->lblsave,g_obj[i]->lblsave) )
			continue; /* セーブ先が異る時も同期しない */

		/* 消す */
		if( obj->dlbl >= 0 ){
			sas_label_del(g_obj[i], obj->dlbl);
			sas_redraw(g_obj[i]);
		}

		/* 追加された時 */
		if( obj->albl >= 0 && obj->nlbl > g_obj[i]->nlbl ){
			ins_label(g_obj[i], obj->albl,
			  obj->lbl[obj->albl].start, obj->lbl[obj->albl].end,
			  obj->lbl[obj->albl].prev, obj->lbl[obj->albl].next,
			  obj->lbl[obj->albl].str, obj->lbl[obj->albl].fname);
			/* 選択より前に挿入されたら選択が１個ずれる */
			if( obj->albl <= g_obj[i]->clbl )
				g_obj[i]->clbl ++;
			/* １個めの時はレイアウトが変わるので全描画 */
			if( g_obj[i]->nlbl == 1 )
				g_obj[i]->view.redraw = 1;
			sas_redraw(g_obj[i]);
		}

		/* 変化があったら動かす */
		if( clbl >= 0 && clblm > 0 ){
			/* 移動された */
			sas_draw_label(g_obj[i],clbl-1,clbl+1); /* 消す(判断は中で) */
			g_obj[i]->lbl[clbl].start = obj->lbl[clbl].start;
			g_obj[i]->lbl[clbl].end = obj->lbl[clbl].end;
			if( clbl + 1 < obj->nlbl )
				 g_obj[i]->lbl[clbl+1].start = obj->lbl[clbl+1].start;
			p = g_obj[i]->lbl[clbl].str;
			str = obj->lbl[clbl].str;
			if( strcmp(p, str) ){
				if( !(p=realloc(p,strlen(str)+1)) )
					fprintf(stderr,"can't realloc\n");
				else {
					strcpy(p,str);
					g_obj[i]->lbl[clbl].str = p;
				}
			}
			sas_draw_label(g_obj[i],clbl-1,clbl+1); /* 書く(判断は中で) */
		}
	}
}

/* 全ての窓の選択開始を指定の窓にそろえる */
void sel_start_windows(SasProp *obj)
{
	int i, smpl;
	for( i=0; i<g_nobj; i++ ){
		if( ! g_obj[i] || g_obj[i] == obj )
			continue;
		if( g_obj[i]->view.nosync )
			continue;
		if( g_obj[i]->view.mode & MODE_ANALYZE ){ /* 音響分析窓 */
			g_obj[i]->view.redraw = 1;
			ana_redraw(g_obj[i]);
			continue;
		}
		else if( global_shift  ){ /* 時系列窓 */
			/* シフトが押されていたら選択範囲の同期をとる */
			smpl = obj->view.ssel;
			/* 周波数が違う時、時間を調整する */
			if( obj->file.freq != g_obj[i]->file.freq )
				smpl = (int)((double)smpl*g_obj[i]->file.freq/obj->file.freq);
			sas_sel_start(g_obj[i], smpl, obj->view.sfreq, obj->view.fsel);
		}
	}
}

/* 全ての窓の選択終了を指定の窓にそろえる */
void  sel_end_windows(SasProp *obj)
{
	int i, smpl;
	for( i=0; i<g_nobj; i++ ){
		if( ! g_obj[i] || g_obj[i] == obj )
			continue;
		if( g_obj[i]->view.nosync )
			continue;
		if( g_obj[i]->view.mode & MODE_ANALYZE ){ /* 音響分析窓 */
			g_obj[i]->view.redraw = 1;
			ana_redraw(g_obj[i]);
		}
		else if( global_shift ){          /* 時系列窓 */
			/* シフトが押されていたら選択範囲の同期をとる */
			smpl = obj->view.ssel + obj->view.nsel;
			/* 周波数が違う時、時間を調整する */
			if( obj->file.freq != g_obj[i]->file.freq )
				smpl = (int)((double)smpl*g_obj[i]->file.freq/obj->file.freq);
			sas_sel_end(g_obj[i],smpl, obj->view.efreq, obj->view.fsel);
		}
	}
}

/* 全ての窓のカーソルを指定の窓にそろえて表示 */
/* 音響分析窓にはカーソル位置のスペクトルを表示 */
void  cursor_all_windows(SasProp *obj)
{
	int  i;
		
	for( i=0; i<g_nobj; i++ ){
		if( g_obj[i] == obj ) continue;     /* 自分には出さない */
		if( !g_obj[i] ) continue;           /* ない窓には出さない */

		if( !(obj->view.mode & MODE_ANALYZE) ){ /* 時系列窓 */
			if( ! (g_obj[i]->view.mode & MODE_ANALYZE) ){ /* 分析窓には同期しない */
				/* 他の窓にカーソル */
				if( g_obj[i]->view.cson ){   /* まず消す */
					sas_cross_hair(g_obj[i], g_obj[i]);
					g_obj[i]->view.cson = 0;
				}
				if( obj->view.cson ){   /* 描く */
					sas_cross_hair( g_obj[i], obj );
					g_obj[i]->view.cson = 1;
				}
			} else if( obj->file.type != 'P' ){
				/* 分析窓には分析表示を描換える(view.cson自動) */
				ana_cur_move( g_obj[i], obj );
			}
		}
		else {                              /* 音響分析窓 */
			if( (g_obj[i]->view.mode & MODE_ANALYZE) ){ /* 分析窓だけ同期 */
				/* 他の窓にカーソル */
				if( g_obj[i]->view.cson ){   /* まず消す */
					ana_cross_hair(g_obj[i], g_obj[i]);
					g_obj[i]->view.cson = 0;
				}
				if( obj->view.cson ){   /* 描く */
					ana_cross_hair( g_obj[i], obj );
					g_obj[i]->view.cson = 1;
				}
			}
		}
	} /* g_obj */
}

/*
	イベントループ。イベントをオブジェクトに渡す
	再生関係はここで処理
	(再生はthreadを使ったほうがいいのだが、今はループしている)
*/
void event_loop(Display *disp)
{
	void    da(SasProp*);
	int     dawin=0;	/* 最後にDAしたウィンドウ */
	int     i;
	XEvent  event;

	while(1){

		/* D/A 中は XNextEvent でブロックされずにループ */
		/* thread を使うのが筋なのだろうなあ */
		if( g_nobj>0 && g_obj[dawin] && g_obj[dawin]->view.daon ) {
			if( ! XPending(disp) ){
				da( g_obj[dawin] );
				XFlush(disp);
				continue;
			}
		}

		/* イベントが来たのでイベントをとる */
		XNextEvent(disp,&event);

		/* イベントの起こった窓 g_obj[i] を見つける */
		for( i=0; i<g_nobj; i++ )
			if( g_obj[i] && event.xany.window == g_obj[i]->win.win )
				break;
		if( i==g_nobj ) continue; /* 窓がない */

		/* 窓がやるべき処理をする */
		if( g_obj[i]->view.mode & MODE_ANALYZE ){ /* 音響分析窓 */
			ana_dispatch(g_obj[i], &event);
			if( !g_obj[i] ) continue;             /* ウィンドウ消えた時 */
		}
		else {                                    /* 時系列窓 */
			sas_dispatch(g_obj[i],&event);
			if( !g_obj[i] ) continue;             /* ウィンドウ消えた時 */

			/* D/A コマンド(MB3)が来たかどうか、ここでチェック */
			da( g_obj[i] );	/* ここで daon が変化する */
			if( g_obj[i]->view.daon )
				dawin = i;
		}

		/* カーソルをそろえる */
		cursor_all_windows(g_obj[i]);
	} /* while(1) */
}


/* メインといっても、ほとんどオプション解釈 */
int  main(int argc, char **argv)
{
	int          optkey;
	int          optind;
	char        *optarg, *p;

	initialize_option();

	/* getopt() はオプションとファイル名の混在ができないので、
	   自分で解釈することにしました ver1.2 */
	for( optind=1; optind<argc; optind++ ){
		if( !strcmp(argv[optind],"-display") ){
			option.dispname = argv[++optind];
			continue;
		}
		if( argv[optind][0] == '-' ){
			optkey =  argv[optind][1]; /* - だけなら optkey = '\0' */
			optarg = &argv[optind][2]; /* arg なければ optarg = "" */
			/* 引数を必要とするオプションSWのチェック */
			if( ! *optarg && strchr("FVLCMSNWPltecfvsygmnfdb",optkey) ){
				if( optind < argc-1 )
					optarg = argv[++optind];
				else {
					fprintf(stderr,"** no arg for -%c option. -h to see usage\n",optkey);
					exit(1);
				}
			}
		} else {
			optkey = -1; /* - で始まらないのはファイル名だ */
			optarg = argv[optind];
		}

		switch(optkey){
		case 'C': if( (p=strchr(optarg,'=')) ) *p++=0;
			if(!strcmp(optarg,"fg"))  {option.fg  = p; break;}
			if(!strcmp(optarg,"bg"))  {option.bg  = p; break;}
			if(!strcmp(optarg,"cs"))  {option.cs  = p; break;}
			if(!strcmp(optarg,"sc"))  {option.sc  = p; break;}
			if(!strcmp(optarg,"mk"))  {option.mk  = p; break;}
			if(!strcmp(optarg,"dim")) {option.dim = p; break;}
			if(!strcmp(optarg,"mw"))  {option.mw = atoi(p); break;}
			if(!strcmp(optarg,"mgn")) {option.mgn = atoi(p); break;}
			if(!strcmp(optarg,"font"))   {option.font   = p; break;}
			if(!strcmp(optarg,"font16")) {option.font16 = p; break;}
			if(!strcmp(optarg,"pm")) {option.plotmk = atoi(p); break;}
			fprintf(stderr," -C%s is not supported.\n",optarg);
			break;
		case 'L': if( (p=strchr(optarg,'=')) ) *p++=0;
			if(!strcmp(optarg,"save")) {option.lsave = p; break;}
			if(!strcmp(optarg,"std"))  {option.std = p?atoi(p):1; break;}
			if(!strcmp(optarg,"mode")) {option.lblmode = p?atoi(p):1; break;}
			if(!strcmp(optarg,"arc")) {option.lblarc = p?atoi(p):1; break;}
			if(!strcmp(optarg,"type")) {option.lbltype = p?atoi(p):0; break;}
			fprintf(stderr," -L%s is not supported.\n",optarg);
			break;
		case 'A':
			if( (p=strchr(optarg,'=')) ) *p++=0;
			if(!strcmp(optarg,"pre")) {option.anapre = p?atof(p):0; break;}
			if(!strcmp(optarg,"lag")) {option.analagbw = p?atof(p):0; break;}
			if(!strcmp(optarg,"lpc")) {option.analpc = p?atoi(p):0; break;}
			if(!strcmp(optarg,"warp")) {option.anawarp = p?atof(p):0; break;}
			if(!strcmp(optarg,"cep")) {option.anacep = p?atoi(p):0; break;}
			if(!strcmp(optarg,"fbnum")) {option.anafbnum = p?atoi(p):0; break;}
			if(!strcmp(optarg,"lifter")) {option.analifter = p?atoi(p):0; break;}
			fprintf(stderr,"-A%s is not supported.\n",optarg);
			break;
		case 'M': option.mode   = optarg; break;
		case 'F': option.font   = optarg; break;	/* backward compatibility */
		case 'S':
			/*sscanf(optarg,"%f,%f",&option.spow,&option.sgain); break;*/
			if( (p=strchr(optarg,'=')) ) *p++=0;
			if(!strcmp(optarg,"spow")) {option.spow = atof(p); break;}
			if(!strcmp(optarg,"sgain")) {option.sgain = atof(p); break;}
			if(!strcmp(optarg,"fmax")) {option.smax = atof(p); break;}
			if(!strcmp(optarg,"color")) {option.grayscale = atoi(p); break;}
			if(!strcmp(optarg,"power")) {option.power = atoi(p); break;}
			if(!strcmp(optarg,"fscale")) {option.fscale = atoi(p); break;}
			if(!strcmp(optarg,"pre")) {option.pre = atof(p); break;}
			fprintf(stderr,"-S%s is not supported.\n",optarg);
			break;
		case 'W': sscanf(optarg,"%f,%f",&option.wbsize,&option.wbskip); break;
		case 'N': sscanf(optarg,"%f,%f",&option.nbsize,&option.nbskip); break;
		case 'V': sscanf(optarg,"%f,%f",&option.vskip,&option.vskips); break;
		case 'l': option.lfile  = optarg; break;
		case 't': option.type   = toupper(optarg[0]); break;
		case 'e': option.endian = toupper(optarg[0]); break;
		case 'c': option.chan   = atoi(optarg); break;
		case 'f': option.freq   = atof(optarg); break;
		case 'v': option.vstart  = atoi(optarg);
		          option.vlen    = atoi(strchr(optarg,',')+1); break;
		case 's': option.sstart  = atoi(optarg);
		          option.slen    = atoi(strchr(optarg,',')+1); break;
		case 'y': option.ymin    = atof(optarg);
		          option.ymax    = atof(strchr(optarg,',')+1); break;
		case 'd': option.dbmin    = atof(optarg);
		          option.dbmax    = atof(strchr(optarg,',')+1); break;
		case 'g': option.geom    = optarg;
		          option.slave   = 0;
		          break;
		case 'b': option.bdskip = atoi(optarg); break;
		case 'm': option.skip = atof(optarg); break;
		case 'n': option.size = atof(optarg); break; /* 00/4/10 */
		case 'h': usage(stdout); exit(0); break;
		case 'a': create_ana_window(MODE_FFTSPECT); break;
		case 'x': option.specdiff = 1; break;
		case  -1: option.fname = optarg;
			  create_sas_window(&option);  /* 窓を作ってしまう */
			  option.slave = 1;
			  break;
		default:
			usage(stderr); exit(1); break;
		}

	}
	if( g_nobj == 0 ) {
		usage(stderr);
		exit(1);
	}

	event_loop(g_disp);
	return 0;
}

