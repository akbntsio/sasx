#ifndef SPECTRO_H
#define SPECTRO_H

extern int sas_draw_pitch(SasProp* obj);
extern int sas_draw_spectrogram(SasProp* obj);
extern void hamming(double* x, int len);
extern void fft(double* xr, double* xi, int indx, int wsize);
extern void sas_create_grayscale(SasProp* obj, GC *gcgray, int num);
extern void sas_create_colorscale(SasProp* obj, GC *gcgray, int num);
extern void sas_free_grayscale(SasProp* obj, GC *gcgray, int num);

#endif /* SPECTRO_H */
