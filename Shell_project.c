/**
UNIX Shell Project

Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Some code adapted from "Fundamentos de Sistemas Operativos", Silberschatz et al.

To compile and run the program:
   $ gcc Shell_project.c job_control.c -o Shell
   $ ./Shell          
	(then type ^D to exit program)

**/

#include "job_control.h"   // remember to compile with module job_control.c 

#define MAX_LINE 256 /* 256 chars per line, per command, should be enough. */
job * job_control_list;  /* list for background or suspended processes */

// -----------------------------------------------------------------------
//                            MAIN          
// -----------------------------------------------------------------------

void job_control_handler(int sig){ 	/* handler for zombie processes */
	switch(sig){
		case SIGCHLD : {
			/* Entered zombie handler */
			int status, info;
			pid_t pid_child;
			block_signal(SIGCHLD, 1);
			job *pJob = job_control_list;
			while(pJob != NULL){
				pid_child = waitpid((*pJob).pgid, &status, WNOHANG | WUNTRACED | WCONTINUED);
				if((*pJob).pgid == pid_child){
					//printf("st_res = %s\n", status_strings[st_res]); 		/* a bit of debugging */
					if(WIFCONTINUED(status)){
						printf("Suspended job has been continued... pid : %d, command: %s.\n", (*pJob).pgid, (*pJob).command);
						(*pJob).state = BACKGROUND;
					} else {
						enum status st_res = analyze_status(status, &info);
						if(st_res == SUSPENDED)
							(*pJob).state = STOPPED;	/* If process changes to SUSPENDED -> change it's state in the job list */
						else if(st_res == EXITED || st_res == SIGNALED){
							printf("Background job has exited... pid : %d, command: %s.\n", (*pJob).pgid, (*pJob).command);
							delete_job(job_control_list, pJob);	/* In any other case get rid of it */
							/* Zombie is punished */
						}
					}
				}
				pJob =(*pJob).next;
			}
			block_signal(SIGCHLD, 0);
		break;
		}
		default: break;
	}
}

int main(void)
{
	char inputBuffer[MAX_LINE]; /* buffer to hold the command entered */
	int background;             /* equals 1 if a command is followed by '&' */
	char *args[MAX_LINE/2];     /* command line (of 256) has max of 128 arguments */
	// probably useful variables:
	int pid_fork, pid_wait; /* pid for created and waited process */
	int status;             /* status returned by wait */
	enum status status_res; /* status processed by analyze_status() */
	int info;				/* info processed by analyze_status() */
	job_control_list = new_list("Shell background or suspended processes list");
	int fg = 0;
	job *pJob;
	int pid;
	char *commandRet;

	while (1)   /* Program terminates normally inside get_command() after ^D is typed*/
	{   		
		printf("COMMAND->");
		fflush(stdout);
		get_command(inputBuffer, MAX_LINE, args, &background);  /* get next command */
		

		terminal_signals(SIG_IGN); 	/* ignore terminal signals */
		signal(SIGCHLD, job_control_handler);  /* set up a handler for SIGCHLD */
		
		if(args[0]==NULL) continue;   // if empty command

		if(!strcmp(*args, "cd")){ /* if the command is "cd" then */
			if(chdir(args[1]) < 0) 	/* execute it as an internal one */
				perror(args[1]);
		} else if(!strcmp(*args, "jobs")){
			print_list(job_control_list, &print_item);
		} else if(!strcmp(*args, "fg")){
			if(!empty_list(job_control_list)){
				fg = 1;
				int pos = args[1] == NULL ? 1 : atoi(args[1]);
				pJob = get_item_bypos(job_control_list, pos);
				killpg(pJob -> pgid, SIGCONT);
				tcsetpgrp(STDIN_FILENO, pJob -> pgid);  /* grant the terminal to the process */
				block_signal(SIGCHLD, 1);
				pJob -> state = FOREGROUND;
				block_signal(SIGCHLD, 0);
				goto exec; 	/* reuse the parent's code (not spaghetti !) */
			}
		} else if(!strcmp(*args, "bg")){
			if(args[1] == NULL)
				printf("Please, introduce an index.\n");
			else {
				pJob = get_item_bypos(job_control_list, atoi(args[1]));
				killpg(pJob -> pgid, SIGCONT);
			}
		} else {  	/* else the command is an external one */
			pid_fork = fork();
			if(pid_fork){ 		/* branch for a parent process */
				if(!background){  	 /* if a process runs in foreground */
			exec:	if(fg){
						pid = (*pJob).pgid;
						commandRet = (*pJob).command;
					} else {
						pid = pid_fork;
						commandRet = *args;
					}
					pid_wait = waitpid(pid, &status, WUNTRACED);   /* wait for the created child process */
					tcsetpgrp(STDIN_FILENO, getpid());  /* give back the terminal to the Shell */
					status_res = analyze_status(status, &info);		/* obtain status report */

					if(status_res == SUSPENDED){		/* If process is suspended -> add it to the list */
						block_signal(SIGCHLD, 1);
						add_job(job_control_list, new_job(pid, commandRet, STOPPED));
						block_signal(SIGCHLD, 0);
					}
					
					if(info != EXIT_FAILURE) 	/* inform about terminated job */
						printf("Foreground pid : %d, command: %s, %s, info: %d.\n", pid, commandRet, status_strings[status_res], info);
					if(fg){
						block_signal(SIGCHLD, 1);
						delete_job(job_control_list, pJob);
						block_signal(SIGCHLD, 0);
						fg = 0;
					}
				} else { 	/* otherwise */
					printf("Background job is running... pid : %d, command: %s.\n", pid_fork, *args);
					block_signal(SIGCHLD, 1);
					add_job(job_control_list, new_job(pid_fork, *args, BACKGROUND));  /* add a job in the list*/
					block_signal(SIGCHLD, 0);
				}
			} else if (!pid_fork){ 	/* branch for a child process */

				int gid_state = setpgid(0, 0); 	/* assign different from the parent PGID to the child */
				if(!background)	/* If it's not a background process -> don't assign the terminal to it */
					tcsetpgrp(STDIN_FILENO, getpid());  /* assign the terminal to the child's PGID */

				terminal_signals(SIG_DFL);	/* restore terminal signals */
				if(execvp(*args, args) < 0){	/* execute child process with a different content */
					printf("Command %s is not found.\n", *args);
					exit(EXIT_FAILURE);
				} else {
					exit(EXIT_SUCCESS);
				}
			}
		}
	}
}
