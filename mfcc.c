/*
    V1.0(2001/06/07)
*/
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include  <fcntl.h>

#define PI M_PI
#define EPS 1.0e-12

int    fbankpow = 0;	/* 0:raw wave energy, 1:filter-bank sum */
int    fbanknormalize = 0;	/* 1:mfcc tilt equalization */

typedef struct{
   int    fbank_num;    /* filterbank size */
   int    fft_size;     /* fft size */
   int    n;            /* log2(fft size) */
   int    lo,hi;        /* low and high frequency cut-off */
   float  f;            /* frequency */
   float *cent;         /* centre frequencies */
   short *lo_index;     /* low channel index */
   float *lo_weight;    /* low channel weight */
   float *Re;           /* fft (real part) */
   float *Im;           /* fft (imag part) */
   float *wgt;          /* normalize weight */
}Fbank;

void Wav2MFCC_E_D(short *wave, float *mfcc, long sampsize,long sfreq
		, int fbank_num, int raw_e, float preEmph, int mfcc_dim
		, int lifter, float *mfbpf);
Fbank GetFBankInfo(long sampsize , long sfreq ,int fbank_num);
void FreeFBankInfo(Fbank fb);
static float Mel(int k, float f);
float CalcLogRawE(float *wave, long sampsize);
void PreEmphasise (float *wave, long sampsize, float preEmph);
void Hamming(float *wave, long sampsize);
void MakeFBank(float *wave, float *fbank, Fbank fb, long sampsize, int fbank_num);
void FFT(float *xRe, float *xIm, int p);
void MakeMFCC_0(float *fbank, float *mfcc, int fbank_num, int mfcc_dim);
void MakeMFCC(float *fbank, float *mfcc, int fbank_num, int mfcc_dim);
void WeightCepstrum (float *mfcc, int mfcc_dim, int lifter);

/* 
 *  Convert wave -> MFCC_E_D_(Z)
 */
void Wav2MFCC_E_D(short *wave, float *mfcc, long sampsize,long sfreq
		, int fbank_num, int raw_e, float preEmph, int mfcc_dim
		, int lifter, float *fbank)
{
    float *bf;                        /* Work space for FFT */
    float energy=0; 
    int i, k;
    Fbank fb;

    /* Get filterbank information */
    fb = GetFBankInfo(sampsize , sfreq ,fbank_num);
  
    if((bf = (float *)malloc(fb.fft_size * sizeof(float))) == NULL){
      perror(NULL); exit(1);
    }

    k = 1;
    for(i = 1; i <= sampsize; i++){
      bf[k] = (float)wave[i - 1];  k++;
    }
    
    /* Calculate Log Raw Energy */
    if(raw_e)
      energy = CalcLogRawE(bf, sampsize); 

    /* Pre-emphasise */
    PreEmphasise(bf, sampsize, preEmph);
    
    /* Hamming Window */
    Hamming(bf, sampsize);

    /* Calculate Log Energy */
    if(!raw_e)
      energy = CalcLogRawE(bf, sampsize);

    /* Filterbank */
    MakeFBank(bf, fbank, fb, sampsize, fbank_num);

    /* MFCC_0 (include mfcc[0]=DC=power) */
    MakeMFCC_0(fbank, mfcc, fbank_num, mfcc_dim);   

    /* Weight Cepstrum */
	if( lifter != 0 )
    	WeightCepstrum(&mfcc[1], mfcc_dim, lifter);

    /* Log Energy */
    if( fbankpow == 0 )
	  mfcc[0] = energy;

    free(bf);
	FreeFBankInfo(fb);
/*
printf("[ ");
for(i=0;i<=mfcc_dim;i++)printf("%g ",mfcc[i]);
printf("]\n");
*/
}


/* 
 *  Get filterbank information
 */
Fbank GetFBankInfo(long sampsize , long sfreq ,int fbank_num)
{
  Fbank fbank;
  float mel_lo, mel_hi, mel_s, mel_k;
  int k, c, max, half;

  fbank.fbank_num = fbank_num;
  fbank.fft_size = 2;  fbank.n = 1;
  while(sampsize > fbank.fft_size
     || fbank_num*3 > fbank.fft_size)  /* added 2001.11.7 for sasx */
  {   
    fbank.fft_size *= 2; fbank.n++;
  }

  half = fbank.fft_size / 2;
  fbank.f = sfreq / (fbank.fft_size * 700.0);
  max = fbank_num + 1;
  fbank.lo = 2;   fbank.hi = half;
  mel_lo = 0;     mel_hi = Mel(half + 1, fbank.f);
  
  if((fbank.cent = (float *)malloc((max + 1) * sizeof(float))) == NULL){
    perror(NULL); exit(1);
  }
  mel_s = mel_hi - mel_lo;
  for (c = 1; c <= max; c++) 
    fbank.cent[c] = ((float)c / max)*mel_s + mel_lo;

  if((fbank.lo_index = (short *)malloc((half + 1) * sizeof(short))) == NULL){
    perror(NULL); exit(1);
  }
  for(k = 1, c = 1; k <= half; k++){
    mel_k = Mel(k, fbank.f);
    while (fbank.cent[c] < mel_k && c <= max) ++c;
    fbank.lo_index[k] = c - 1;
  }

  if((fbank.wgt = (float *)malloc((max + 1) * sizeof(float))) == NULL){
    perror(NULL); exit(1);
  }
  for(c = 0; c <= max; c++ )
    fbank.wgt[c] = 0;

  if((fbank.lo_weight = (float *)malloc((half + 1) * sizeof(float))) == NULL){
    perror(NULL); exit(1);
  }
  for(k = 1; k <= half; k++) {
    c = fbank.lo_index[k];
    if (c > 0) 
      fbank.lo_weight[k] = (fbank.cent[c + 1] - Mel(k, fbank.f)) / (fbank.cent[c + 1] - fbank.cent[c]);
    else
      fbank.lo_weight[k] = (fbank.cent[1] - Mel(k, fbank.f)) / (fbank.cent[1] - mel_lo);

    /* bin>0なら lo_weight[bin]で足す bin<fbank_numなら bin+1に1-lo_weight[bin]を足す */
    if (c > 0) fbank.wgt[c] += fbank.lo_weight[k];
    if (c < fbank_num) fbank.wgt[c+1] += (1.0 - fbank.lo_weight[k]);
  }
  
  if((fbank.Re = (float *)malloc((fbank.fft_size + 1) * sizeof(float))) == NULL){
    perror(NULL); exit(1);
  }
  if((fbank.Im = (float *)malloc((fbank.fft_size + 1) * sizeof(float))) == NULL){
    perror(NULL); exit(1);
  }

#if 0
  printf("wgt");
  for(k=1; k<=fbank_num; k++) printf(" %g",fbank.wgt[k]);
  printf("\n");
#endif

  return(fbank);
}

/*
 *  Free Filterbank Information
 */
void FreeFBankInfo(Fbank fb)
{
	free(fb.Im);
	free(fb.Re);
	free(fb.lo_weight);
	free(fb.lo_index);
	free(fb.cent);
	free(fb.wgt);
}

/* 
 *  Convert mel-frequency
 */
static float Mel(int k, float f)
{
  return(1127 * log(1 + (k-1) * f));
}

/* 
 *  Calculate Log Raw Energy 
 */
float CalcLogRawE(float *wave, long sampsize)
{		   
  int i;
  float raw_E = 0.0;
  float energy;

  for(i = 1; i <= sampsize; i++)
    raw_E += wave[i] * wave[i];
  if( raw_E < EPS )
    raw_E = EPS;
  energy = (float)log(raw_E);

  return(energy);
}

/* 
 *  Pre-emphasis
 */
void PreEmphasise (float *wave, long sampsize, float preEmph)
{
  int i;
   
  for(i = sampsize; i >= 2; i--)
    wave[i] -= wave[i - 1] * preEmph;
  wave[1] *= 1.0 - preEmph;  
}

/* 
 *  Hamming window
 */
void Hamming(float *wave, long sampsize)
{
  int i;
  float a;

  a = 2 * PI / (sampsize - 1);
  for(i = 1; i <= sampsize; i++)
    wave[i] *= 0.54 - 0.46 * cos(a * (i - 1));
}

/* 
 *  Convert wave -> mel-frequency filterbank
 */
void MakeFBank(float *wave, float *fbank, Fbank fb, long sampsize, int fbank_num)
{
  int k, bin, i;
  float Re, Im, A, temp;

  fb.fbank_num = fbank_num;

  for(k = 1; k <= sampsize; k++){
    fb.Re[k - 1] = wave[k];  fb.Im[k - 1] = 0.0;
  }
  for(k = sampsize + 1; k <= fb.fft_size; k++){
    fb.Re[k - 1] = 0.0;      fb.Im[k - 1] = 0.0;
  }
  
  FFT(fb.Re, fb.Im, fb.n);

  for(i = 1; i <= fbank_num; i++)
    fbank[i] = 0;
  
  for(k = fb.lo; k <= fb.hi; k++){
    Re = fb.Re[k - 1];
	Im = fb.Im[k - 1];
    A = sqrt(Re * Re + Im * Im);
    bin = fb.lo_index[k];
    Re = fb.lo_weight[k] * A;
    if(bin > 0) fbank[bin] += Re;
    if(bin < fbank_num) fbank[bin + 1] += A - Re;
  }

  for(bin = 1; bin <= fbank_num; bin++){ 
    temp = fbank[bin];
	if( fbanknormalize )
		temp /= fb.wgt[bin];
    if(temp < EPS) temp = EPS;
    fbank[bin] = log(temp);  
  }
}


/* 
 *  FFT
 */
void FFT(float *xRe, float *xIm, int p)
{
  int i, ip, j, k, m, me, me1, n, nv2;
  float uRe, uIm, vRe, vIm, wRe, wIm, tRe, tIm;
  
  n = 1<<p;
  nv2 = n / 2;
  
  j = 0;
  for(i = 0; i < n-1; i++){
    if(j > i){
      tRe = xRe[j];      tIm = xIm[j];
      xRe[j] = xRe[i];   xIm[j] = xIm[i];
      xRe[i] = tRe;      xIm[i] = tIm;
    }
    k = nv2;
    while(j >= k){
      j -= k;      k /= 2;
    }
    j += k;
  }

  for(m = 1; m <= p; m++){
    me = 1<<m;                me1 = me / 2;
    uRe = 1.0;                uIm = 0.0;
    wRe = cos(PI / me1);      wIm = -sin(PI / me1);
    for(j = 0; j < me1; j++){
      for(i = j; i < n; i += me){
	ip = i + me1;
	tRe = xRe[ip] * uRe - xIm[ip] * uIm;
	tIm = xRe[ip] * uIm + xIm[ip] * uRe;
	xRe[ip] = xRe[i] - tRe;   xIm[ip] = xIm[i] - tIm;
	xRe[i] += tRe;            xIm[i] += tIm;
      }
      vRe = uRe * wRe - uIm * wIm;   vIm = uRe * wIm + uIm * wRe;
      uRe = vRe;                     uIm = vIm;
    }
  }
}

/* 
 *  Apply the DCT to filterbank (test version : including 0th power)
 */ 
void MakeMFCC_0(float *fbank, float *mfcc, int fbank_num, int mfcc_dim)
{
  int i, j;
  float A, B, C;

  A = sqrt(2.0 / fbank_num);
  B = PI / fbank_num;

  /* Take DCT */
  for(i = 0; i <= mfcc_dim; i++){
    mfcc[i] = 0.0;
    C = i * B;
    for(j = 1; j <= fbank_num; j++)
      mfcc[i] += fbank[j] * cos(C * (j -1 + 0.5));
    mfcc[i] *= A;
  }
  mfcc[0] /= sqrt(2.0);
}

/* 
 *  Apply the DCT to filterbank
 */ 
void MakeMFCC(float *fbank, float *mfcc, int fbank_num, int mfcc_dim)
{
  int i, j;
  float A, B, C;
  
  A = sqrt(2.0 / fbank_num);
  B = PI / fbank_num;

  /* Take DCT */
  for(i = 1; i <= mfcc_dim; i++){
    mfcc[i - 1] = 0.0;
    C = i * B;
    for(j = 1; j <= fbank_num; j++)
      mfcc[i - 1] += fbank[j] * cos(C * (j - 0.5));
    mfcc[i - 1] *= A;     
  }       
}

/* 
 *  Weight cepstrum
 *  BHJ weight
 */
void WeightCepstrum (float *mfcc, int mfcc_dim, int lifter)
{
  int i;
  float a, b;
  
  a = PI / lifter;
  b = lifter / 2.0;
  for(i = 0; i < mfcc_dim; i++){
    mfcc[i] *= 1.0 + b * sin((i + 1) * a);
  }
}

