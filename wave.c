#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include "sas.h"
#include "wave.h"

/* =================================================================
 y 方向クリッピング付き線の描画用クリッピング関数
	ymin: y座標下限 ymax: y座標上限
================================================================= */
int  sas_clip_y(int x1, int y1, int x2, int y2, int ymin, int ymax,
	int *xx1, int *yy1, int *xx2, int *yy2)
{
	if( y2 < y1 ){
		*xx1 = x1; x1 = x2; x2 = *xx1;
		*yy1 = y1; y1 = y2; y2 = *yy1;
	}
	if( y1 < ymin ){
		if( y2 > ymax ){
			*xx1 = x1 + (x2-x1) * (ymin-y1) / (y2-y1);
			*xx2 = x1 + (x2-x1) * (ymax-y1) / (y2-y1);
			*yy1 = ymin;
			*yy2 = ymax;
		}
		else if( y2 > ymin ){
			*xx1 = x1 + (x2-x1) * (ymin-y1) / (y2-y1);
			*xx2 = x2;
			*yy1 = ymin;
			*yy2 = y2;
		}
		else
			return 0;
	}
	else if( y1 < ymax ){
		if( y2 > ymax ){
			*xx1 = x1;
			*xx2 = x1 + (x2-x1) * (ymax-y1) / (y2-y1);
			*yy1 = y1;
			*yy2 = ymax;
		}
		else{
			*xx1 = x1;
			*xx2 = x2;
			*yy1 = y1;
			*yy2 = y2;
		}
	}
	else
		return 0;

	return 1;
}

/* =================================================================
 y 方向クリッピング付き線の描画
	ymin: y座標下限 ymax: y座標上限
================================================================= */
void  sas_draw_clip_y(SasProp *obj, GC gc, int x1, int y1, int x2, int y2,
	 int ymin, int ymax)
{
	int   xx1, xx2, yy1, yy2;
	if( !sas_clip_y( x1, y1, x2, y2, ymin, ymax, &xx1, &yy1, &xx2, &yy2) )
		return;
	XDrawLine(obj->win.disp,obj->win.win,gc, xx1, yy1, xx2, yy2);
	XDrawLine(obj->win.disp,obj->win.pix,gc, xx1, yy1, xx2, yy2);
}


int read_ascii_num(
	FILE *fp,
	char *word,
	int  len)
{
	int c, i = 0;
	while((c=fgetc(fp)) != EOF){
		if( strchr("0123456789-+.eE",c) ){
			word[i++] = c;
			if( i == len-1 )
				break;
		}
		else if( i )
			break;
	}
	word[i] = 0;
	if( c == EOF && i == 0 )
		return EOF;
	else
		return i;
}

#define  WLEN 64

int ascii_filesize(char *fname)
{
	int  i;
	char word[WLEN];
	FILE *fp;
	if( !(fp=fopen(fname,"r")) ) return 0;
	for( i=0; read_ascii_num(fp, word, WLEN) != EOF; i++ ) ;
	fclose(fp);
	return i;
}

int read_ascii(
	FILE *fp,
	int offset,  /* sample */
	int nchan,   /* 1,2,3,.... */
	int start,
	int len,
	double *dbuff)
{
	int  i;
	char word[WLEN];
	if( fseek(fp, 0, SEEK_SET) < 0 )
		return -1;
	for( i=0; i<nchan * (offset + start); i++ )
		if( read_ascii_num(fp, word, WLEN) == EOF )
			return 0;
	for( i=0; i<nchan * len; i++ ){
		if( read_ascii_num(fp, word, WLEN) == EOF )
			break;
		dbuff[i] = atof(word);
	}
	return i/nchan;
}

/* =================================================================
double のバッファに波形を読み込む
読めたサンプル数(チャンネル数で割った数)を返す
================================================================= */
int read_double(
	FILE *fp,
	int hsize,   /* header size (byte) */
	int offset,  /* sample */
	int type,    /* S L F D */
	int nchan,   /* 1,2,3,.... */
	int swap,    /* 0,1 */
	int start,
	int len,
	double *dbuff)
{
	long  unit, l, i;
	union { unsigned char *uc; char *c; short *s; long *l; float *f; double *d; } buff;

	if(type == 'A')
		return read_ascii(fp, offset, nchan, start, len, dbuff);

	if     (type == 'U') unit = 1;
	else if(type == 'C') unit = 1;
	else if(type == 'S') unit = 2;
	else if(type == 'L') unit = 4;
	else if(type == 'F') unit = 4;
	else if(type == 'D') unit = 8;
	else return 0;

	/* SEEK */
	if( (l=fseek(fp, hsize + unit*nchan*(start+offset),SEEK_SET)) < 0 )
		return -1;

	/* WAVE BUFFER OVERLAY */
	buff.c = (void *)dbuff;

	/* READ */
	if( (l=fread(buff.c, unit*nchan, len, fp)) <= 0 )
		return l;

	/* SWAP IF NEEDED */
	if( swap )
		swapbyte(unit*nchan*l, unit, (char *)buff.c);

	/* CONVERT */
	if     ( type == 'U' )
		for( i=l*nchan-1; i>=0; i-- ) dbuff[i] = (double)buff.uc[i];
	if     ( type == 'C' )
		for( i=l*nchan-1; i>=0; i-- ) dbuff[i] = (double)buff.c[i];
	else if( type == 'S' )
		for( i=l*nchan-1; i>=0; i-- ) dbuff[i] = (double)buff.s[i];
	else if( type == 'L' )
		for( i=l*nchan-1; i>=0; i-- ) dbuff[i] = (double)buff.l[i];
	else if( type == 'F' )
		for( i=l*nchan-1; i>=0; i-- ) dbuff[i] = (double)buff.f[i];
	/* D : nothing to do */

	return l;
}

/* =================================================================
波形を描画(普通の描画)
	表示サンプル数が少ない時
	表示区間のサンプルを全部読み込む
================================================================= */
int  sas_slow_wave(SasProp *obj)
{
	double   *buff;
	int x[256], y[256];
	FILE     *fp;
	int i, j, l, ch, nchan, sview, nview, ybias, swap;
	float ymax, ymin, yrange;
	int h, w, scaleh, scalew, lblh;

	nchan = obj->file.chan;
	sview = obj->view.sview;
	nview = obj->view.nview;

	if( nview <= 1 ) return 0;

	swap  = (obj->file.endian == obj->view.cpuendian)? 0: 1;

	/* WAVE BUFFER */
	buff = (double *)malloc(sizeof(double)*nchan*(nview+1));
	if( !buff ){
		fprintf(stderr,"can't alloc buff\n");
		return -1;
	}

	/* FILE ACCESS */
	if( !(fp=fopen(obj->file.name,"r")) ){
		fprintf(stderr,"can't open %s\n",obj->file.name);
		free(buff); return -1;
	}
	l = read_double(fp, obj->file.hsize, obj->file.offset, obj->file.type, nchan, swap, sview, nview+1, buff);
	fclose(fp);

	if( l <= 0 ){ free(buff); return 0; }

	ymax = obj->view.ymax;
	ymin = obj->view.ymin;
	yrange = ymax - ymin;
	scaleh = obj->win.scaleh;
	scalew = obj->win.scalew;
	lblh = (obj->nlbl)? obj->win.lblh:0;
	h = obj->win.size.height - scaleh*2 - lblh;
	w = obj->win.size.width - scalew;

	j = 0;
	for( i=0; i<=nview && i<l; i++ ){
		for( ch=0; ch<nchan; ch++ ){
			int xx, yy, a;
			ybias = h*ch/nchan + scaleh;
			xx = w*i/nview + scalew;
			yy = ybias + h*(ymax-buff[j++])/yrange/nchan;
			a = (yy<y[ch])?(-1):((yy>y[ch])?1:0);	/* 線の途切れ防止に */
			if( i > 0 )
				sas_draw_clip_y(obj,obj->win.fggc, x[ch],y[ch],xx,yy+a,
				  obj->win.scaleh, obj->win.scaleh + h);
			x[ch] = xx;
			y[ch] = yy;
		}
	}
	XFlush(obj->win.disp);
	free(buff);
	return 0;
}

/* =================================================================
波形を描画(高速描画)
	表示サンプル数が多い時
	固定長で読み込んで順次縦線を描画
================================================================= */
#define FW_BUF 4096
int  sas_fast_wave(SasProp *obj)
{
	int     start, len, rest, limit, i, o, p, to, swap;
	float	fto;
	int     nchan, ch;
	int     x, ybias, hrange, scaleh, lblh;
	float   ymin, ymax, yrange;
	double	buff[FW_BUF];
	double  max[MAX_NCHAN], min[MAX_NCHAN], pmax[MAX_NCHAN], pmin[MAX_NCHAN];
	FILE   *fp;

	if( obj->view.nview <= 1 ) return 0;

	if( !(fp=fopen(obj->file.name,"r")) ){
		fprintf(stderr,"can't open %s\n",obj->file.name);
		return(-1);
	}

	swap   = (obj->file.endian == obj->view.cpuendian)? 0: 1;
	start  = obj->view.sview;
	nchan  = obj->file.chan;
	limit  = FW_BUF / nchan;

	ymin   = obj->view.ymin;
	ymax   = obj->view.ymax;
	yrange = ymax - ymin;
	scaleh = obj->win.scaleh;
	lblh   = (obj->nlbl)?obj->win.lblh:0;
	hrange = obj->win.size.height - scaleh*2 - lblh;

	fto    = obj->view.spp;
	to     = ((int)fto)*nchan;

	/* 初期化 */
	for(ch=0;ch<nchan;ch++){
		max[ch] = pmin[ch] = (-MAX_AMP);
		min[ch] = pmax[ch] = MAX_AMP;
	}

	/*  i: 読み込みサンプル数(1チャンネルあたり) */
	/*  o:表示に使用したサンプル数 */
	x=obj->win.scalew;
	for( i=o=0; i<obj->view.nview; i+=len, start+=len ){
		/* とりあえず読むサンプル数(上限固定) */
		rest = obj->view.nview - i;
		if( rest > limit ) rest = limit;

		len = read_double(fp, obj->file.hsize, obj->file.offset, obj->file.type, nchan, swap, start, rest, buff);
		if( len <= 0 ) break;

		for( p=0; p<len*nchan; o++,p++ ){
			/* サンプルが集まるまでmax,minを計算 */
			ch = o%(obj->file.chan);
			if(max[ch] < buff[p]) max[ch] = buff[p];
			if(min[ch] > buff[p]) min[ch] = buff[p];
			if( o!=to ) continue;

			/* サンプルが集まったら(o==to)１本線引く */
			for(ch=0; ch<nchan; ch++){
				int y1, y2;
				ybias = hrange*ch/nchan + scaleh;
				/* 見ためが途切れないように */
				if( min[ch] > pmax[ch] ) min[ch] = pmax[ch];
				else if( max[ch] < pmin[ch] ) max[ch] = pmin[ch];
				y1 = ybias+hrange*(ymax-min[ch])/yrange/nchan;
				y2 = ybias+hrange*(ymax-max[ch])/yrange/nchan;
				if( y1 == y2 ) y2++;
				sas_draw_clip_y(obj, obj->win.fggc,
				  x, y1, x, y2, scaleh, scaleh + hrange);
				pmin[ch] = min[ch];
				pmax[ch] = max[ch];
				max[ch]=-32768;
				min[ch]=32767;
			}
			fto += obj->view.spp;
			to = ((int)fto)*nchan;
			x++;
		}
	}
	XFlush(obj->win.disp);

	fclose(fp);
	return 0;
}

/* =================================================================
選択範囲のセーブ
	とりあえずバージョン 2004.7.30
	ASCII の時は非対応
	出力は元のデータ形式で、ただしRAWデータ
================================================================= */
int sas_save_area(SasProp *obj)
{
	int	i, unit, l, start, len;
	FILE *ifp, *ofp;
	char *ofname = "sastmp.raw";
	char buf[8];

	if( obj->view.nsel == 0 ) return 0;

	else if( obj->view.nsel < 0 ){
		start =   obj->view.ssel + obj->view.nsel;
		len   = - obj->view.nsel;
	}
	else {
		start = obj->view.ssel;
		len   = obj->view.nsel;
	}

	if     (obj->file.type == 'U') unit = 1;
	else if(obj->file.type == 'C') unit = 1;
	else if(obj->file.type == 'S') unit = 2;
	else if(obj->file.type == 'L') unit = 4;
	else if(obj->file.type == 'F') unit = 4;
	else if(obj->file.type == 'D') unit = 8;
	else return -1;

	if( !(ifp=fopen(obj->file.name,"r")) ){
		fprintf(stderr,"can't open %s\n",obj->file.name);
		return -1;
	}

	if( ! (ofp=fopen(ofname,"w")) ){
		fprintf(stderr,"can't open %s\n",ofname);
		fclose(ifp);
		return -1;
	}

	if( (l=fseek(ifp, obj->file.hsize +
			unit * obj->file.chan * (start + obj->file.offset),
			SEEK_SET)) < 0 )
		return -1;

	for( i=0; i<len; i++ ){
		if( ! fread(buf, unit, 1, ifp) )
			break;
		if( ! fwrite(buf, unit, 1, ofp) )
			break;
	}

	fclose(ifp);
	fclose(ofp);

	return len;
}

