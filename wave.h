#ifndef WAVE_H
#define WAVE_H

extern int  sas_clip_y(
		int x1, int y1, int x2, int y2, int ymin, int ymax,
		int *xx1, int *yy1, int *xx2, int *yy2
);

extern void  sas_draw_clip_y(SasProp *obj, GC gc,
		int x1, int y1, int x2, int y2, int ymin, int ymax);

extern int   ascii_filesize(char *fname);

extern int   read_double(FILE *fp, int hsize, int offset, int type, int nchan,
		int swap, int start, int len, double *dbuff);

extern int   sas_slow_wave(SasProp* obj);

extern int   sas_fast_wave(SasProp* obj);

extern int sas_save_area(SasProp *obj);

#endif /* WAVE_H */
