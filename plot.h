#ifndef PLOT_H
#define PLOT_H

void  sas_draw_plot(SasProp*);
GC    sas_make_gc(SasProp *, char *);
void  sas_change_gc(SasProp *, GC , char *, char *);
int   sas_draw_legend(SasProp *, GC , int , int , char *);
void  sas_free_gc(SasProp *, GC );
float sas_plot_xmax(char *);

#endif /* PLOT_H */
