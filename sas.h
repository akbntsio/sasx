#ifndef SAS_H
#define SAS_H

#ifdef ANA_THREAD
#include <pthread.h>
#endif /* ANA_THREAD */

/* ウィンドウ最小サイズ */
#define SAS_MIN_WIDTH   100
#define SAS_MIN_HEIGHT  50

/* 窓のモード */
#define MODE_WAVE            0
#define MODE_SPECTRO         1
#define MODE_MFCC            2

#define MODE_ANALYZE         256
#define MODE_FFTSPECT       (MODE_ANALYZE | 1)
#define MODE_LPCSPECT       (MODE_ANALYZE | 2)
#define MODE_LPCCEPSPECT    (MODE_ANALYZE | 4)
#define MODE_LPCMELSPECT    (MODE_ANALYZE | 8)
#define MODE_LPCMELCEPSPECT (MODE_ANALYZE | 16)
#define MODE_MFCCSPECT      (MODE_ANALYZE | 32)
#define MODE_MFCCSPECT2     (MODE_ANALYZE | 64)

/* 時間スケールモード */
#define	SCALE_TIME      1
#define SCALE_HMS       2
#define	SCALE_SAMPLE    3
#define	SCALE_FRAME     4
#define SCALE_NO        5

/* カーソルモード */
#define CS_NUM          6
#define CS_NORMAL       0
#define CS_LR           1
#define CS_UD           2
#define CS_TEXT         3
#define CS_HAND         4
#define CS_ARROW        5

/* ラベルタイプ */
#define LBL_AUTO        0
#define LBL_SEG         1
#define LBL_ATR         2
#define LBL_CVI         3

/* 各種上限 */
#define MAX_NCHAN     256
#define MAX_ANACH      64

#define MAX_AMP     (1.0e12)

/*  SAS オブジェクトの構造 */

/* =================================================================
  表示ファイルの属性
================================================================= */
typedef struct file_property {
	char         name[256];		/* filename string */
	int          type;		/* A(auto),C,S(short),L,F,D,P(plot) */
	int          unit;		/* 1,2,4,4,8 */
	int          endian;		/* B or L */
	int          chan;		/* channels */
	int          freq;		/* sampling frequency Hz */
	int          size;		/* file size (sample) */
	char         filter[1024];	/* filter command */
	int          stat;		/* 0:non 1:new 2:open */
	int          hsize;		/* header size (byte) */
	int          offset;    /* offset samples */
} FileProp;

/* =================================================================
  ウィンドウの属性
================================================================= */
/* ウィンドウ自身の属性から引っ張り出せるが、変更する時など何かと役にたつ */
typedef struct win_property {
	int          id;        /* window idendifier */
	char         name[512]; /* window name */
	Display     *disp;      /* display */
	int          screen;    /* screen */
	Window       parent;    /* parent */
	Window       win;       /* window ( realize したら != 0 )*/
	XSizeHints   size;      /* x,y,width,height */
	int          bw;		/* border width */
	int          mw;		/* marker width */
	GC           fggc,		/* foreground */
	             bggc,		/* background */
	             csgc,		/* cursor */
	             scgc,		/* scale */
	             grgc,		/* grid */
	             mkgc,		/* marker */
	             mkgc16,		/* marker kanji */
	             smkgc,		/* solid marker */
	             dimgc;		/* dimmed color */
	KeySym       keysym;		/* keysym */
	Colormap     cmap;		/* color map */
	char         *fg,		/* foreground color name */
	             *bg,		/* background color name */
	             *bd,		/* border color name */
	             *cs,		/* corsor color name */
	             *sc,		/* scale color name */
	             *mk,		/* marker color name */
	             *dim;		/* dimmed color name */
	char         fontname[256];	/* font name */
	char         fontname16[256];	/* font name */
	Font         font;
	Font         font16;
	XFontStruct *font_struct;
	XFontStruct *font_struct16;
	XCharStruct  char_struct;
	XCharStruct  char_struct16;
	int          fonth,
	             fonta,
	             fontd;
	int          fonth16,
	             fonta16,
	             fontd16;
	Pixmap       pix;
	int          scaleh;
	int          scalew;
	int          lblh;
	Cursor       csfont[CS_NUM];    /* 0:gumby 1:sb_h_double_arrow */
	int          csmode;            /* CS_NORMAL CS_LR CS_UD CS_TEXT */
	int          csmgn;	            /* label segment grab margin */
	int          plotmk;            /* plot marker */
} WinProp;

/* =================================================================
  表示形式の属性
================================================================= */
typedef struct view_property {
	/* 各種モード */
	int          cpuendian;     /* CPU endian B or L */
	int          mode;          /* WAVE,SPECTRO,ANALYZE */
	int          xscale;         /* TIME,SAMPLE,FRAME */
	float        frameskip;     /* 分析スキップ(ms) */
	float        framesize;     /* 分析窓(ms) */
	int          nosync;        /* 他の窓に同期しない */

	/* 波形表示領域 */
	int          sview, nview;  /* view sample (start, len) */
	int          redraw;        /* redraw request */
	float        spp;           /* samples pre pixel */
	int          csbias;        /* cursor position of the first sample */
	float        ymin,          /* display magnitude min */
	             ymax;          /* display magnitude max */
	float        vskip;         /* (%) skip bounded to key f,b */
	float        vskips;        /* (%) skip bounded to key F,B */

	/* カーソル */
	int          ox, oy;        /* mouse x,y at the pressing */
	int          mx, my;        /* mouse x,y */
	int          csx, csy;      /* cursor x,y (tacked onto sample) */
	int          cson;          /* cursor on */
	int          cssmpl;        /* cursor tacking sample */
	float        csamp;         /* cursor position amplitude */
	float        csfreq;        /* cursor position frequency */
	int          csch;          /* cursor position chan */
	int          b1press, b2press, b3press;    /* pressed */
	int          b1sel;         /* pressed for selection */
	int          b1mot, b2mot, b3mot;         /* pressed and moved */

	/* 選択 */
	int          sfrom, sto;    /* select from select to (window pos) */
	int          ssel, nsel;    /* select sample (start, len) */
	int          sfreq, efreq;  /* select freq (start end) */
	int          ffrom, fto;    /* select freq (window pos) */
	int          fsel;          /* select freq or not */

	/* 再生 */
	int          daloop;        /* loop playback */
	int          daon;          /* 1 when D/A progam active */
	int          marker;        /* marker sample position */
	char         markerstr[32]; /* "PLAY" string or nothing */

	/* スペクトログラム */
	float        frameskipwb;   /* ms/frame */
	float        framesizewb;   /* ms/frame */
	float        spow;          /* spectrogram pow() factor */
	float        sgain;         /* spectrogram gain  factor */
	float        smax;          /* spectrogram max freq */
	float        pre;           /* pre-emphasis */
	float        fzoom;         /* frequency zoom */
	int          narrow;        /* narrow band */
	float        frameskipnb;   /* ms/frame narrow band */
	float        framesizenb;   /* ms/frame narrow band */
	int          grayscale;     /* use grayscale */
	int          specdiff;      /* show timediff */

	/* ピッチ */
	int          pitch;         /* show pitch 1:on 0:no */
	int          pitchsft;      /* frame shift ms */
	float        pitchroot;     /* root for calc power spectrum */
	int          pitchup;       /* IFFT oversample rate */
	int          pitchmin;      /* Hz */
	int          pitchmax;      /* Hz */
	float        pitchthr;      /* c[p]/c[0] threshold */
	char         pitchfile[256]; /* 入力 */
	char         pitchsave[256]; /* 出力 */
	float       *pitchbuff;     /* 配列 */
	int          pitchnum;     /* ピッチ数 */

	/* パワー */
	int          power;         /* show power */
	int          powerh;        /* graph height */

	/* 音響分析窓の設定 */
	enum {FSC_LINEAR,FSC_MEL,FSC_LOG}
	             fscale;        /* 周波数スケール */
	int          anapow;        /* パワー表示するか */
	float        dbmin, dbmax;  /* 縦の範囲 */
	float        fmin;          /* 最低サンプリング周波数 */
	float        fmax;          /* 最高サンプリング周波数/2 */
	float        lfmin;          /* log用最低サンプリング周波数 */
	float        lfmax;          /* log用最高サンプリング周波数 */
	float        lmin;          /* 最低サンプリングlog周波数 */
	float        lmax;          /* 最高サンプリングlog周波数 */
	float        mmin;          /* 最低サンプリングMel周波数 */
	float        mmax;          /* 最高サンプリングMel周波数 */
	float        anapre;        /* 分析用プリエンファシス係数 */
	int          anapref;       /* プリエンファシスフラグ 1 で掛ける */
	float        analagbw;      /* LPC 分析lag窓バンド幅0.01 */
	int          analpc;        /* LPC 次数 */
	float        anawarp;       /* 周波数ワープ係数 */
	int          anacep;        /* CEP 次数 */
	int          anafbnum;      /* MFCC フィルタバンク数 */
	int          analifter;     /* MFCC リフタ長 */
	int          single;        /* 一発分析をするモード */
	enum {SQUARE,HAMMING} anawin;        /* 分析窓タイプ */
	int          powmode;        /* POWER計算方法(波形パワー,スペクトル調整) */
	int          mfcctilt;        /* MFCC の fbank の傾き補正 */
	int          fftmin;        /* 表示用FFTの最小値 */

	/* グラフ */
	int          nline;         /* 分析表示グラフ数 */

	/* カーソル位置分析 */
	int          *coordx;       /* y座標の記憶 */
	int          *coordy;       /* y座標の記憶 */
	int          coordsize;     /* 記憶領域サイズ */
	int          anafreq;       /* 分析周波数 */
	int          anach;         /* 分析ライン総数 */
	short        anasize[MAX_ANACH];   /* 分析プロット数[ch] */
	int          anamode[MAX_ANACH];   /* 分析方法[ch] */
	int          anaon;         /* カーソル分析表示ON */
	int          *coordpow;     /* 現在のパワー値[ch] */
	int          curch;         /* 現在ファイルのチャンネル数 */


	/* 表示範囲変更 */
	enum {DR_NONE,DR_MOVE,DR_HMOVE,DR_VMOVE,DR_ZOOM,DR_HZOOM,DR_VZOOM}
	             dragmode;      /* 移動か拡大か */
#ifdef ANA_THREAD
	int          tstate;        /* thread 状態 */
	int          tquit;         /* thread 中断 */
	pthread_t     thread;        /* blackbox */
#endif /* ANA_THREAD */

} ViewProp;

/* =================================================================
  ラベルの属性
================================================================= */
typedef struct label_poperty {
	float       start, end;
	float       prev, next;
	char        *fname, *str;
} LABEL;
        
/* =================================================================
  プロットの属性 使っていない(その都度ファイルから読んでいる)
================================================================= */
typedef struct plot_property {
	char        *legText;
	int	        size;
	float       (*cood)[2];
} PLOT;

/* =================================================================
  音響分析対象の属性
================================================================= */
typedef struct ana_property {
	FileProp     file;              /* ファイル */
	int          start, len;        /* 分析範囲 */
	float       (*coord)[2];        /* グラフ記憶 */
} AnaProp;

/* =================================================================
  SAS全体の属性
================================================================= */
typedef struct sas_property {
	FileProp     file;        /* ファイル属性 */
	WinProp      win;         /* 窓の属性 */
	ViewProp     view;        /* 表示関係 */
//	int          nline;       /* 分析ライン数 */
//	AnaProp      *ana;        /* 分析対象数 */
	/* ラベル情報 */
	char         lblfile[256]; /* 入力 */
	char         lblsave[256]; /* 出力 */
	int          lbltype;      /* 0:AUTO 1:SEG 2:ATR 3:CVI */
	int          lblmode;      /* 0:区間とセット 1:詰めたりする */
	int          lblarc;       /* 区間補助線を引く */
	int          nlbl;	/* ラベル数 */
	LABEL       *lbl;	/* ラベル構造体 */
	int          clbl;	/* 選択中のラベル */
	int          clblm;	/* 選択中のメンバ    1:始点 2:終点 3:終点と次始点 4:ラベル */
	int          mlbl;	/* ラベル編集モード  1:始点 2:終点 3:終点と次始点 4:ラベル */
	float        ulbl;	/* アンドゥーバッファ */
	int          slbl;	/* 分割されたラベル番号 普段は -1 */
	int          albl;	/* 追加されたラベル番号 普段は -1 */
	int          dlbl;	/* 削除されたラベル番号 普段は -1 */
	int          std;	/* ラベルを標準入力から変更 */
} SasProp;


/* =================================================================
  メタキーは窓の制限を受けないのでグローバル変数にとる
================================================================= */
extern   int      global_shift;
extern   int      global_ctrl;
extern   int      global_meta;
extern   int      global_alt;

/* =================================================================
  関数プロトタイプ宣言
================================================================= */
/*
   create() -> realize() -> size change -> file_open() -> set view()
とやると、空の表示をしてから何度も」書き換えるので
   create() -> size change -> file_open() -> set view() -> realize()
とやっていきなり目的の絵を描くほうが表示が早い
*/

/* Window Control */
extern void* sas_create(Display* disp);     /* 新しい SasProp* を作る */
extern int   sas_realize(SasProp* obj);     /* 表示する */
extern void  sas_destroy(SasProp* obj);     /* 消す */
extern void  sas_parse_geometry(SasProp* obj, char* geom);     /* サイズ */
extern void  sas_move_resize(SasProp* obj, int x, int y, unsigned int w, unsigned int h);
extern void  sas_move(SasProp* obj, int x, int y);
extern void  sas_resize(SasProp* obj, int w, int h);
extern int   sas_color(SasProp* obj, char *colorname);

/* File Command */
extern int   sas_file_open(SasProp* obj, char* fname);
extern int   sas_file_close(SasProp* obj);
extern int   filesize(char* fname);
extern int   cpuendian();
extern void  swapbyte(int size, int unit, char* buff);
extern void  sas_set_title(SasProp* obj);
extern void  sas_calc_spp(SasProp* obj);

/* not implemented
extern int   sas_freq(SasProp* obj, int freq);
extern int   sas_endian(SasProp* obj, int endian);
extern int   sas_chan(SasProp *obj, int chan);
extern int   sas_type(SasProp *obj, int type);
extern int   sas_mode(SasProp *obj, int mode);
*/

/* View Commands */
extern int   sas_set_view(SasProp* obj, int start, int len);
extern void  sas_zoom_up(SasProp* obj);
extern void  sas_zoom_down(SasProp* obj);
extern void  sas_view_all(SasProp* obj);
extern void  sas_view_right(SasProp* obj);
extern void  sas_view_left(SasProp* obj);
extern void  sas_skip_right(SasProp* obj);
extern void  sas_skip_left(SasProp* obj);
extern void  sas_gain_up(SasProp* obj);
extern void  sas_gain_down(SasProp* obj);
extern void  sas_change_mode(SasProp* obj);
extern void  sas_change_mode2(SasProp* obj);
extern void  sas_fzoom_up(SasProp* obj);
extern void  sas_fzoom_down(SasProp* obj);
extern void  sas_change_xscale(SasProp* obj);
extern void  sas_change_fscale(SasProp* obj);

/* general drawing */
extern int   sas_check_resize(SasProp* obj);
extern void  sas_redraw(SasProp* obj);
extern void  sas_draw_scroll(SasProp* obj);
extern void  sas_draw_tags(SasProp* obj);
extern int   sas_draw_wave(SasProp* obj);
extern void  sas_clear_win(SasProp* obj);
extern void  sas_draw_scale(SasProp* obj);

/* draw cursor and selection */
extern int   sas_x_to_sample(SasProp *obj, int x);
extern void  sas_cross_hair(SasProp *obj, SasProp *master);
extern void  sas_move_cursor(SasProp *obj, int x, int y);
extern void  sas_fill_rect(SasProp *obj, int x1, int y1, int x2, int y2);
extern void  sas_fill_area(SasProp *obj);
extern void  sas_sel_start(SasProp *obj, int smpl, int freq, int fsel);
extern void  sas_sel_end(SasProp *obj, int smpl, int freq, int fsel);
extern void  sas_marker(SasProp *obj, int smpl);
extern void  sas_markerstr(SasProp *obj, char *str);
extern void  sas_change_cursor(SasProp *obj, int type);

/* X Event Handler */
extern void   sas_dispatch(SasProp *obj, XEvent *event);
extern void   sas_skip_motions(XEvent *event);

/* analyze window */
extern void  *ana_create(Display *disp, int anamode);
extern void  ana_realize(SasProp *obj);
extern void  ana_destroy(SasProp* obj);     /* 消す */
extern void  ana_dispatch(SasProp *obj, XEvent *event);

extern int   ana_check_resize(SasProp *obj);  
extern void  ana_clear_win(SasProp *obj);  
extern void  ana_draw_scale(SasProp *obj);  
extern void  ana_draw_fftspect(SasProp *obj, GC gc, SasProp *src);  
extern void  ana_draw_lpcspect(SasProp *obj, GC gc, SasProp *src, float warp, int docep);  
extern void  ana_draw_spect(SasProp *obj);
extern void  ana_redraw(SasProp *obj);
extern void  ana_cross_hair(SasProp *obj, SasProp *cur);
extern void  ana_move_cursor(SasProp *obj, int x, int y);
extern void  ana_cur_move(SasProp *obj, SasProp *cur);
extern void  ana_cur_make(SasProp *obj, SasProp *cur);
extern void  ana_cur_draw(SasProp *obj);
extern void  ana_draw_condition(SasProp *obj);

extern void  warp_alpha(double *alpha, int order, int order2, float warpcoef);
extern void  autocor(double *wav, double *cor, int wsize, int order);
extern void  lagwin(double *cor, int order, float bw);
extern double  coralf(int order, double *cor, double *alf);
extern void  alfcep(double *alf, int aorder, double *cep, int corder);
extern void  sas_calc_tic(float range, int width, int maxpix, int mintics, int *tic, int *tic10, int *ticlb);
extern void  sas_calc_tic_real(float range, int width, int minpix, int *tic, int *inex10);
extern void  pre_emphasis(float coef, double *buf, int len);

#endif /* SAS_H */
