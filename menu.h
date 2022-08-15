#ifndef MENU_H
#define MENU_H

int simple_menu(Display *disp, int screen, int size, char **txt, int id);
int radio_menu(Display *disp, int screen, int size, char **txt, int *id);
int toggle_menu(Display *disp, int screen, int size, char **txt, int *id, char *flag);

#endif /* MENU_H */
