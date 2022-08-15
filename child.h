#ifndef CHILD_H
#define CHILD_H

pid_t exec_command(int *pfd, char *command, char *key);
pid_t exec_child(int *pfd, char **args, char *key);

#endif /* CHILD_H */
