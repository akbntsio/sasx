#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <math.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/Xutil.h>
#include "sas.h"
#include "child.h"
/*
   USE_POPEN
   コンパイル時に -DUSE_POPEN を指定すると、exec_child()の変わりに
   標準ライブラリ関数 popen() を使用します。
   ただし、fwrite を使うので、音の出だしが若干遅くなります。
   また、プロセスを削除することができないので、ボタンを離しても
   しばらく音が続きます。
*/

/*
    DAB_SIZE
    最大のバッファサイズ
*/
#define DAB_SIZE 4096

/*
    DA_PREDELAY (ms)
    DA の立上りに少し時間がかかるので、マーカ表示などを少し待たせる
*/
#ifndef DA_PREDELEAY
#define DA_PREDELAY  50
#endif
/*
    DA_POSTDELAY (ms)
    デバイスが解放されるのを少し待つ
*/
#ifndef DA_POSTDELAY
#define DA_POSTDELAY 10
#endif

/*
    sox play を使うとき
*/
#define SOXPLAY

#ifdef hpux
#include <sys/time.h>
int usleep(int usec)
{
	struct timeval timeout;
	timeout.tv_sec  = 0;
	timeout.tv_usec = usec;
	select(0,0,0,0,&timeout);
	return 0;
}
#endif

/* ------------------------------------------------------------------------
*   周波数選択フィルタ
*   FFT サイズ今のところ固定
*/
# define CH       2
# define FFTI    11               /* 4096/2/2*2=2048=2**11 */
# define FFTL    (2*DAB_SIZE/CH/sizeof(short))
# define POSI    30.0
# define RANG    40.0
static short     in[CH][FFTL]={{0},{0}};
static double    out[CH][FFTL]={{0},{0}};

/* フィルタのリセット */
void reset_filter()
{
	memset(in,0,sizeof(in));
	memset(out,0,sizeof(out));
}

/*
	１回の処理で FFTL(2chのときはFFTL/2)サンプルずつ入出力
	入力サンプルは１回遅れて出力される

	 < FFTL > < FFTL >
	|--------|            in
	         |--------|   sbuf <-- input
	|_,--~~~~~~~~~--,_|   real = (in sbuf) * haning
	         |--------|   in  = sbuf
	     (FFT,IFFT)
	|_,--~~~~~~~~~--,_|   real
	|~~~--,,_|            out
	|--------|            sbuf = real + sbuf --> out
	         |'''--,,_|   out  = real

*/
void freq_filter(
	short *sbuf,     /* 入出力バッファ */
	int    chan,     /* チャンネル数 */
	int    freq,     /* サンプリング周波数 */
	int    sfreq,    /* 開始周波数 */
	int    efreq,    /* 終了周波数 */
	int    mode)     /* 0:bandpass, 1:bandelim */
{
	double         real[2][FFTL*4];
	double         imag[2][FFTL*4];
	int            ch, i;
	int            ffti, fftl;
	int            flow, fhigh;     /* Hz */
	int            fftlow, ffthigh; /* FFT index */
	extern void    hanning(double*,int);
	extern void    fft(double*,double*,int,int);

	/* チャンネル数によって処理を変える */
	if( chan == 2 )
		{ ffti = FFTI; fftl = FFTL; } /* 2ch なら窓半分になる */
	else
		{ ffti = FFTI+1; fftl = FFTL*2; }

	/* FFTの範囲を選択 */
	flow = sfreq;
	fhigh = efreq;
	if( flow > fhigh ) { /* 逆にする */
		flow = flow + fhigh;
		fhigh = flow - fhigh;
		flow = flow - fhigh;
	}
	fftlow = fftl * flow / freq;
	ffthigh = fftl * fhigh / freq;
	if( ffthigh == fftlow ){
		if( ffthigh >= fftl/2 ) fftlow --;
		else ffthigh ++;
	}

	/* 周波数分解 */
	for(ch=0; ch<chan; ch++){
		for(i=0;i<fftl/2;i++){
			real[ch][i] = in[ch][i];                 /* 前フレーム */
			in[ch][i] = sbuf[i*chan+ch]; /* 保存 */
			real[ch][i+fftl/2] = (double)in[ch][i];  /* 今フレーム */
			imag[ch][i] = imag[ch][i+fftl/2] = 0.0;
		}
		hanning(real[ch],fftl);
		fft(real[ch],imag[ch],ffti,fftl);
	}

	/* 周波数によって帯域選択 */
	for(ch=0;ch<chan;ch++){
		real[ch][0] = imag[ch][0] = 0.0; /* DC CUT */
		real[ch][fftl/2] = imag[ch][fftl/2] = 0; /* niquist CUT */
		if( mode ){
			/* band-elim */
			for(i=1;i<fftlow; i++){
				real[ch][i] /= fftl;
				real[ch][fftl-i] /= fftl;
				imag[ch][i] /= -fftl;
				imag[ch][fftl-i] /= -fftl;
			}
			for(i=fftlow;i<ffthigh; i++){
				real[ch][i] = real[ch][fftl-i] = 0.0;
				imag[ch][i] = imag[ch][fftl-i] = 0.0;
			}
			for(i=ffthigh;i<fftl/2; i++){
				real[ch][i] /= fftl;
				real[ch][fftl-i] /= fftl;
				imag[ch][i] /= -fftl;
				imag[ch][fftl-i] /= -fftl;
			}
		}
		else {
			/* band-pass */
			for(i=1;i<fftlow; i++){
				real[ch][i] = real[ch][fftl-i] = 0.0;
				imag[ch][i] = imag[ch][fftl-i] = 0.0;
			}
			for(i=fftlow;i<ffthigh; i++){
				real[ch][i] /= fftl;
				real[ch][fftl-i] /= fftl;
				imag[ch][i] /= -fftl;
				imag[ch][fftl-i] /= -fftl;
			}
			for(i=ffthigh;i<fftl/2; i++){
				real[ch][i] = real[ch][fftl-i] = 0.0;
				imag[ch][i] = imag[ch][fftl-i] = 0.0;
			}
		}
	}

	/* 再合成 */
	for(ch=0; ch<chan; ch++){
		fft(real[ch],imag[ch],ffti,fftl);
		for(i=0; i<fftl/2; i++){
			sbuf[i*chan+ch] = (short)(out[ch][i] + real[ch][i]);
			out[ch][i] = real[ch][i+fftl/2];
		}
	}
}


/* ---------------------------------------------------------------
	再生するために、short として読み込む
	len  :  short データとしたときのバイト数(サンプル数じゃない)
*/
int  read_short(SasProp *obj, char *buff, int len, FILE *fp)
{
#if 0
	len = fread(buff,1,len,fp);
	if( obj->file.endian != obj->view.cpuendian )
		swapbyte(len,sizeof(short),buff);
	return len;
#else
	short   *sbuf = (short *)buff;
	int      i;

	if( obj->file.type == 'S' ){
		len = fread(buff,1,len,fp);
		if( obj->file.endian != obj->view.cpuendian )
			swapbyte(len,sizeof(short),buff);
		return len;
	}

	/* smaller datatype */
	else if( obj->file.type == 'C' ) {
		len = fread(buff,1,len/sizeof(short),fp);
		for( i=len-1; i>=0; i-- )
			sbuf[i] = buff[i] * 256;
		return len*sizeof(short);
	}
	else if( obj->file.type == 'U' ){
		unsigned char *ubuf = (unsigned char *)buff;
		len = fread(buff,1,len/sizeof(short),fp);
		for( i=len-1; i>=0; i-- )
			sbuf[i] = (ubuf[i] - 128) * 256;
		return len*sizeof(short);
	}

	/* larger datatype */
	else if( obj->file.type == 'L' ){
		long  *lbuf = (long *)buff;
		float gain = 65536.0 / (obj->view.ymax - obj->view.ymin);
		len = fread(buff,sizeof(long),len/sizeof(short),fp);
		for( i=0; i<len; i++ )
			sbuf[i] = (short)(lbuf[i] * gain);
		return len*sizeof(short);
	}
	else if( obj->file.type == 'F' ){
		float  *fbuf = (float *)buff;
		float gain = 65536.0 / (obj->view.ymax - obj->view.ymin);
		len = fread(buff,sizeof(float),len/sizeof(short),fp);
		for( i=0; i<len; i++ )
			sbuf[i] = (short)(fbuf[i] * gain);
		return len*sizeof(short);
	}
	else if( obj->file.type == 'D' ){
		double  *dbuf = (double *)buff;
		float gain = 65536.0 / (obj->view.ymax - obj->view.ymin);
		len = fread(buff,sizeof(double),len/sizeof(short),fp);
		for( i=0; i<len; i++ )
			sbuf[i] = (short)(dbuf[i] * gain);
		return len*sizeof(short);
	}
	else
		return 0;
#endif
}

/* ---------------------------------------------------------------
*   再生プロセス。いや、プロセスは作っていない。
*
*/
/* 再生プロセスが消えた事を受け付ける */
static pid_t  pid=0;		/* プロセス番号 */
static void letmeknow() {wait(0);pid=0;}

/*
	ボタン３を押している間再生する
	ボタンを離したらできるだけすぐに止まる
	セレクトされた始点から再生する
	セレクトされた終点に来たら止まる
	セレクト範囲が0ならファイルの終りで止まる
	これはとりあえずバージョンです：
		スリープ時間が適当なので周波数によってはダメ
		バイトスワップしない
		コマンドが替えられない

	基本的に short データを扱うことにしているが、
	後付けで char double なども扱うが、
	short データでのバイト数で制御しているので
	コードがキタナイ. 書き直したい... 2003.9
*/

void da(SasProp *obj)
{
#ifdef USE_POPEN
	static FILE   *pfp=0; ///
#else
	static int    pfd[4]; ///		/* プロセスとのやりとり用 */
#endif /* USE_POPEN */
	static FILE   *fp = 0;
	static int    start, size;      /* 出力サンプル数(chあたり) */
	static int    rest=0;           /* 残りバイト数 */
	static int    passed=0;         /* プロセスに送ったバイト数 */
	static int    poolmax=0;        /* プロセス側に貯めておくバイト数 */
	static int    trans_size=0;     /* 1回の転送バイト数 */
	int           played;           /* 音の出たバイト数(時間から逆算) */
	char          command[128];
	int           len;
	char          buff[DAB_SIZE*sizeof(double)/sizeof(short)];
									/* 再生用バッファ/読み込みバッファ兼用 */
	static int    marker=(-1), markerstop;
	int           ms;
	extern long   get_msec(int);
	int           freq, chan;
	int           chunit;           /* ファイルの１サンプルのバイト数 */
	int           playunit;         /* 再生出力の１サンプルのバイト数 */
	int           status;
	int           loop;

	if( ! obj ) return;
	if( ! strchr("UCSLFD", obj->file.type) ) return;
	/* Ascii, Plot の時は再生しない */

	freq = obj->file.freq;
	chan = obj->file.chan;
	chunit = obj->file.chan*obj->file.unit;
	playunit = obj->file.chan*sizeof(short);
	if( chan > 2 ) {chan = 1; freq *= chan;}
		/* 3チャンネル以上は再生できないのでモノラルでスピードだけ合わせる */
	loop = ( obj->view.daloop || global_shift )? 1 : 0;

	if( obj->view.b3press || global_alt ){
		if( ! obj->view.daon ){
			/* 起動 押した時だけ */
#if defined(hpux)
			sprintf(command,"splayer -l16 -srate %d",
					obj->file.freq);
#elif defined(ALSA)
			sprintf(command,"aplay -t raw -f S16_LE -r %d -c %d -q",
					obj->file.freq, chan);
#elif defined(SOXPLAY)
			sprintf(command,"play -t s16 -r %d -c %d -q -",
					obj->file.freq, chan);
#else /* not hpux not soxplay */
				sprintf(command,"da -f%g -c%d -n",
					(float)obj->file.freq/1000, chan);
#endif /* hpux */
			/* fprintf(stderr,"command: %s\n", command); */

#ifdef USE_POPEN
			pfp = popen(command, "w");
			if( ! pfp ) return;
#else
			signal(SIGCHLD,letmeknow);
			pid = exec_command(pfd,command,"w");
			if( pid <= 0 ) return; /* fork失敗 */
#endif /* USE_POPEN */

			/* 入力ファイル準備 */
			fp = fopen(obj->file.name, "r");

			/* 再生する範囲を決定 */
			start = obj->view.ssel;
			size = obj->view.nsel;
/*
			if( obj->view.shift ){
				start = obj->view.sview;
				size = obj->view.nview;
			}
			else {
				start = obj->view.ssel;
				size = obj->view.nsel;
			}
*/
			/* 逆転してたら前向きに修正など */
			if( size < 0 ) { start += size; size = -size; }
			if( start < 0 ) start = 0;
			if( start > obj->file.size ) start = 0; /* 2002.2.20 */
			if( size == 0 || start + size > obj->file.size )
				size = obj->file.size - start;

			/* PLAY 表示 */
			sas_markerstr(obj,"PLAY");
			obj->view.daon = 1;

			/* 転送サイズは 100ms のバイト数 */
  			trans_size = freq * chan * sizeof(short) / 16; /* 1/16s */ 
  			if( trans_size > DAB_SIZE ) trans_size = DAB_SIZE;

			/* 送り出しバッファサイズは 500ms のバイト数 */
			poolmax = freq * chan * sizeof(short) / 2;  /* 1/4s */
//			if( poolmax < trans_size*2 ) poolmax = trans_size*2;

			/* フィルタリングの準備 */
			if( obj->view.mode != MODE_WAVE && obj->view.fsel ){
				/* 手抜きコードな為 バッファサイズ固定してしまう。*/
				trans_size = DAB_SIZE;
				poolmax = trans_size * 2;

				/* 読みだし１回ぶん前に戻り、２回フィルタ処理で空回り */
				reset_filter();
				if( start > 0 ){
					len = trans_size;
					if( start*playunit > len ){
						/* char uchar のためちょっとここ汚い */
						long pos = obj->file.hsize
						     + (obj->file.offset + start) * chunit
						     - len * obj->file.unit / sizeof(short);
						fseek(fp, pos, 0);
					}
					else {
 						len  = start*playunit;
						memset(buff,0,trans_size-len);
					}
					read_short(obj, buff+(trans_size-len), len, fp);
					freq_filter((short*)buff, obj->file.chan, 
						obj->file.freq, obj->view.sfreq, obj->view.efreq, global_ctrl);
				}
				len = read_short(obj, buff, trans_size, fp);
				freq_filter((short*)buff, obj->file.chan, 
					obj->file.freq, obj->view.sfreq, obj->view.efreq, global_ctrl);
			}
			else {
				/* 読みだし開始位置 */
				long pos = obj->file.hsize
				         + (obj->file.offset + start) * chunit;
				fseek(fp, pos, 0);
			}

			/* 再生すべき残りバイト数 */
			rest = size * playunit;
			markerstop = 0;
			passed = 0;
			played = 0;

			/* マーカ表示 */
			marker = start;
			sas_marker(obj, marker);

			usleep(1000*DA_PREDELAY); /* 起動をちょっと待つ */
			get_msec(0);              /* 時刻計測スタート */

		} /* 起動時の処理おわり */

		/* 空回り */
		/* ボタンは押されているが、再生は終っている時 */
		if( ! obj->view.daon ) return;

		/* 音の出た時間からバイト数を推測 */
		/* write する前に測定を始めているが */
		/* カーソルが音より数ms先に動くのは許容されるだろう */
		/* ms * freq .. とやるとオーバーフローするので分ける */
		ms = get_msec(1);
		played = (ms/1000) * freq * playunit;
		played += (ms%1000) * freq * playunit / 1000;

		/* マーカを動かす */
		if( ! markerstop ) {
			/* マーカ消去 */
			sas_marker(obj, marker);
			/* マーカ表示 */
			marker = start + played / playunit;
			if( marker >= start + size ) {
				if( loop ){
					marker = start + ((played / playunit)%size);
				}
				else {
					marker = start + size;
					markerstop = 1;
				}
			}
			sas_marker(obj, marker);
		}

		/* 待ち合わせ */
		/* poolmax(500ms?)余裕があれば転送しないで5ms待ってリターンする */
		/* 転送しすぎてリアルタイム性がなくなるのを防ぐ */
		if( passed - played > poolmax || rest == 0 ){
			usleep(5000);
			return;
		}

		/* データの読み込みとプロセスへの書き込み(バイト単位) */
		if( rest > 0 ) {
			len = (rest>trans_size)?trans_size:rest;
			len = read_short(obj, buff, len, fp);
			if( len < trans_size )
				memset(buff+len, 0, trans_size-len);

			/* 周波数範囲を選択 */
			if( obj->view.mode != MODE_WAVE &&
			    obj->file.chan <= 2 && obj->view.fsel == 1 ) {
				freq_filter((short*)buff, obj->file.chan, 
					obj->file.freq, obj->view.sfreq, obj->view.efreq, global_ctrl);
				/* filter 使う場合、転送１回分ずれて出力される */
				/* rest は１回多く残しているので、EOFまで選択した場合、EOFからはみ出して読み出す */
				/* 最後にEOFからはみ出しても処理つづけてもらう */
				len = trans_size;
			}

			int pushed = 0;
			while( pushed < len ) {
				int wrote = 0;
#ifdef USE_POPEN
				wrote = fwrite(buff,1,len,pfp);
				fflush(pfp);
#else /* not USE_POPEN */
				wrote = write(pfd[1],buff,len);
#endif /* USE_POPEN */
				if( wrote < 0 ) break;
				pushed += wrote;
			}
			rest -= len;
			passed += len;

			/* 再生範囲の終りに来たら */
			if( rest <= 0 ){
				if( loop ){
					/* ループの時は最初へ戻る */
					/* 読みだし開始位置 */
					long pos = obj->file.hsize
				         	+ (obj->file.offset + start) * chunit;
					fseek(fp, pos, 0);
					rest = size * playunit;
				} else {
					/* ループしない時は終る */
					/* close しないと最後まで再生しない */
#ifdef USE_POPEN
					if( pfp ) pclose(pfp);
					pfp = 0;
#else /* not USE_POPEN */
					close(pfd[1]);/* たぶんプロセスは終る */
#endif /* USE_POPEN */
					usleep(1000*DA_POSTDELAY); /* 念のためデバイス解放時間 */
					fclose(fp);
					fp = 0;
					rest = 0;
					sas_markerstr(obj,"PLAY");
					sas_markerstr(obj,"");
				}
			}
		}
	}
	else {
		if( ! obj->view.daon ) return;
		/* 離したときに一回だけ */
		if( rest > 0 ){
#ifdef USE_POPEN
			if( pfp ) pclose(pfp);
			pfp = 0;
			usleep(1000*(passed-played)/playunit*1000/freq); /* 一応待つ */
#else
			close(pfd[1]);
			if( pid > 0 ) kill(pid,SIGHUP); /* なるべく早く止まるように */
#endif /* USE_POPEN */

			fclose(fp);
			fp = 0;
			sas_markerstr(obj,"PLAY");
			sas_markerstr(obj,"");
		}
		wait(&status); /* 一応ここでブロックしておく */
		usleep(1000*DA_POSTDELAY); /* 念のためデバイス解放時間 */
		rest = 0;
		sas_marker(obj, marker);
		marker = -1;
		sas_marker(obj, marker);
		obj->view.daon = 0;
	}
/*
	b3press : 0->1                   1->0
	daon :         0->1                   1->0
	rest : samples rest      0:
	marker : marker position  -1:not shown
	markerstop : stop marker
	
*/
}


/*
	mode = 0	リセット
	mode = 1	リセットしてからの経過時間(msec)
*/
#include <sys/time.h>
long get_msec(int mode)
{
	static struct timeval tv0;
	struct timeval tv;
	struct timezone tz;

	if( mode == 0 ){
		gettimeofday( &tv0, &tz );
		return(0);
	}
	else {
		gettimeofday( &tv, &tz );
		return (tv.tv_sec - tv0.tv_sec)*1000
				 + (tv.tv_usec - tv0.tv_usec)/1000;
	}
}

/*
 * HANNING 窓
 */
void hanning(double *x, int len)
{
	int i;
	static int wsize=0;
	static double *win=0;
	if( len != wsize ){
		if( win ) free(win);
		if( ! (win=malloc(sizeof(double)*len)) )
			return;
		wsize = len;
		for( i=0; i<len; i++ )
			win[i] = 0.5 - 0.5 * cos(i*3.14159265*2/(len-1));
	}
	for( i=0; i<len; i++ )
		x[i] *= win[i];
}

