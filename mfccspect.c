#include <stdio.h>
#include <stdlib.h>
#include <string.h>	/* memset */
#include <math.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include "sas.h"
#include "wave.h"
#include "spectro.h"


/*
	スペクトログラムを描画
	ハードコピーが同じになるよう誤差分散法を使って白黒２値でグレー表示

	fs	256なら		128なら		64なら
	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	 8k	32ms/31Hz	16ms/62.5Hz	8ms/125Hz
	12k	21ms/46.8Hz	10.7ms/93.7Hz	5.3ms/187Hz
	16k	16ms/63Hz	8ms/125Hz	4ms/250Hz

*/
int sas_draw_mfcc_spectrogram(SasProp* obj)
{
	FILE    *fp;
	int      order, fbnum, lifter;
	int      i, j, k, size, skip, fftsize, ffti, len, nchan, swap;
	int      y1, y2, yr, y, lblh, powh;
	int      x1, x2, x, xstart, xlast;
	short    *sbuff;
	float    *mfcc;
	float    *mfbpf;
	double   *dbuff;
	double   *xr, *xi, gain, power=0;
	float    rest, val;
	double   spow  = obj->view.spow;
	float    pre   = obj->view.pre;
	int      ch, chy;
	double   scale, scale2, shift;

	extern int     fbankpow;
	extern int     fbanknormalize;
	extern void    Wav2MFCC_E_D(short *wave, float *mfcc, long sampsize,
		long sfreq, int fbank_num, int raw_e, float preEmph,
		int mfcc_dim, int lifter, float *mfbpf);

	extern float   Mel(float);
	float          hz, melmax;

#define GSCALE 64
	int    cval;
	GC     gcgray[GSCALE];

	if( (fp=fopen(obj->file.name,"r"))==0 ){
		fprintf(stderr,"can't open %s\n",obj->file.name);
		return -1;
	}

	if( obj->view.grayscale == 1 )
		sas_create_grayscale(obj, gcgray, GSCALE);
	else if( obj->view.grayscale == 2 )
		sas_create_colorscale(obj, gcgray, GSCALE);

	swap  = (obj->file.endian == obj->view.cpuendian)? 0: 1;
	nchan = obj->file.chan;
	lblh  = (obj->nlbl)?obj->win.lblh: 0;
	powh  = (obj->view.power)? obj->view.powerh: 0;
	x1 = obj->win.scalew;
	x2 = obj->win.size.width - 1;
	y1 = obj->win.scaleh;
	y2 = obj->win.size.height - obj->win.scaleh - lblh;
	yr = (y2 - y1)/(obj->file.chan);

	/* calc gain */
	gain = 65536.0 / (obj->view.ymax - obj->view.ymin);
	if( obj->view.grayscale != 0 )
		gain /= 2;  /* dither に比べて弱めにするほうがきれい */

	size = (int)(obj->file.freq * obj->view.framesize / 1000);
	skip = (int)(obj->file.freq * obj->view.frameskip / 1000);
	order = 12;
	fbnum = 24;
	lifter = 0; /*obj->view.analifter;*/
/*
	scale = 20.0/log(10.0)/sqrt((double)fbnum);
	shift = log((double)size)/2*sqrt((double)fbnum);
	scale2 = 20.0/log(10.0)/sqrt((double)fbnum)/(1+lifter/sqrt(2.0));
*/
/*
	scale = 1.0/sqrt((double)fbnum);
	shift = log((double)size)/2*sqrt((double)fbnum);
	scale2 = 1.0/sqrt((double)fbnum)/(1+lifter/sqrt(2.0));
*/
	scale = 2.0/sqrt((double)fbnum);  /* 2.0 は何故か不明 */
	shift = log((double)size) / 2 * sqrt((double)fbnum);
	scale2 = 1.0/sqrt((double)fbnum)/(1+lifter/sqrt(2.0));

	melmax = Mel((float)obj->file.freq/2/obj->view.fzoom);

	/* 全然進まないと困るので 2000.12.8 */
	if( size < 1 ) size = 1;
	if( skip < 1 ) skip = 1;

	ffti=8; fftsize=256;
	/*for( ffti=2, fftsize=4; fftsize<order; ffti++, fftsize*=2 );*/
	/* スペクトルをスムーズに見せるためにFFTは大きめにしています */
/*
	printf("narrow %d zoom %g size %d skip %d fft %d\n",
	obj->view.narrow,obj->view.fzoom,size,skip,fftsize);
*/

	sbuff = malloc(sizeof(short)*size);
	mfcc = malloc(sizeof(float)*(order+1));
	mfbpf = malloc(sizeof(float)*(fbnum+1));
	dbuff = malloc(sizeof(double)*nchan*size);
	xr    = malloc(sizeof(double)*fftsize*2);
	xi    = malloc(sizeof(double)*fftsize*2);
	if( !dbuff || !mfcc || !sbuff || !xr || !xi ){
		fprintf(stderr,"can't alloc fft buffers\n");
		fclose(fp);
		return -1;
	}

	/* 分析フレーム(j)ループ */
	xlast  = -1; /* 同じ x 座標に重複して描画するのを防ぐ為 */
	for( j=0,i=obj->view.sview;
	     j<obj->view.nview+skip/2 && i<obj->file.size;
	     j+=skip,i+=skip ){
		if( i<0 ) continue;
		xstart = xlast+1;
		xlast  = (j+skip/2)/obj->view.spp;
		if( xstart > xlast ) continue;
		if( i < size/2 ){ /* 最初 */
			int size2 = i + size/2;
			memset(dbuff, 0, sizeof(double)*nchan*(size-size2));
			len = read_double(fp, obj->file.hsize, obj->file.offset, obj->file.type, nchan, swap,
			       0, size2, &dbuff[nchan*(size-size2)]);
			if( len < size2 ) continue;
			len = size;
		} else {
			len = read_double(fp, obj->file.hsize, obj->file.offset, obj->file.type, nchan, swap, (i-size/2), size, dbuff);
			if( len <= 0 ) continue;
			if( len < size )
				memset(&dbuff[nchan*len], 0, sizeof(double)*nchan*(size-len));
		}

		for( ch=0; ch<obj->file.chan ; ch++ ){
			for( k=0; k<size; k++ )
				sbuff[k] = (short)dbuff[nchan*k+ch];
			fbankpow = 1;
			fbanknormalize = 1;
			Wav2MFCC_E_D(sbuff, mfcc, size, obj->file.freq, fbnum,
				0/*raw_e*/, pre, order, lifter, mfbpf);
			fbankpow = 0;
			fbanknormalize = 0;

			memset( xi, 0, sizeof(double)*fftsize );
			memset( xr, 0, sizeof(double)*fftsize );
			for( k=1; k<=order; k++ ){
				xr[k] = xr[fftsize-k] = scale2 * mfcc[k];
			}
			xr[0] = power = scale * (mfcc[0] + log(gain)*4 - shift);

			/* パワーを表示するなら計算する */
			if( obj->view.power ){
				power = exp(power);
				power = pow(power,spow) / pow(32768.0,spow) / 4;
				/*power = (log(power+0.5)-log(0.5)) / 21;  */ /* log(32767**2+0.5)-log(0.5)=21.48 */
				if( power > 1.0 ) power = 1.0;
			}

			/* 周波数領域に変換 */
			fft( xr, xi, ffti, fftsize );

			/* 対数パワーの計算(ver1.3から約３乗根) */
			for( k=0; k<fftsize/2; k++ ){
				double p = exp(xr[k]);
				xr[k] = pow(p,spow) / pow(32768.0,spow) / 4;
				/*xr[k] = (log(p+3)-log(3)) / 20;*/
				if(xr[k]<0.0) xr[k]=0.0;
				if(xr[k]>1.0) xr[k]=1.0;
			}

			/* 周波数０の位置 */
			chy = y1 + yr * (ch+1);

			/* 時間方向ピクセル(x)ループ */
			/* １回の窓の計算結果で表示するピクセル幅の繰り返し */
			/* 抜けを防ぐ為少し手前からループを回す。重複しないようxlastで判断 */
			for( x=xstart; x<=xlast && x<x2-x1;
		  	x ++ ){
				/* パワー表示 */
				if( obj->view.power ){
					XDrawLine(obj->win.disp, obj->win.win,
					 obj->win.fggc, x1+x, chy, x1+x, chy - powh * power);
					XDrawLine(obj->win.disp, obj->win.pix,
					 obj->win.fggc, x1+x, chy, x1+x, chy - powh * power);
				}

				/* モアレや縞が出ないよう、各列の初期誤差に乱数を使う */
				rest = (float)(rand()%2048)/2048.0 - 0.5;
				/* 周波数方向ピクセル(y)ループ */
				for( y=0; y<(yr-powh); y++ ){
					/* 希望値(0<val<1)を誤差(-0.5<rest<+0.5)に足して */
					/* 0.5を越えたら点を描く。0.5未満なら描かない。 */
					/* 描いたら誤差から1を引く */

					/* linear -> linear */
					/*k = fftsize/2/obj->view.fzoom*y/(yr-powh);*/	/* FFTバンド(k) */
					if( obj->view.fscale == FSC_MEL ){
						/* y mel -> k mel */
						k =  fftsize/2 * y/(yr-powh) * melmax / Mel((float)obj->file.freq/2);	/* FFTバンド(k) */
					}
					else {
						/* y linear -> k mel */
						hz = (float)obj->file.freq/2/obj->view.fzoom * y/(yr-powh);	/* リニアHz */
						k =  fftsize/2 * Mel(hz) / Mel((float)obj->file.freq/2);	/* FFTバンド(k) */
					}

					val = xr[k];

					if( obj->view.grayscale != 0 ){
						/* DRAW with GRAYSCALE */
						/* 四角で塗る方が早いが、まあいいや */
						/* グレースケールやカラースケールでは */
						/* 若干鈍感にしたほうがきれい */
						cval = (int)(GSCALE * val);
						if( cval >= GSCALE ) cval = GSCALE-1;
						if( cval < 0  ) cval = 0;
						XDrawPoint(obj->win.disp, obj->win.win,
							gcgray[cval], x1+x, chy-powh-y );
						XDrawPoint(obj->win.disp, obj->win.pix,
							gcgray[cval], x1+x, chy-powh-y );
					}
					else {
						/* DRAW WITH DITHER */
						/* 希望値(0<val<1)を誤差(-0.5<rest<+0.5)に足して */
						/* 0.5を越えたら点を描く。0.5未満なら描かない。 */
						/* 描いたら誤差から1を引く */
						rest += val;
						if( rest >= 0.5 ){
							if( obj->view.pitch ){
								XDrawPoint(obj->win.disp, obj->win.win,
								obj->win.dimgc, x1+x, chy-powh-y );
								XDrawPoint(obj->win.disp, obj->win.pix,
								obj->win.dimgc, x1+x, chy-powh-y );
							} else {
								XDrawPoint(obj->win.disp, obj->win.win,
								obj->win.fggc, x1+x, chy-powh-y );
								XDrawPoint(obj->win.disp, obj->win.pix,
								obj->win.fggc, x1+x, chy-powh-y );
							}
							rest -= 1;
						}
					} /* dither */
				} /* y */
			} /* x */
		} /* ch */
	}

	fclose(fp);
	free(xi);
	free(xr);
	free(dbuff);
	free(mfbpf);
	free(mfcc);
	free(sbuff);
	if( obj->view.grayscale != 0 )
		sas_free_grayscale(obj, gcgray, GSCALE);
	return 0;
}

