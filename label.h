#ifndef LABEL_H
#define LABEL_H

/* label.c */
extern int   add_label(SasProp *obj, int posi, float st, float en, float prev, float next, char *str, char *fname);
extern int   ins_label(SasProp *obj, int posi, float st, float en, float prev, float next, char *str, char *fname);
extern int   del_label(SasProp *obj, int posi);
extern void  sas_draw_label(SasProp* obj, int start, int end);
extern void  sas_copy_label(SasProp *obj, SasProp *src);
extern int   sas_read_label(SasProp *obj, char* fname);
extern void  sas_write_label(SasProp *obj);
extern int   sas_xy_to_lbl(SasProp *obj, int x, int y, int *clbl, int *clblm);
extern int   sas_lbl_to_x(SasProp *obj, int clbl, int clblm);
extern void  sas_label_sel(SasProp *obj, int clbl, int clblm);
extern void  sas_label_grab(SasProp *obj);
extern void  sas_label_move(SasProp *obj, int x);
extern void  sas_label_release(SasProp *obj);
extern void  sas_label_keep(SasProp *obj);
extern void  sas_label_undo(SasProp *obj);
extern void  sas_label_edit(SasProp *obj, char *str);
extern void  sas_label_del(SasProp *obj, int dlbl);
extern int   sas_label_split(SasProp *obj, int splbl);
extern void  sas_label_merge(SasProp *obj, int mglbl);
extern void  sas_label_add(SasProp *obj, float st, float en, float prev, float next, char *str);

#endif /* LABEL_H */
