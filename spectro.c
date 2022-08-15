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
#include "plot.h"

#if 0
int sas_draw_pitch(SasProp* obj)
{
	if( obj->view.pitchfile[0] == 0 ){
		return sas_draw_pitch_auto(SasProp* obj);
	}
	else{
		return sas_draw_pitch_file(SasProp* obj);
	}
}

int sas_draw_pitch_file(SasProp* obj)
{
}
#endif

/* グレースケールを作成 */
void sas_create_grayscale(SasProp *obj, GC *gcgray, int num)
{
	int       i;
	int  r, g, b;
	char cname[16];
	for( i=0; i<num; i++ ){
		r = g = b = 255 - 256 * ((float)i/num);
		sprintf(cname, "#%02x%02x%02x", r, g, b);
		gcgray[i] = sas_make_gc(obj, cname);
	}
}

/* カラースケールを作成 */
void sas_create_colorscale(SasProp *obj, GC *gcgray, int num)
{
	int       i;
	int  r, g, b;
	char cname[16];

	/* カラースケール適当 */
	for( i=0; i<num; i++ ){
		if( i < num/4 ){
			b = i * 4 * 256 / num;
			r = g = 0;
		} else if( i < num/2 ){
			b = 255;
			g = i * 4 * 256 / num - 256;
			r = 0;
		} else if( i < num*3/4 ){
			b = 255;
			/*g = 767 - i * 4 * 256 / num;*/
			g = 511 - i * 2 * 256 / num;
			r = i * 4 * 256 / num - 512;
		} else {
			b = 1023 - i * 4 * 256 / num;
			/*g = i * 4 * 256 / num - 768;*/
			g = i * 2 * 256 / num - 256;
			r = 255;
		}
		sprintf(cname, "#%02x%02x%02x", r, g, b);
		gcgray[i] = sas_make_gc(obj, cname);
	}
}

void sas_free_grayscale(SasProp *obj, GC *gcgray, int num)
{
	int      i;
	for(i=0;i<num;i++)
		sas_free_gc(obj, gcgray[i]);
}


/*
	ピッチ分析をする
	４倍程度のアップサンプルで細かく見る
	遅い
*/
int sas_draw_pitch(SasProp* obj)
{
	FILE   *fp;
	int     i, j, k, size, skip, fftsize, ffti, len, swap, nchan;
	int     ifftsize, iffti, up, lcc;
	int     pmin, pmax, cmax;
	int     y1, y2, yr, y, lblh, powh;
	int     x1, x2, x;
	double *dbuff;
	double *xr, *xi;
	double  root, pit, ratio;
	// double  gain, tau, f0;
	int     ch, chy;
	extern float   Mel(float);
	extern float   MelInv(float);

#	define  MAXPIT   (5)
	float   pitchtau[MAXPIT*2];
	float   pitchratio[MAXPIT*2];
	float   pitchrec[MAXPIT];
	float   pitchrecratio[MAXPIT];
	// float   pitchcont[MAXPIT];
	int     npit;
	int     overextract = 0;    /* 候補を多めに抽出 */
	GC      gc;
	static char  *color[MAXPIT+6] =
		{"#ff0000","#00ff00","#0000ff","#ffff00","#00ffff","#ff00ff"};
	static char  *solid = "\012\002\377\377";

	/* ピッチリセット */
	if( obj->view.pitchnum > MAXPIT )
		obj->view.pitchnum = MAXPIT;
	for(i=0;i<MAXPIT;i++)
		pitchrec[i] = pitchrecratio[i] = 0;

	/* 分析対象ファイル */
	if( (fp=fopen(obj->file.name,"r"))==0 ){
		fprintf(stderr,"can't open %s\n",obj->file.name);
		return -1;
	}

	swap   = (obj->file.endian == obj->view.cpuendian)? 0: 1;
	nchan = obj->file.chan;
	lblh = (obj->nlbl)?obj->win.lblh: 0;
	powh = (obj->view.power)? obj->view.powerh: 0;
	x1 = obj->win.scalew;
	x2 = obj->win.size.width - 1;
	(void)x2;
	y1 = obj->win.scaleh;
	y2 = obj->win.size.height - obj->win.scaleh - lblh;
	yr = (y2 - y1)/nchan;


	// gain = 65536.0 / (obj->view.ymax - obj->view.ymin);
	size = (int)(obj->file.freq / obj->view.pitchmin * 2); /* window size */
	if( size < 2 ) size = 2;
	up   = obj->view.pitchup;              /* cepstrum oversample rate */
	skip = obj->file.freq * obj->view.pitchsft / 1000; /* samples */
	if( skip < 1 ) skip = 1;
	pmax = (int)(obj->file.freq / obj->view.pitchmin); /* max tau */
	pmin = (int)(obj->file.freq / obj->view.pitchmax); /* min tau */
	if( obj->view.pitchroot > 0.0 ) root = 1.0/obj->view.pitchroot;
	else root = 1/3.0;

	for( ffti=2, fftsize=4; fftsize<size; ffti++, fftsize*=2 );
	for( iffti=2, ifftsize=4; ifftsize<fftsize*up; iffti++, ifftsize*=2 );

	/* 不要な帯域をカット */
	/* 以前は高域もカットしていたが、閾値ではなく徐々に減衰することにした */
	/* 低域は台形窓でカット */
	lcc  = (int)((float)fftsize * obj->view.pitchmin / obj->file.freq);

/*
	printf("size %d fft %d ifft %d skip %d pmax %d pmin %d lcc %d\n",
	size,fftsize,ifftsize,skip,pmax,pmin,lcc);
*/

	/* ワークメモリ */
	dbuff = malloc(sizeof(double) * nchan * size);
	xr = malloc(sizeof(double)*fftsize*2*up);
	xi = malloc(sizeof(double)*fftsize*2*up);
	if( !dbuff || !xr || !xi ){
		fprintf(stderr,"can't alloc fft buffers\n");
		fclose(fp);
		return -1;
	}

	gc = sas_make_gc(obj, color[0]);

	/* 分析フレーム(j)ループ */
	for( j=0,i=obj->view.sview;
	     j<obj->view.nview+skip/2 && i<obj->file.size;
	     j+=skip,i+=skip ){

		/* i サンプルを中心とした size サンプルのデータを読む */
		len = read_double(fp, obj->file.hsize, obj->file.offset,
		                  obj->file.type, nchan, swap,
		                  i-size/2, size, dbuff);
		if( len < size ) continue;

		/* 各チャンネルループ */
		for( ch=0; ch<nchan ; ch++ ){
			float  dmin = 10000;      /* 不連続性最小になった距離指標 */
			int    minslot[MAXPIT];   /* 不連続性最小になった順序 */

			/* チャンネル抽出 */
			for( k=0; k<size; k++ )
				xr[k] = dbuff[nchan*k+ch];
			for(    ; k<fftsize; k++ )
				xr[k] = 0;
			for( k=0; k<fftsize; k++ )
				xi[k] = 0;

			/* 窓掛け */
			hamming(xr, size);

			/* 周波数領域に変換 */
			fft( xr, xi, ffti, fftsize );

			/* 3乗根パワーの計算 */
			for( k=0; k<fftsize/2; k++ ){
				xr[k] = xr[k]*xr[k] + xi[k]*xi[k];
				if( obj->view.pitchroot <= 0.0 )
					xr[k] = log(xr[k]);
				else
					xr[k] = pow(xr[k], root);
					/* おすすめ root=0.33 */
			}

			/* 高域を減衰 */
			/* (ピッチ変動が周波数に比例して大きく不安定) */
			/* カットしていたのを軟らかくした */
			/* 謎の数字が入っているが、チューニングは不完全である */
			for( k=1; k<fftsize/2; k++ )
				/*xr[k] *= (float)(fftsize/2-k)/(fftsize) + 0.5;*/
				xr[k] *= pow(1.0/((float)k),0.3);

			/* 低域除去 */
			/* lcc まで0にしてしまうのはゴミをひろいやすかったので台形に */
			for( k=0; k<lcc; k++ ) xr[k] *= (float)k/lcc;

			/* ケプストラム */
			for( k=0; k<fftsize/2; k++ )
				xr[ifftsize-k-1] = xr[k];
			for( k=fftsize/2; k<ifftsize-fftsize/2; k++ )
				xr[k] = 0;
			for( k=0; k<ifftsize; k++ )
				xi[k] = 0;
			fft( xr, xi, iffti, ifftsize );

#if 0
			/* 長いピッチに下駄はかせる */
			/* 不要になった 2003.5 */
			for( k=1; k<ifftsize/2; k++ )
				xr[k] *= pow((double)k/ifftsize*2,1)+0.3;
#endif


			/* ピッチ指標を求める */
			/* 半ピッチの値(の半分)との差を使う事にした */
			/* サイン波などのピッチが正確にとれるように */
			if( xr[0] < 1 ) xr[0] = 1;      /* 割算の安全のために */
			for( k=1; k<ifftsize/2; k++ ){
				xi[k] = xr[k] - xr[k/2]*0.5 - xr[k/3]*0.5;
				if( obj->view.pitchroot <= 0.0 )
					xi[k] *= 10.0;
			}

			/* 複数ピッチを抽出 */
			for( npit=0; npit<obj->view.pitchnum+overextract; npit++ ){
				/* ピーク検出 */
				cmax = pmin*up;
				for( k=pmin*up; k<pmax*up; k++ ){
					if( xi[cmax] < xi[k] )
						cmax = k;
				}
				pitchtau[npit] = cmax;
				pitchratio[npit] = xi[cmax] / xr[0];
				/* このピッチは消しとく */
				for( k=cmax-cmax*up/16; k<=cmax+cmax*up/16; k++ )
					xi[k] = 0;
			}

			/* 初めての時 */
			if( pitchrec[0] == 0 ){
				for( npit=0; npit<obj->view.pitchnum; npit++ )
					pitchrec[npit] = pitchtau[npit];
				pitchrecratio[npit] = pitchratio[npit];
			}

			/* pitchnum ** pitchnum の組合せから */
			/* ピッチ不連続性が最小になるピッチ候補の順序づけ */
			/* ここは表示の一貫性のため */
			/* 順序       posi        0    1    2    3    4   */
			/* ピッチ順位 slot[i]     3    1    4    0    2   */
			/* ピッチ候補 pitchtau[i] a    b    c    d    e   */
			if( obj->view.pitchnum > 1 ) {
				int    num, slot[MAXPIT], posi;
				num=obj->view.pitchnum;
				if (num > MAXPIT) num = MAXPIT;
				for(posi=0;posi<num;posi++) slot[posi]=0;
				posi=0;
				/* 各ピッチ候補のスロットへの割り当て。全組合せを調べる */
				while(1){
					/* 全部の組合せを調べたら終る */
					if(slot[0]>=num) break;
					/* 桁あげ */
					if(slot[posi]>=num){ slot[posi]=0; posi--; slot[posi]++; }
					/* 最後の桁までループの中で決めてゆく */
					else if(posi<num-1){ posi++; }
					/* 最後の桁まで決まったらテストする */
					else {
						int     l, same = 0;
						/* 重複のある組合わせは除外し、次の組合せへ */
						for(k=0; k<num; k++)
							for(l=k+1; l<num; l++)
								if(slot[k]==slot[l]){same=1;break;}
						if(same==0){
							/* 重複のない組合せなら不連続性 dd を調べる */
							float  d, dd = 0;
							for(npit=0; npit<num; npit++){
								/* 過去と現在の低い方の倍数で示す 1 < d */
								d = pitchrec[npit] - pitchtau[slot[npit]];
								if( d >= 0 ) d = d / pitchrec[npit];
								else         d = -d / pitchtau[slot[npit]];
								/* ピッチ強度に応じて距離が大きい */
								dd += pitchrecratio[npit] * d;
								/* ピッチ強度にも連続性を */
								d = pitchrecratio[npit] - pitchratio[slot[npit]];
								if( d < 0 ) d = -d;
								dd += d * 0.5;
							}
							/* より不連続の少ないものに順序を書き換える */
							if( dd < dmin ){
								dmin = dd;
								for(npit=0;npit<num;npit++)
									minslot[npit] = slot[npit];
							}
						}
						/* 次の組合せを試す */
						slot[posi]++;
					}
				}
				/* 割当が決まったら 過去ピッチを更新する */
				for( npit=0; npit<num; npit++ ){
					/* 今回の値に置き換えて更新する */
					/* pitchrec[npit] = pitchtau[minslot[npit]]; */
					/* pitchrecratio[npit] = pitchratio[minslot[npit]]; */
					/* 若干履歴を残して更新する */
					pitchrec[npit] = pitchrec[npit] * 0.0
					               + pitchtau[minslot[npit]] * 1.0;
					pitchrecratio[npit] = pitchrecratio[npit] * 0.0
					               + pitchratio[minslot[npit]] * 1.0;
					//pitchcont[npit] = dmin;
				}
			}
			else {
				/* 1個だけなら、並び替えない */
				pitchrec[0] = pitchtau[0];
				//pitchcont[0] = 0;
				pitchrecratio[0] = pitchratio[0];
				minslot[0] = 0;
			}

			/* 表示 */
			for( npit=0; npit<obj->view.pitchnum; npit++ ){
				/* 色を替える */
				sas_change_gc(obj,gc,color[npit],solid);

				/* 判定 */
				ratio = pitchratio[minslot[npit]];
				if( ratio > obj->view.pitchthr ){
					pit = (double)up/pitchtau[minslot[npit]]; /* 周波数比 */
					//tau = pitchtau[minslot[npit]]/up;   /* 周期(サンプル) */
					//f0  = obj->file.freq/tau;        /* 周波数[Hz] */
				}
				else{
					pit = 0;
					//tau = 0;
					//f0  = 0;
				}

				/* 表示 ピッチなければ 0 Hzに表示 */
				chy = y1 + yr * (ch+1);
				x = (int)(j/obj->view.spp);
				if( obj->view.fscale == FSC_MEL ){
					y = (int)((float)(yr-powh)
						* Mel((float)obj->file.freq*pit)
						/ Mel((float)obj->file.freq/2/obj->view.fzoom));
				}
				else
					y = (int)((float)(yr-powh)*2*pit*obj->view.fzoom);
				if( pit > 0 ){
					XDrawRectangle(obj->win.disp, obj->win.win,
						gc, x1+x-1, chy-powh-y-1, 3,3);
					XDrawRectangle(obj->win.disp, obj->win.pix,
						gc, x1+x-1, chy-powh-y-1, 3,3);
				}

				/* 指標の表示 0-100Hzにマップ*/
				y = (int)((float)(yr-powh)*ratio*200*obj->view.fzoom/obj->file.freq);
				XDrawRectangle(obj->win.disp, obj->win.win,
					gc, x1+x-1, chy-powh-y-1, 1,1);
				XDrawRectangle(obj->win.disp, obj->win.pix,
					gc, x1+x-1, chy-powh-y-1, 1,1);
			} /* npit */
		} /* ch */
	}
	sas_free_gc(obj,gc);
	fclose(fp);
	free(xi);
	free(xr);
	free(dbuff);

	return 0;
}

/*
	スペクトログラムを描画
	ハードコピーが同じになるよう誤差分散法を使って白黒２値でグレー表示

	fs	256なら		128なら		64なら
	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	 8k	32ms/31Hz	16ms/62.5Hz	8ms/125Hz
	12k	21ms/46.8Hz	10.7ms/93.7Hz	5.3ms/187Hz
	16k	16ms/63Hz	8ms/125Hz	4ms/250Hz

*/
int sas_draw_spectrogram(SasProp* obj)
{
	FILE *fp;
	int   i, j, k, size, skip, fftsize, ffti, len, nchan, swap;
	int   y1, y2, yr, y, lblh, powh;
	int   x1, x2, x, xstart, xlast;
	double *dbuff;
	double *xr, *xi, gain, power=0;
	double *xro;
	float rest, val;
	double spow  = obj->view.spow;
	double sgain = obj->view.sgain;
	float pre   = obj->view.pre;
	int   ch, chy;
	extern float   Mel(float);
	extern float   MelInv(float);
	float   maxfreq;

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


	sgain = 1.0 / pow(sgain,spow*2);
	maxfreq = obj->file.freq/2/obj->view.fzoom;
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

	if( obj->view.narrow ){
		size = (int)(obj->file.freq * obj->view.framesizenb / 1000);
		skip = (int)(obj->file.freq * obj->view.frameskipnb / 1000);
	} else {
		size = (int)(obj->file.freq * obj->view.framesizewb / 1000);
		skip = (int)(obj->file.freq * obj->view.frameskipwb / 1000);
	}

	/* 全然進まないと困るので 2000.12.8 */
	if( size < 1 ) size = 1;
	if( skip < 1 ) skip = 1;

	for( ffti=7, fftsize=128; fftsize<size*(2); ffti++, fftsize*=2 );
	/* スペクトルをスムーズに見せるためにFFTは大きめにしています */
/*
	printf("narrow %d zoom %g size %d skip %d fft %d\n",
	obj->view.narrow,obj->view.fzoom,size,skip,fftsize);
*/

	dbuff = malloc(sizeof(double)*nchan*size);
	xr    = malloc(sizeof(double)*fftsize*2);
	xi    = malloc(sizeof(double)*fftsize*2);
	xro   = malloc(sizeof(double)*fftsize*2);
	if( !dbuff || !xr || !xi ){
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
			/* ゲイン＋高域強調 */
			xr[0] = gain * dbuff[ch];
			for( k=1; k<size; k++ )
				xr[k] = gain * (dbuff[nchan*k+ch] - pre * dbuff[nchan*(k-1)+ch]);
			for(    ; k<fftsize*2; k++ )
				xr[k] = 0;
			for( k=0; k<fftsize*2; k++ )
				xi[k] = 0;

			/* 窓掛け */
			hamming(xr, size);

			/* パワーを表示するなら計算する */
			if( obj->view.power ){
				double dc;
				for( dc=0,power=0,k=0; k<size; k++ ){
					power += xr[k]*xr[k];
					dc += xr[k];
				}
				dc /= size;
				power = power / size - dc*dc;
				/* スペクトルより若干飽和レベルを大きめに */
				power = pow(power,spow) * sgain * 0.8;
				if( power > 1.0 ) power = 1.0;
			}

			/* 周波数領域に変換 */
			fft( xr, xi, ffti, fftsize );

			/* 対数パワーの計算(ver1.3から約３乗根) */
			for( k=0; k<fftsize/2; k++ ){
				xr[k] = xr[k]*xr[k] + xi[k]*xi[k];
				xr[k] /= size;
				xr[k] = pow(xr[k],spow) * sgain;
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
				int ko=0;
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

					/* FFT バンド(k)のどれが y の位置にあたるか */
					if( obj->view.fscale == FSC_MEL ){
						float melf = Mel((float)maxfreq) * y / (yr-powh);
						k = fftsize * MelInv(melf) / obj->file.freq;
					}
					else
						k = fftsize/2/obj->view.fzoom * y/(yr-powh);
					/* val = xr[k]; */
					if(obj->view.specdiff) {
						val = xr[k]-xro[k];
						if(val<0)val=-val;
						val *= 4;
						if(k!=ko){
							xro[ko] = xr[ko];
							ko=k;
						}
					}
					else val = xr[k];

					/* 点を打つ */
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
	free(xro);
	free(dbuff);
	if( obj->view.grayscale != 0 )
		sas_free_grayscale(obj, gcgray, GSCALE);
	return 0;
}

/*
 * HAMMING 窓
 */
void hamming(double *x, int len)
{
	int i;
#ifdef ANA_THREAD
	for( i=0; i<len; i++ )
		x[i] *= 0.54 - 0.46 * cos(i*3.14159265*2/(len-1));
#else /* ANA_THREAD */
	static int wsize=0;
	static double *win=0;
	if( len != wsize ){
		if( win ) free(win);
		if( ! (win=malloc(sizeof(double)*len)) )
			return;
		wsize = len;
		if( len == 1 )
			win[0] = 1;
		else
			for( i=0; i<len; i++ )
				win[i] = 0.54 - 0.46 * cos(i*3.14159265*2/(len-1));
	}
	for( i=0; i<len; i++ )
		x[i] *= win[i];
#endif /* ANA_THREAD */
}


/*
 * FFT (in place)
 *
 *  最後の２ステージを基数４と基数２のDFTで求めるので、かなり速いはず
 *  そのかわり４ポイント以下のFFTは実行できない
 *
 *  double version       Aug. 31, 1994
 *  index from 0 version July. 1, 1998
 */

#ifndef M_PI
#define M_PI 3.14159265358979
#endif

/*
 *	xr[wsize*2]	入力および出力の実部
 *	xi[wsize*2]	入力および出力の虚部
 *	index		n = 2**index;
 *	wsize		2**index
 */
void fft(double *xr, double *xi, int index, int wsize)
{
	double   PI=M_PI;
	double   ur,ui,d,wr,wi,tr,ti;
	double   a,b;
	int      le1,l,le,j,i,nv2,k,ip;
	double   cos(),sin();

	/*   radix-2  fft    */
	le1=wsize;
	for(l=1 ; l<index-1  ; l++) {
		le=le1;                 /* skipping period */
		le1=le/2;               /* skipping period/2 */
		d=PI/le1;               /* rotation angle */
		wr=cos((double)(d));    /* rotation coef. diagonal */
		wi=(-sin((double)(d))); /* rotation coef. other */
		ur=1.0;
		ui=0.0;
		for(j=0;j<le1;j++){     /* offset */
			for(i=j;i<wsize;i+=le) { /* block skip */
				ip=i+le1;       /* next block index */
				tr=xr[ip];
				ti=xi[ip];
				a=xr[i]-tr;
				b=xi[i]-ti;
				xr[ip]=a*ur-b*ui; /* rotate */
				xi[ip]=a*ui+b*ur;
				xr[i]+=tr;
				xi[i]+=ti;
			}
			a=ur*wr-ui*wi;
			ui=ur*wi+ui*wr;     /* rotation for the next block */
			ur=a;
		}
	}

	/*  before the last stage
 	*   4-point DFT
 	*/
	for(j=3;j<wsize;j+=4) {
		tr=xr[j-3];
		ti=xi[j-3];
		xr[j-3]=tr+xr[j-1];
		xi[j-3]=ti+xi[j-1];
		xr[j-1]=tr-xr[j-1];
		xi[j-1]=ti-xi[j-1];
		tr=xr[j-2];
		ti=xi[j-2];
		xr[j-2]=tr+xr[j];
		xi[j-2]=ti+xi[j];
		tr-=xr[j];
		ti-=xi[j];
		xr[j]=ti;
		xi[j]=(-tr);
	}

	/* the last stage
	*  2-point DFT
 	*/
	for(j=1;j<wsize;j+=2) {
		tr=xr[j-1];
		ti=xi[j-1];
		xr[j-1]=tr+xr[j];
		xi[j-1]=ti+xi[j];
		xr[j]=tr-xr[j];
		xi[j]=ti-xi[j];
	}

	/*  bit reversal
 	*/
	for(nv2=wsize/2,j=0,i=0; i<wsize-1 ;i++) {
		if(i<j) {
			tr=xr[j];
			ti=xi[j];
			xr[j]=xr[i];
			xi[j]=xi[i];
			xr[i]=tr;
			xi[i]=ti;
		}
		k=nv2;
		while(k<=j) {
			j-=k;
			k/=2;
		}
		j+=k;
	}
}
