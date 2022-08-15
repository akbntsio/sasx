/*

CHILD PROCESS CONTROL

	1998/05/17 T.Akabane
	fopen のように "w", "r", "rw" でプロセスの入出力を指定する
	"r"  のとき、プロセスへからの読みだしは pfd[0] を使う
	"w"  のとき、プロセスへの書き込みは pfd[1] を使う
	"rw" のとき、プロセスへからの読みだしは pfd[2]
		     プロセスへの書き込みは pfd[1] を使う
	読み書きしないプロセスは作るようになっていない
	exec_child はコマンドラインを引数毎に分割して指定する必要がある
	exec_command はコマンドラインを１つの文字列で指定する
		ただし、引用符号("')の扱いについてはタコである。改善したい


	SAMPLE PROGRAM:

	#include <sys/types.h>
	#include <signal.h>
	int run;
	void killed() { wait(0); run = 0; }
	main() {
		int pfd[4];
		pid_t pid;
		pid = exec_child(pfd,args,"w");
		fprintf(stderr,"child process is %d\n",pid);
		signal(SIGCHLD,killed);
		for(run=1;run;){
			write(pfd[1],buff,size);
			if( event )
				kill(pid,SIGKILL);
		}
		wait(0);
	}
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include "child.h"

pid_t exec_command(int *pfd, char *command, char *key)
{
	char str[512], *args[128], *p;
	int  i;
	strncpy(str,command,512);
	for( i=0,p=str; (p=strtok(p," \t")); p=0,i++ ){
		args[i] = p;
	}
	args[i] = 0;
	return exec_child(pfd, args, key);
}
	
pid_t exec_child(int *pfd, char **args, char *key)
{
	pid_t pid;
	int tmp;

	if( !strcmp(key,"w") ){
		/* child process's stdin is mother process's pfd[0]
		*   +-----------+------+     +-------+----------+
		*   |   parent  |pfd[1]| --> |pfd[0] |0  child  |
		*   +-----------+------+     +-------+----------+
		*/
		if( pipe(pfd) ) return -1;
		if( !(pid=fork()) ){
			close(0);		/* close stdin */
			tmp = dup(pfd[0]);	/* stdin <- pfd[0] */
			if (tmp != 0) {
				fprintf(stderr,"pipe open failed\n");
			}
			close(pfd[0]);		/* pfd[0] */
			close(pfd[1]);		/* pfd[1] */
			if( execvp(args[0], args) )
				fprintf(stderr,"can't exec %s\n",args[0]);
			exit(0);
		}
		close(pfd[0]);		/* must */
		return(pid);
	}

	if( !strcmp(key,"r") ){
		/* child process's stdout  is mother process's pfd[1]
		*   +----------+-------+     +-------+----------+
		*   |  parent  | pfd[0]| <-- |pfd[1] |1  child  |
		*   +----------+-------+     +-------+----------+
		*/
		if( pipe(pfd) ) return -1;
		if( !(pid=fork()) ){
			close(1);		/* close stdout */
			tmp = dup(pfd[1]);	/* stdout <- pfd[1] */
			if( tmp != 1 ) {
				fprintf(stderr,"pipe open failed\n");
			}
			close(pfd[0]);
			if( execvp(args[0], args) )
				fprintf(stderr,"can't exec %s\n",args[0]);
			exit(0);
		}
		close(pfd[1]);		/* must */
		return(pid);
	}

	if( !strcmp(key,"rw") || !strcmp(key,"wr") ){
		/*
		*   +----------+-------+     +-------+----------+
		*   |          | pfd[1]| --> |pfd[0] |0         |
		*   |  parent  +-------+     +-------+   child  |
		*   |          | pfd[2]| <-- |pfd[3] |1         |
		*   +----------+-------+     +-------+----------+
		*/
		if( pipe(pfd) ) return -1;
		if( pipe(pfd+2) ) return -1;
		if( !(pid=fork()) ){
			close(0);
			tmp = dup(pfd[0]);	/* attach 0(stdin) to ifd[0] */
			if( tmp != 0 ) {
				fprintf(stderr,"pipe open failed\n");
			}
			close(1);	
			tmp = dup(pfd[3]);	/* attach 1(stdout) to ofd[1] */
			if( tmp != 1 ) {
				fprintf(stderr,"pipe open failed\n");
			}
			close(pfd[2]);
			close(pfd[1]);
			if( execvp(args[0], args) )
				fprintf(stderr,"can't exec %s\n",args[0]);
			exit(0);
		}
		close(pfd[0]);
		close(pfd[3]);
		return(pid);
	}
	return(0);
}

