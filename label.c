#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include "sas.h"
#include "label.h"

/* 文字の変更 */
void edit_labelstr(SasProp *obj, int posi, char *str)
{
	char *p;
	if( ! (p=realloc(obj->lbl[posi].str,strlen(str)+1)) ){
		fprintf(stderr,"can't realloc\n");
	} else {
		strcpy(p,str);
		obj->lbl[posi].str = p;
	}
}

/* ラベルを１区間追加する （メモリを動的に拡張する） */
int add_label(SasProp *obj, int posi, float st, float en, float prev, float next, char *str, char *fname)
{
	int i, strposi;
	char   localstr[32];
	if( !obj->lbl || obj->nlbl == 0 ){
		if( ! (obj->lbl=(LABEL*)malloc(sizeof(LABEL))) ){
			fprintf(stderr,"can't alloc lbl\n");
			return 0;
		}
		obj->nlbl = 0;
	}
	else {
		if( ! (obj->lbl=(LABEL*)realloc((char *)obj->lbl,sizeof(LABEL)*(obj->nlbl+1))) ){
			fprintf(stderr,"can't alloc lbl[%d]\n",obj->nlbl);
			return obj->nlbl;
		}
	}

	/* 挿入の場合はシフトする */
	for( i=obj->nlbl; i>posi; i-- ){
		obj->lbl[i].start = obj->lbl[i-1].start;
		obj->lbl[i].end   = obj->lbl[i-1].end;
		obj->lbl[i].prev  = obj->lbl[i-1].prev;
		obj->lbl[i].next  = obj->lbl[i-1].next;
		if( obj->lblmode != 1 )
			obj->lbl[i].str   = obj->lbl[i-1].str; /* 区間と一緒に動かす */
		obj->lbl[i].fname = obj->lbl[i-1].fname;
	}

	/* 追加する */
	obj->lbl[posi].start = st;
	obj->lbl[posi].end = en;
	obj->lbl[posi].prev = prev;
	obj->lbl[posi].next = next;

	/* 文字列生成場所 */
	if( obj->lblmode == 1 )
		strposi = obj->nlbl;
	else strposi = posi;

	/* 文字列が空の時は、文字列の内容 */
	if( !str || !*str ){
		sprintf(localstr, "#%d", strposi+1);
		str = localstr;
	}

	/* 文字列生成 */
	if( (obj->lbl[strposi].str=(char *)malloc(strlen(str)+1)) ){
		strcpy(obj->lbl[strposi].str,str);
	}else{
		fprintf(stderr,"can't alloc lbl[%d].str\n",strposi);
	}

	/* ファイル名情報生成 */
	if( !fname || !*fname )
		fname = obj->file.name;
	if( fname && *fname ){
		if( (obj->lbl[posi].fname=(char *)malloc(strlen(fname)+1)) ){
			strcpy(obj->lbl[posi].fname,fname);
		}else{
			fprintf(stderr,"can't alloc lbl[%d].fname\n",posi);
		}
	}else{
		obj->lbl[posi].fname = 0;
	}
	obj->nlbl ++;  /* ここで数を変更 */
	return obj->nlbl;
}

/* ラベルを分割する */
int ins_label(SasProp *obj, int posi, float st, float en, float prev, float next, char *str, char *fname)
{
	char localstr[32];
	int  strposi;

	/* 文字列がない時と、lblmode=1 の時は文字列生成 */
	if( obj->lblmode == 1 || !str || !*str ){
		/* lblmode=1 なら文字列自体は最後に追加するので */
		if( obj->lblmode == 1 ) strposi = obj->nlbl;
		else                    strposi = posi;
		sprintf(localstr, "#%d", strposi+1); /* #1 から始まる */
		str = localstr;
	}

	return add_label(obj, posi, st, en, prev, next, str, fname);
}

/* ラベルを削除する */
int  del_label(SasProp *obj, int posi)
{
	int strposi;
	if( posi < 0 || posi >= obj->nlbl ) return obj->nlbl;

	if( obj->lblmode == 1 ) strposi = obj->nlbl-1;
	else strposi = posi;
	if( obj->lbl[strposi].str ) free( obj->lbl[strposi].str );

	if( obj->lbl[posi].fname ) free( obj->lbl[posi].fname );

	obj->nlbl--;
	for( ; posi<obj->nlbl; posi++ ){
		obj->lbl[posi].start = obj->lbl[posi+1].start;
		obj->lbl[posi].end   = obj->lbl[posi+1].end;
		obj->lbl[posi].prev  = obj->lbl[posi+1].prev;
		obj->lbl[posi].next  = obj->lbl[posi+1].next;
		if( obj->lblmode != 1 )
			obj->lbl[posi].str   = obj->lbl[posi+1].str; /* 区間と一緒にシフト */
		obj->lbl[posi].fname = obj->lbl[posi+1].fname;
	}
	return obj->nlbl;
}


/* ラベル情報をまるごとコピーする。構造体を作りながら。 */
/* src → obj */
void sas_copy_label(SasProp *obj, SasProp *src)
{
	int i, size;
	for( i=size=0; i<src->nlbl; i++ ){	/* 最後のおまけも */
		size = add_label(obj, size,
			src->lbl[i].start, src->lbl[i].end,
			src->lbl[i].prev, src->lbl[i].next,
			src->lbl[i].str, src->lbl[i].fname);
	}
/* なくてもコピーされる.
	obj->nlbl = size;
	strcpy(obj->lblfile, src->lblfile);
	strcpy(obj->lblsave, src->lblsave);
*/
}

/* 単語が数字なら１ */
int is_number(char *p)
{
	while( *p ){
		if( ! strchr("0123456789.",*p) )
			return 0;
		p ++;
	}
	return 1;
}

/* リストファイルを読んで サイズを返す */
/* ラベルファイルを読んで サイズを返す */
/* 構造は同じ。使うフィールドが異る */
int sas_read_label(SasProp *obj, char *fname)
{
	FILE   *fp;
	char    buff[1024], str[256], path[256];
	float	st, en, prev, next;
	int     n, a, b;

	if( ! (fp=fopen(fname,"r")) ){
		fprintf(stderr,"sas_read_label: %s open error\n",fname);
		return(0);
	}
	obj->nlbl = 0;
	obj->lbl = NULL;

	while( fgets(buff,1024,fp) ){
		*path = *str = '\0';
		prev = next = -1;

		/* 最初の単語を取り出してみる */
		if( (n=sscanf(buff,"%s",str)) == 0 )
			continue;

		/* ATR で # が来たら終る( ただし最初からの時は待ってみる ) */
		if( obj->lbltype == LBL_ATR && buff[0] == '#' && obj->nlbl > 0 ){
			break;
		}

		/* 最初が数字なら、まず ATR でチェックする */
		/* 04abc.wav なんてのがファイル名でも数字と思ってしまわないように */
		if( is_number(str) &&
		    (obj->lbltype == LBL_AUTO || obj->lbltype == LBL_ATR) ){

			/* ATR ラベルかな？ */
			n = sscanf(buff,"%f %s %f",&st,str,&en);
			if( n == 3 ){ /* ATR ラベルでしょう */
				obj->lbltype = LBL_ATR;
				add_label(obj, obj->nlbl, st, en, prev, next, str, path);
				continue;
			}
			if( n == 2 ){ /* ポイントだけのラベルかもね */
				obj->lbltype = LBL_ATR;
				add_label(obj, obj->nlbl, st, st, prev, next, str, path);
				continue;
			}
			if( n == 1 ){ /* 時刻だけなんて、なんかようわからない */
				continue;
			}
			/* n == 0 の時は次の判断に任せる */
		}

		/* 最初はファイル名だと思うので CVI か、SEG だ */
		if( obj->lbltype != LBL_ATR ){

			/* 一番フィールドの多い目的地フォーマットでテスト */
			n = sscanf(buff,"%s %f %f %d %d %f %f %s\n",
			 	path,&st,&en,&a,&b,&prev,&next,str);
			if( n < 0 ) continue;
			if( n == 1 && !strcmp(path,"#") ){
				if(obj->nlbl==0) continue;	/* いきなり # は読みとばす */
				else  break;
			}
			if( n == 2 ){
				continue; /* 理解できないのでスキップ */
			}
			if( n <= 4 || obj->lbltype == LBL_SEG ){
				/* 4 つめが数字でも、それ以後なければ単語名と考える SEG */
				obj->lbltype = LBL_SEG;
				/* 4つめは単語名かな */
				*str = '\0';
				n = sscanf(buff,"%s %f %f %s",path, &st, &en, str);
				add_label(obj, obj->nlbl, st, en, prev, next, str, path);
				/* str が読めんかったら中で文字列生成する */
				continue;
			}
//			if( n < 8 ){
//				/* わからんけどとりあえず番号つけとこ */
//				sprintf(str,"(%d)",obj->nlbl+1);
//			}
			obj->lbltype = LBL_CVI;
			add_label(obj, obj->nlbl, st, en, prev, next, str, path);
		} /* not ATR */
	}
	fclose(fp);
	strcpy(obj->lblfile,fname);
	return obj->nlbl;
}

/* ラベルをファイルにセーブする */
void sas_write_label(SasProp *obj)
{
	FILE *fp;
	int i, type=0; /* 0:list 1:atr */
	float prev,next;

	if( ! obj->lblsave[0] ){
		return;
	}
	if( ! (fp=fopen(obj->lblsave,"w")) ){
		fprintf(stderr,"can't open %s for write\n",obj->lblsave);
	}
	for( i=0; i<obj->nlbl; i++ ){
		if( obj->lbltype != LBL_ATR ){ /* ファイル名付きリスト */
			if( obj->lbltype == LBL_SEG ) {
				/* スポット用 */
				/* ファイル スタート エンド ラベル */
				fprintf(fp,"%s %6.0f %6.0f %s\n", obj->lbl[i].fname,
					obj->lbl[i].start, obj->lbl[i].end, obj->lbl[i].str);
			} else {
				/* 以前のナビ用 */
				/* ファイル スタート エンド フラグx2 限界x2 ラベル */
				if( i>0 && obj->lbl[i-1].fname[0]
			 	&& !strcmp(obj->lbl[i-1].fname,obj->lbl[i].fname) )
					prev = (obj->lbl[i-1].end);
				else	prev = obj->lbl[i].prev;
				if( i<obj->nlbl-1 && obj->lbl[i+1].fname[0]
			 	&& !strcmp(obj->lbl[i].fname,obj->lbl[i+1].fname) )
					next = obj->lbl[i+1].start;
				else    next = obj->lbl[i].next;
				fprintf(fp,"%s %6.0f %6.0f %d %d %6.0f %6.0f %s\n",
					obj->lbl[i].fname, obj->lbl[i].start, obj->lbl[i].end,
					0, i, prev, next, obj->lbl[i].str);
			}
			type = 0;
		} else {
			/* ATRラベル */
			fprintf(fp,"%8.2f %-8s %8.2f\n",
			 obj->lbl[i].start, obj->lbl[i].str, obj->lbl[i].end);
			type = 1;
		}
	}
	if( type == 1 ) /* ATR の終り */
		fprintf(fp,"#\n");
	fclose(fp);
}

/* ＥＵＣコードのＭＳＢを０にする */
void str8to7(char *str1,char *str2)
{
	while(*str2)
		*str1++ = (*str2++)&0x7f;
	*str1++ = 0;
	*str1 = 0;
}

/* ラベルの文字列を１個描く */
void draw_label_string(SasProp *obj, int x, int y, char *str)
{
	char *p, tmp[3];
	for( p=str; *p; ){
		if( (*p)&0x80 && (*(p+1))&0x80 ){
			tmp[0] = (*p) & 0x7f;
			tmp[1] = (*(p+1)) & 0x7f;
			XDrawString16(obj->win.disp, obj->win.win, obj->win.mkgc16,
				x, y, (XChar2b*)tmp, 1);
			x += XTextWidth16(obj->win.font_struct16, (XChar2b*)tmp, 1);
			p+=2;
		} else {
			tmp[0] = (*p) & 0x7f;
			XDrawString(obj->win.disp, obj->win.win, obj->win.mkgc,
				x, y, tmp, 1);
			x += XTextWidth(obj->win.font_struct, tmp, 1);
			p ++;
		}
	}
}

/* 文字列の横幅を計算する */
int text_width(SasProp *obj, char *str)
{
	char *p, tmp[3];
	int  wid = 0;
	for( p=str; *p; ){
		if( (*p)&0x80 && (*(p+1))&0x80 ){
			tmp[0] = (*p) & 0x7f;
			tmp[1] = (*(p+1)) & 0x7f;
			wid += XTextWidth16(obj->win.font_struct16, (XChar2b*)tmp, 1);
			p+=2;
		} else {
			tmp[0] = (*p) & 0x7f;
			wid += XTextWidth(obj->win.font_struct, tmp, 1);
			p ++;
		}
	}
	return wid;
}

/* 表示領域中に含まれるラベル情報を表示する */
void sas_draw_label(SasProp *obj, int start, int end)
{
	int  sv, ev;
	int  i, ss, es, sx, ex, lw, lx, y1, y2, y3;
	int  overlap;

	if( ! obj->lbl || ! obj->nlbl ) return;
	sv = obj->view.sview;
	ev = obj->view.sview + obj->view.nview;
	y1 = obj->win.scaleh;                    /* 線の上端 */
	y3 = obj->win.size.height - obj->win.scaleh; /* 線の下端 */
	y2 = y3 - obj->win.lblh;                                     /* 横線 */
	for(i=start; i<=end; i++){
		if( i< 0 ) continue;
		if( i >= obj->nlbl ) break;
		if( obj->lbl[i].fname && strcmp(obj->lbl[i].fname,obj->file.name) )
			continue; /* ファイル名が違う時 */
		ss = obj->lbl[i].start * obj->file.freq / 1000;
		es = obj->lbl[i].end * obj->file.freq / 1000;
		if( ss > es ) continue;
		if( ss <= ev && es >= sv ){ /* 表示範囲に入っている時 */
			sx = (ss - obj->view.sview) / obj->view.spp + obj->win.scalew;
			ex = (es - obj->view.sview) / obj->view.spp + obj->win.scalew;

			/* オーバーラップ調査 */
			overlap = 0;
			if( i>0 && obj->lbl[i].start < obj->lbl[i-1].end ){
				if( !obj->lbl[i].fname || !obj->lbl[i-1].fname ||
				!strcmp(obj->lbl[i].fname,obj->lbl[i-1].fname) )
					overlap = 1;
			}
			if( i<obj->nlbl-1 && obj->lbl[i].end > obj->lbl[i+1].start ){
				if( !obj->lbl[i].fname || !obj->lbl[i+1].fname ||
				!strcmp(obj->lbl[i].fname,obj->lbl[i+1].fname) )
					overlap = 1;
			}
			/* 始点 */
			if( ss >= sv ){
				XDrawLine(obj->win.disp, obj->win.win, obj->win.mkgc,
				sx, y1, sx, y3);
				/* 始点が選択されていれば四角を描く */
				if( obj->clbl == i && obj->clblm == 1 ){
					XFillRectangle(obj->win.disp, obj->win.win, obj->win.mkgc,
						sx-3, y1-6, 6, 6);
					XFillRectangle(obj->win.disp, obj->win.win, obj->win.mkgc,
						sx-3, y3-0, 6, 6);
				}
			}
			else sx = obj->win.scalew;

			/* 終点 */
			if( ex <= obj->win.size.width ){
				XDrawLine(obj->win.disp, obj->win.win, obj->win.mkgc16,
				ex, y1, ex, y3);
				/* nlbl 番めの始点が選択されている時だけ、最終ラベルの終点を描く */
				if( obj->clbl == i && obj->clblm&2  ){
					XFillRectangle(obj->win.disp, obj->win.win, obj->win.mkgc,
						ex-3, y1-6, 6, 6);
					XFillRectangle(obj->win.disp, obj->win.win, obj->win.mkgc,
						ex-3, y3-0, 6, 6);
				}
			}
			else ex = obj->win.size.width -1;

#if 0
			/* 横線 */
			XDrawLine(obj->win.disp, obj->win.win, obj->win.mkgc, sx, y3, ex, y3);
#endif
			/* ラベル */
			lw = text_width(obj, obj->lbl[i].str);
			lx = (sx + ex)/2 - lw/2 + 1;
			if( ss>=ev && es<=sv ) continue; /* 丁度端っこの時は線だけ引く */
			if( lx < obj->win.scalew ) lx = obj->win.scalew;
			if( lx > obj->win.size.width - lw ) lx = obj->win.size.width - lw;
			draw_label_string(obj, lx, y3-5, obj->lbl[i].str);

			/* 補助線 */
			if( (obj->lblarc || overlap) && ex-sx > lw + 14 ){
				XDrawLine(obj->win.disp, obj->win.win, obj->win.smkgc,
				sx, y2+1, lx-6,    y2 + obj->win.lblh/2);
				XDrawLine(obj->win.disp, obj->win.win, obj->win.smkgc,
				ex, y2+1, lx+lw+6, y2 + obj->win.lblh/2);
			}

			/* 選択されていればリバースする */
			if( obj->clbl == i && obj->clblm == 4 )
				XFillRectangle(obj->win.disp, obj->win.win, obj->win.mkgc,
					lx-2, y2, lw+2, obj->win.lblh );
		}
	}
	XFlush(obj->win.disp);
}

/* 以下はラベル編集のための関数 */

/* マウスの x,y 座標から近くにあるラベルの始点の番号を返す */
/* なければ -1. ラベルの領域になければ -2 を返す */
/* (x,y)窓内の座標                               */
/* (c,m)ラベル番号とラベル要素(1:始点2:終点3:終点＋次始点4:ラベル) */
int  sas_xy_to_lbl(SasProp *obj, int x, int y, int *clbl, int *clblm)
{
	int x1, x2, y1, y2, mgn;
	int koho;

	*clbl = -1;
	*clblm = 0;
	if( obj->nlbl == 0 ) return -2;

	/* 近傍ラベル境界を探す */
	x2 = obj->win.size.width - 1;
	x1 = obj->win.scalew;
	y2 = obj->win.size.height - obj->win.scaleh;
	y1 = y2 - obj->win.lblh;
	mgn = obj->win.csmgn;
	*clbl = -1; /* 近傍なしを -1 とする */
	*clblm = 0; /* なし */
	koho = -1;
	if( y > y1 && y < y2 && x > x1 && x < x2 ){
		int l, smp, sx,ex,lw,lx;
		for( l=0; l<obj->nlbl; l++ ){

			/* リスト形式でファイル名が違う時スキップ */
			if( obj->lbl[l].fname && strcmp(obj->lbl[l].fname, obj->file.name) )
				continue;

			/* 始点チェック */
			smp = obj->lbl[l].start * obj->file.freq / 1000 - obj->view.sview;
			sx = smp / obj->view.spp + obj->win.scalew;
			if( x >= sx - mgn && x <= sx + mgn )
				{*clbl=l; *clblm=1; break;} /* 始点 */

			/* 終点チェック */
			smp = obj->lbl[l].end*obj->file.freq/1000 - obj->view.sview;
			ex = smp / obj->view.spp + obj->win.scalew;
			if( x >= ex - mgn && x <= ex + mgn ){
				if( l == obj->nlbl-1                         /* 最終 */
				 || obj->lbl[l].end != obj->lbl[l+1].start ) /* 隙間あり */
					{*clbl=l; *clblm=2; break;}            /* 終点 */
				/* 区間が密着している場合 */
				if( !obj->lbl[l].fname || !obj->lbl[l+1].fname   /* ATR */
				 || !strcmp(obj->lbl[l].fname, obj->lbl[l+1].fname) ) /* 同ファイル */
					{*clbl=l; *clblm=3; break;}            /* 終点+次始点 */
				else
					{*clbl=l; *clblm=2; break;}            /* 終点 */
			}

			/* ラベルの位置かどうか */
			if( sx > x2 || ex < x1 ) continue;
			if( sx < x1 ) sx = x1;
			if( ex > x2 ) ex = x2;
			lw = text_width(obj, obj->lbl[l].str);
			lx = (sx + ex)/2 - lw/2;
			if( x >= lx - mgn && x <= lx + lw + mgn ){
				{*clbl=l; *clblm=4; break;}
			}

			/* 区間の中かどうか */
			if( sx < x && x < ex && koho < 0 ){
				koho=l;
			}
		}
		if( !(*clblm) && koho >= 0 )
			{*clbl=koho; *clblm=0;}
		return *clbl; /* エリア内 */
	}
	return -2; /* エリア外 */
}

/* 指定ラベルの(先頭、終端)時刻のx座標を返す */
int sas_lbl_to_x(SasProp *obj, int clbl, int clblm)
{
	int x;
	if( obj->nlbl == 0 || clbl < 0 || clbl >= obj->nlbl )
		return 0;
	switch( clblm ){
	case 1:
		x =  (int)(((int)(obj->lbl[clbl].start * obj->file.freq/1000) - obj->view.sview)
		/ obj->view.spp) + obj->win.scalew;
		break;
	case 2: case 3:
		x =  (int)(((int)(obj->lbl[clbl].end * obj->file.freq/1000) - obj->view.sview)
		/ obj->view.spp) + obj->win.scalew;
		break;
	default:
		x = obj->view.mx;	/* 分からん時はそのまま */
		break;
	}
	return x;
}

/* undo の準備 */
void sas_label_keep(SasProp *obj)
{
	if( obj->clbl < 0 ) return;
	switch( obj->clblm ){
	case 1:   obj->ulbl = obj->lbl[obj->clbl].start; break;
	case 2:
	case 3:   obj->ulbl = obj->lbl[obj->clbl].end; break;
	default: break;
	}
}

/* １回だけ undo */
void sas_label_undo(SasProp *obj)
{
	if( obj->nlbl <= 0 ) return;
	if( obj->clbl < 0  ) return;

	/* 消す */
	sas_draw_label(obj, obj->clbl, obj->clbl+1);
	/* 戻す */
	switch( obj->clblm ){
	case 1:   obj->lbl[obj->clbl].start = obj->ulbl; break;
	case 3:   obj->lbl[obj->clbl+1].start = obj->ulbl;
	case 2:   obj->lbl[obj->clbl].end = obj->ulbl; break;
	default: break;
	}
	/* 書く */
	sas_draw_label(obj, obj->clbl, obj->clbl+1);
/*
	if( obj->albl ){
		sas_label_del( obj, obj->albl );
		obj->albl = -1;
	}
	else
		obj->dlbl = obj->albl = -1;
*/
	sas_write_label(obj);
}

/* ラベル区間をえらぶ */
void sas_label_sel(SasProp *obj, int clbl, int clblm)
{
	int    ss, es, mgn, shift, i, incr;
	extern void sel_start_windows(SasProp *);
	extern void sel_end_windows(SasProp *);

	if( obj->nlbl <= 0 ) return;

	/* ファイル名が同じのがみつかるまで１周探す */
	incr = (clbl < obj->clbl)? (-1):1;
	for( i=0; i<obj->nlbl; i++, clbl+=incr ){
		if( clbl < 0 ) clbl = obj->nlbl - 1;
		else if( clbl >= obj->nlbl ) clbl = 0;
		if( !obj->lbl[clbl].fname || !obj->lbl[clbl].fname[0] ) break;
		if( !strcmp(obj->lbl[clbl].fname, obj->file.name) ) break;
	}
	if( i == obj->nlbl ) return;
//	if( obj->lbl[clbl].fname && strcmp(obj->lbl[clbl].fname,obj->file.name) ) return;
	obj->clbl = clbl;
	obj->clblm = clblm;

	/* 必要なら表示領域をシフトする */
	ss = obj->lbl[obj->clbl].start * obj->file.freq/1000;
	es = obj->lbl[obj->clbl].end * obj->file.freq/1000;
	mgn = obj->view.nview/20;
	if( es > obj->view.sview + obj->view.nview )
		sas_set_view(obj, ss - mgn, obj->view.nview);
	else if( ss < obj->view.sview )
		sas_set_view(obj, es + mgn - obj->view.nview, obj->view.nview);
	sas_redraw(obj);

	/* 選択領域を変更する。分析もあり得るので main に渡す */
	shift = global_shift;
	global_shift = 0;
	sas_sel_start(obj, obj->lbl[obj->clbl].start * obj->file.freq/1000, -1, 0);
	sel_start_windows(obj);
	sas_sel_end(obj, obj->lbl[obj->clbl].end * obj->file.freq/1000, -1, 0);
	sel_end_windows(obj);
	global_shift = shift;
}

/* ラベルの移動開始 */
void sas_label_grab(SasProp *obj)
{
	obj->albl = obj->dlbl = -1;
}

/* ラベルの移動,挿入 */
void sas_label_move(SasProp *obj, int x)
{
	int  clbl, clblm, smpl, sx, ex;
	float time, max;
	int  overlap;  /* overlap を許す場合 1 */
	extern void sync_labels(SasProp *obj);

	if( obj->nlbl <= 0 ) return;        /* ラベルなし */
	if( obj->clbl < 0 ) return;         /* ラベル選択なし */
	if( obj->clblm < 1 || obj->clblm > 3 ) return; /* 境界選択以外 2001.9.3 debug */

	clbl = obj->clbl;
	clblm = obj->clblm;

	/* 時刻に変換 */
	smpl = sas_x_to_sample(obj, x);
	time = (smpl+0.5) * 1000.0 / obj->file.freq;
	max = (double)(obj->file.size) * 1000.0 / obj->file.freq;
		/* 2000.11.2 double で計算しないとオーバーフローする */

	/* X座標に変換 */
	sx = sas_lbl_to_x(obj, clbl, 1);
	ex = sas_lbl_to_x(obj, clbl, 2);

	obj->dlbl = obj->albl = -1; /* 念の為 */

	/* shift は分割や追加 */
	if( global_shift && obj->mlbl == 1 /* 分割前 */ ){
/*
		clblm         1     1     2     2     3     3
		motion        L     R     L     R     L     R
		-
		slbl(split)  +0    +0    +0    +0    +0    +1
		albl(ins)    +0    +0    +1    +1    +1    +1
		head/tail     h     h     t     t     t     h
		clbl         +0    +0    +0    +1    +0    +1
		clblm         1     3     3     2     3     3
*/
		if( clblm == 1 && x < sx - obj->win.csmgn ){
			/* 左に追加 */
			if( ! sas_label_split(obj, clbl) )
				return;
			obj->lbl[clbl].end = obj->lbl[clbl+1].start = obj->lbl[clbl].start;
			sync_labels(obj);
			obj->clbl = clbl;
			obj->clblm = 1;
			obj->mlbl = 2;
		}
		else if( clblm == 1 && x > sx + obj->win.csmgn ){
			/* 右に挿入 */
			if( !sas_label_split(obj, clbl) )
				return;
			obj->lbl[clbl].end = obj->lbl[clbl+1].start = obj->lbl[clbl+1].end;
			sync_labels(obj);
			obj->clbl = clbl;
			obj->clblm = 3;
			obj->mlbl = 2;
		}
		else if( clblm == 2 && x < ex - obj->win.csmgn ){
			/* 左に挿入 */
			if( !sas_label_split(obj, clbl) )
				return;
			obj->lbl[clbl].end = obj->lbl[clbl+1].start = obj->lbl[clbl+1].end;
			sync_labels(obj);
			obj->clbl = clbl;
			obj->clblm = 3;
			obj->mlbl = 2;
		}
		else if( clblm == 2 && x > ex + obj->win.csmgn ){
			/* 右に追加 */
			if( !sas_label_split(obj, clbl) )
				return;
			obj->lbl[clbl].end = obj->lbl[clbl+1].start = obj->lbl[clbl+1].end;
			sync_labels(obj);
			obj->clbl = clbl+1;
			obj->clblm = 2;
			obj->mlbl = 2;
		}
		else if( clblm == 3 && x < ex - obj->win.csmgn ){
			/* 左に挿入 */
			if( !sas_label_split(obj, clbl) )
				return;
			obj->lbl[clbl].end = obj->lbl[clbl+1].start = obj->lbl[clbl+1].end;
			sync_labels(obj);
			obj->clbl = clbl;
			obj->clblm = 3;
			obj->mlbl = 2;
		}
		else if( clblm == 3 && x > ex + obj->win.csmgn ){
			/* 右に挿入 */
			if( !sas_label_split(obj, clbl+1) )
				return;
			obj->lbl[clbl+1].end = obj->lbl[clbl+2].start = obj->lbl[clbl+1].start;
			sync_labels(obj);
			obj->clbl = clbl+1;
			obj->clblm = 3;
			obj->mlbl = 2;
		}
		else{
			return;
		}
		sas_redraw(obj);
	}

	/* ctrl はくっついているのを分離 */
	if( global_ctrl ){
		/* 最初に左へうごいたら 終点変更 */
		if( clblm == 3 && x < ex - obj->win.csmgn ){
			clblm = obj->clblm = 2;
		}
		/* 最初に右へうごいたら 次の区間の始点変更 */
		else if( clblm == 3 && x > ex + obj->win.csmgn ){
			sas_draw_label(obj,obj->clbl,obj->clbl+1);
			clbl = obj->clbl = clbl + 1;
			clblm = obj->clblm = 1;
			sas_draw_label(obj,obj->clbl-1,obj->clbl);
		}
		/* 区間移動量が少ない時は何もしない */
		else if( clblm == 3 ){
			return;
		}
	}

	/* 標準消す */
	sas_draw_label(obj, clbl-1, clbl+1);

	/* 上限、下限 */
	/* ctrl キーが押されていたらオーバーラップを許す */
	/* 最初からオーバーラップしていたらそのままオーバーラップを許すが */
	/* 一度オーバーラップを止めたら、ctrl キーなしでオーバーラップできない */
	if( time > max ) time = max ;
	if( time < 0.0 ) time = 0.0;
	if( global_ctrl ) overlap = 1;
	else                 overlap = 0;
	if( clblm == 1/*始点*/ ){
		if( clbl>0 && obj->lbl[clbl].start < obj->lbl[clbl-1].end )
			overlap = 1;
		if( time > obj->lbl[clbl].end ) /* 上限 */
			time = obj->lbl[clbl].end;
		if( !overlap && clbl>0 && time < obj->lbl[clbl-1].end ){ /* 下限 */
			if( !obj->lbl[clbl].fname || !obj->lbl[clbl-1].fname
			 || !strcmp(obj->lbl[clbl].fname,obj->lbl[clbl-1].fname) )
				time = obj->lbl[clbl-1].end; /* 同一ファイル内 */
		}
	}
	else if( clblm==2/*終点*/ ){
		if( time < obj->lbl[clbl].start ) /* 下限 */
			time = obj->lbl[clbl].start;
		if( clbl < obj->nlbl-1 && obj->lbl[clbl].end > obj->lbl[clbl+1].start )
			overlap = 1;
		if( !overlap && clbl<obj->nlbl-1 && obj->lbl[clbl+1].start < time ){
			if( !obj->lbl[clbl].fname || !obj->lbl[clbl+1].fname
			 || !strcmp(obj->lbl[clbl].fname,obj->lbl[clbl+1].fname) )
				time = obj->lbl[clbl+1].start;
		}
	}
	else if( clblm==3/*終点+始点*/ ){
		if( time < obj->lbl[clbl].start ) /* 下限 */
			time = obj->lbl[clbl].start;
		if( clbl<obj->nlbl-1 && obj->lbl[clbl+1].end < time ){
			if( !obj->lbl[clbl].fname || !obj->lbl[clbl+1].fname
			 || !strcmp(obj->lbl[clbl].fname,obj->lbl[clbl+1].fname) )
				time = obj->lbl[clbl+1].end;
		}
	}

	/* 変更 */
	switch( clblm ){
	case 1:  obj->lbl[clbl].start = time; break;
	case 2:  obj->lbl[clbl].end = time; break;
	case 3:  obj->lbl[clbl].end = obj->lbl[clbl+1].start = time; break;
	default: break;
	}

	/* 書く */
	sas_draw_label(obj, clbl-1, clbl+1);
}

/* 移動の終了 */
void sas_label_release(SasProp *obj)
{
	/* ファイルセーブするだけ */
	sas_write_label(obj);
/*	obj->albl = obj->dlbl = -1; */
}


/* 文字の編集 */
void sas_label_edit(SasProp *obj, char *str)
{
	char *p;

	obj->dlbl = obj->albl = -1;
	if( obj->nlbl <= 0 ) return;
	if( obj->clbl < 0 || obj->clbl >= obj->nlbl || obj->clblm != 4 ) return;
	sas_draw_label(obj, obj->clbl, obj->clbl);
	if( ! (p=realloc(obj->lbl[obj->clbl].str,strlen(str)+1)) ){
		fprintf(stderr,"can't realloc\n");
	} else {
		strcpy(p,str);
		obj->lbl[obj->clbl].str = p;
	}
	sas_draw_label(obj, obj->clbl, obj->clbl);
	sas_write_label(obj);
}

/* ラベル削除 */
void sas_label_del(SasProp *obj, int dlbl)
{
	obj->dlbl = obj->albl = -1;
	if( obj->nlbl < 0 ) return;
	if( dlbl < 0 || dlbl >= obj->nlbl ) return;
	obj->dlbl = dlbl;
	sas_draw_label(obj,dlbl,dlbl);   /* 表示上消す */
	del_label(obj,dlbl);             /* データ上消す */
	/* 選択ラベルより前を消した時は、選択ラベルを移動させる */
	if( obj->clbl >= dlbl && dlbl > 0 ) obj->clbl --;
	sas_write_label(obj);
	if( obj->nlbl == 0 )
		obj->view.redraw = 1;
	sas_redraw(obj);
}

/* ラベル分割 */
/* 成功したら 1 失敗したら 0 */
int sas_label_split(SasProp *obj, int clbl)
{
#define  STRMAX  512
	float mid, max;
	char  *str, *fname;
	char  localstr[STRMAX], *comma;
	obj->dlbl = obj->albl = -1; /*  念の為 */
	if( obj->nlbl < 1 || clbl < 0 || clbl >= obj->nlbl )
		return 0;

	mid = (obj->lbl[clbl].start + obj->lbl[clbl].end) / 2;
	max = (double)(obj->file.size) * 1000.0 / obj->file.freq;
	str = obj->lbl[clbl].str;
	fname = obj->lbl[clbl].fname;
	if( obj->lblmode == 2 || obj->lblmode == 3 ){
		strncpy(localstr,obj->lbl[clbl].str,STRMAX);
		localstr[STRMAX-1] = 0; /* 念のため */
		comma = strchr(localstr,',');
		if( comma ){
			*comma = 0;
			str = localstr;
			edit_labelstr(obj, clbl, comma+1);
		}
		else if( obj->lblmode == 2 ) return 0;
			/* 分割できない */
	}
	ins_label(obj, clbl, obj->lbl[clbl].start, mid, 0, max, str, fname);
	obj->lbl[clbl+1].start = mid;

	obj->dlbl = -1;
	obj->albl = clbl;
	obj->clbl = clbl+1;
	obj->clblm = 4;

	sas_write_label(obj);
	sas_redraw(obj);
/*
	sas_sel_start(obj, obj->lbl[obj->clbl].start * obj->file.freq/1000, -1, 0);
	sas_sel_end(obj, obj->lbl[obj->clbl].end * obj->file.freq/1000, -1, 0);
*/
	return 1;
}

/* ラベル結合 */
void sas_label_merge(SasProp *obj, int clbl)
{
	float  start, end;
	if( clbl <= 0 || obj->nlbl < 2 ) return;

	start = obj->lbl[clbl-1].start;
	if( obj->lbl[clbl].start < start )
		start = obj->lbl[clbl].start;
	end = obj->lbl[clbl].end;
	if( obj->lbl[clbl-1].end > end )
		end = obj->lbl[clbl-1].end;

	obj->lbl[clbl].start = start;
	obj->lbl[clbl].end = end;
	if( obj->lblmode != 1 ){  /* mode 0 の時はラベルを合成する */
		int l1, l2;
		char *s1, *s2, *s3;
		s1 = obj->lbl[clbl-1].str;
		s2 = obj->lbl[clbl].str;
		if( s1 ) l1 = strlen(s1); else l1 = 0;
		if( s2 ) l2 = strlen(s2); else l2 = 0;
		if( l1 + l2 > 0 ){
			s3 = (char *)malloc(l1+l2+2);
			if( s1 ) strcpy(s3,s1);
			if( s1 && s2 ) strcat(s3,",");
			if( s2 ) strcat(s3,s2);
			if( s2 ) free( s2 );
			obj->lbl[clbl].str = s3;
		}
	}

	obj->dlbl = clbl - 1;
	del_label(obj, clbl - 1);             /* データ上消す */
	obj->clbl = clbl - 1;
	obj->clblm = 4;

	sas_write_label(obj);
	sas_redraw(obj);
	sas_sel_start(obj, obj->lbl[obj->clbl].start * obj->file.freq/1000, -1, 0);
	sas_sel_end(obj, obj->lbl[obj->clbl].end * obj->file.freq/1000, -1, 0);
}

/* 新規作成 */
void sas_label_add(SasProp *obj,
	float st, float en, float prev, float next, char *str)
{
	int posi;
	/* 挿入場所を探す */
	for(posi=0;posi<obj->nlbl;posi++){
		if( obj->lbl[posi].fname
		  && strcmp(obj->file.name, obj->lbl[posi].fname) ) continue;
		if( en < obj->lbl[posi].end ) break;
	}
	/* ラベル挿入 */
	if( obj->lbltype == LBL_ATR )
		add_label(obj, posi, st, en, st, en, str, "");
	else
		add_label(obj, posi, st, en, st, en, str, obj->file.name);
	sas_write_label(obj);
	obj->clbl = obj->albl = posi;
	obj->clblm = 4;
	/* 始めてのときはレイアウトがかわるのでスケールも再描画 */
	if( obj->nlbl == 1 ) obj->view.redraw = 1;
	sas_redraw(obj);
}

