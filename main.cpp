/**
 * Simple UNIX Shell (Mac OS X 10.6)
 * Builtin commands: cd, exit, killbg
 *
 * @version 28.12.2010
 * @author	Jeff Jankowski
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>
#include <list>

//max character input
#define CHARS_MAX	100
//max arguments
#define	ARGS_MAX	10

/*
 * Specifies whether the process should be run in the 
 * foreground or the background
 */
enum ProcessType {FORE, BACK}; 

//prefix to shell prompt
static const char* prmpt = "[mySHELL]";

//redefine handler return type, since we don't use it
typedef void (*sighandler_t)(int);
//argument array
static char *my_argv[ARGS_MAX];
//global process type
static ProcessType pt;
//currently running PIDs
static std::list<int> bgProcIDs;

/*
 * Prints prmpt to standard out
 */
void prompt()
{
	printf("%s ", prmpt);
}

/*
 * On interupt signal: prompt and flush
 */
void interrupt_handler(int sig)
{
	printf("\n%s ", prmpt);
	fflush(stdout);
}

/*
 * On child exit signal: reap zombie and remove 
 * pid from running process list
 */
void child_handler(int sig)
{
	//reset signal handler
	signal(SIGCHLD, child_handler);
	int pid = wait(NULL);
	bgProcIDs.remove(pid);
}

/*
 * Fills my_argv array from raw input
 * @param input	Pointer to the raw input string
 */
void getArgs(char *input)
{
	//token
	char *tok;
	int argNum = 0;
	//get first token, delimited by whitespace
	tok = strtok(input," ");
	
	//loop through tokens
	while (tok != NULL || argNum >= ARGS_MAX) {
		if (tok[strlen(tok)-1] != '&') {
			//clear some space safely
			if (my_argv[argNum] == NULL)
				my_argv[argNum] = (char *)malloc(sizeof(char) * strlen(tok) + 1);
			else {
				bzero(my_argv[argNum], strlen(my_argv[argNum]));
			}
			
			//add the argument to arg list
			strncpy(my_argv[argNum], tok, strlen(tok));
			strncat(my_argv[argNum], "\0", 1);
			
			argNum++;
			tok = strtok(NULL, " ");
		}
		//FOUNDS &: process in background
		else {
			//copy all but the & at the end
			if (strlen(tok) > 1) {
				if (my_argv[argNum] == NULL)
					my_argv[argNum] = (char *)malloc(sizeof(char) * strlen(tok));
				else {
					bzero(my_argv[argNum], strlen(my_argv[argNum]));
				}
				
				strncpy(my_argv[argNum], tok, strlen(tok) - 1);
				strncat(my_argv[argNum], "\0", 1);
			}
			//stop looking after ampersand shows up
			pt = BACK;
			return;
		}
	}
	//default: process in the foreground
	pt = FORE;
}

/*
 * Execute the command in a child process.
 * Wait for the child if pt = FORE
 * @param *cmd	Point to commands string
 */
void execute(const char *cmd)
{
	//Child Code
	int pid = fork();
	if (pid == 0) {
		//should not return if successful
		execvp(cmd, my_argv);
		
		printf("%s: command not found\n", cmd);
		exit(1);
	//Parent Code
	} else if (pid > 0) {
		//wait if foreground process
		if (pt == FORE)
			waitpid(pid, NULL, 0);
		//add pid to end of running process list
		else if (pt == BACK) {
			bgProcIDs.push_back(pid);
		}
	}
}

/*
 * Free up and clear the argument list
 */
void resetArgs()
{
	int i;
	//loop through all existing arguments
	for (i = 0; my_argv[i] != NULL; i++) {
		//zero out the string
		bzero(my_argv[i], strlen(my_argv[i]) + 1);
		my_argv[i] = NULL;
		free(my_argv[i]);
	}
}

/*
 * Builtin cd (change directory) command
 */
void changeDir()
{
	//1st argument after command is the path
	char *path = my_argv[1];
	
	if (path == NULL)
		return;
	else if (chdir(path))
		printf("cd: %s: No such file or directory\n", path);
}

/*
 * Kill all the currently running processes.
 */
void killBgProcs()
{
	std::list<int>::iterator it;
	
	//kill processes in running process list
	for (it = bgProcIDs.begin(); it != bgProcIDs.end(); ++it) {
		int pid = *it;
		kill(pid, SIGINT);
		//as child processes die, they send asynch SIGCHLD signals
		//handler cannot take them all at once, and there is no queue
		//sleep 1ms to give time for processing
		usleep(1000);
	}
	
	//clear the list
	bgProcIDs.clear();
}

/*
 * MAIN Shell
 */
int main(int argc, char *argv[], char *envp[])
{
	char c;
	//raw input
	char *input = (char *)malloc(sizeof(char) * CHARS_MAX);
	//the command to execute
	char *cmd = (char *)malloc(sizeof(char) * CHARS_MAX);
	
	pt = FORE;
	
	//clear the interrupt signal handler
	signal(SIGINT, SIG_IGN);
	//attach our own handler
	signal(SIGINT, interrupt_handler);
	//clear the child exit signal handler
	signal(SIGCHLD, SIG_IGN);
	//add out own handler
	signal(SIGCHLD, child_handler);
	
	//clear the console
	execute("clear");
	prompt();
	fflush(stdout);
	
	//loop until end-of-file character (ctrl-D)
	while(c != EOF) {
		//get input character
		c = getchar();
		
		switch (c) {
			//the enter/return key was pushed
			case '\n':
				//no input
				if (input[0] == '\0') {
					prompt();
				} 
				else {
					//populate argv for passing
					getArgs(input);
					//isolate command
					strncpy(cmd, my_argv[0], strlen(my_argv[0]));
					strncat(cmd, "\0", 1);
					
					//catch builtin command
					if (!strcmp(cmd, "cd"))
						changeDir();
					else if (!strcmp(cmd, "killbg"))
						killBgProcs();
					else if (!strcmp(cmd, "exit"))
						exit(0);
					else 
						//execute command
						execute(cmd);
					
					resetArgs();
					prompt();
					//reset commands
					bzero(cmd, CHARS_MAX);
				}
				
				//reset input
				bzero(input, CHARS_MAX);
				break;
				
			//add current character to input
			default: 
				strncat(input, &c, 1);
				break;
		}
	}
	
	free(input);
	free(cmd);
	printf("\n");
	return 0;
}
