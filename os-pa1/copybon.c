/**********************************************************************
 * Copyright (c) 2021
 *  Sang-Hoon Kim <sanghoonkim@ajou.ac.kr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTIABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 **********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

#include <string.h>

#include "types.h"
#include "list_head.h"
#include "parser.h"

struct entry {
	struct list_head list;
	char *command;
	int index;
};
LIST_HEAD(history);
static int count = 0;
/***********************************************************************
 * run_command()
 *
 * DESCRIPTION
 *   Implement the specified shell features here using the parsed
 *   command tokens.
 *
 * RETURN VALUE
 *   Return 1 on successful command execution
 *   Return 0 when user inputs "exit"
 *   Return <0 on error
 */



static void append_history(char * const command)
{	struct entry *ptr = (struct entry*)malloc(sizeof(struct entry));
if(ptr!=NULL){
	ptr->command = (char*)malloc(sizeof(char)*80);
	strcpy(ptr->command, command);
	ptr->index = count++;
	list_add(&(ptr->list), &history);
}

}



static int run_command(int nr_tokens, char *tokens[])
{	int i;
	if (strcmp(tokens[0], "exit") == 0) return 0;

	else if(strncmp(tokens[0], "cd", 2)==0){
			if(nr_tokens == 1 || strncmp(tokens[1], "~", 1)==0){
				chdir(getenv("HOME"));
				return 1;
			}
			else{								//to tokens[1] dir
				char strBuffer[MAX_TOKEN_LEN] = { 0, };
				getcwd(strBuffer, MAX_TOKEN_LEN);
				chdir(tokens[1]);
				return 1;
			}
	}

	else if(strncmp(tokens[0], "history", 7)==0){
		struct entry* pos;
		list_for_each_entry_reverse(pos, &history, list){
			fprintf(stderr, "%2d: %s", pos->index, pos->command);
		}
		
	}

	else if(strncmp(tokens[0], "!", 1)==0){
		struct entry* pos;
		list_for_each_entry_reverse(pos, &history, list){
			if(*tokens[1]==pos->index){
				append_history(*tokens);
				parse_command(pos->command, &nr_tokens, tokens);
				run_command(nr_tokens, tokens);
			}
		}


	}




	else{
	int pid;
	pid = fork();
			if(pid<0){
				fprintf(stderr, "FORK ERROR!!!\n");
				return -1;
			}
			else if(pid == 0){
				
				if(execvp(tokens[0],tokens) < 0) {
				if (errno == 2) {
					fprintf(stderr, "Unable to execute %s\n", tokens[0]);
					return -EINVAL;
				}	
				for (i=0;tokens[i]!=NULL;i++) { // 동적 할당 해제
				free(tokens[i]);
				}
				free(tokens);
				} 
				return 1;
				}
				else{
					int status;
					waitpid(pid, &status, 0);
					if(WIFSIGNALED(status) || WIFSTOPPED(status) || WEXITSTATUS(status)==1){
						fprintf(stderr, "COMMAND ERROR!!!\n");
					}
				}
		}
	return 1;

 }



/***********************************************************************
 * struct list_head history
 *
 * DESCRIPTION
 *   Use this list_head to store unlimited command history.
 */





/***********************************************************************
 * append_history()
 *
 * DESCRIPTION
 *   Append @command into the history. The appended command can be later
 *   recalled with "!" built-in command
 */


/* TODO: Implement this function */
/***********************************************************************
 * initialize()
 *
 * DESCRIPTION
 *   Call-back function for your own initialization code. It is OK to
 *   leave blank if you don't need any initialization.
 *
 * RETURN VALUE
 *   Return 0 on successful initialization.
 *   Return other value on error, which leads the program to exit.
 */
static int initialize(int argc, char * const argv[])
{
	return 0;
}


/***********************************************************************
 * finalize()
 *
 * DESCRIPTION
 *   Callback function for finalizing your code. Like @initialize(),
 *   you may leave this function blank.
 */
static void finalize(int argc, char * const argv[])
{

}


/*====================================================================*/
/*          ****** DO NOT MODIFY ANYTHING BELOW THIS LINE ******      */
/*          ****** BUT YOU MAY CALL SOME IF YOU WANT TO.. ******      */
static int __process_command(char * command)
{
	char *tokens[MAX_NR_TOKENS] = { NULL };
	int nr_tokens = 0;

	if (parse_command(command, &nr_tokens, tokens) == 0)
		return 1;

	return run_command(nr_tokens, tokens);
}

static bool __verbose = true;
static const char *__color_start = "[0;31;40m";
static const char *__color_end = "[0m";

static void __print_prompt(void)
{
	char *prompt = "$";
	if (!__verbose) return;

	fprintf(stderr, "%s%s%s ", __color_start, prompt, __color_end);
}

/***********************************************************************
 * main() of this program.
 */
int main(int argc, char * const argv[])
{
	char command[MAX_COMMAND_LEN] = { '\0' };
	int ret = 0;
	int opt;

	while ((opt = getopt(argc, argv, "qm")) != -1) {
		switch (opt) {
		case 'q':
			__verbose = false;
			break;
		case 'm':
			__color_start = __color_end = "\0";
			break;
		}
	}

	if ((ret = initialize(argc, argv))) return EXIT_FAILURE;

	/**
	 * Make stdin unbuffered to prevent ghost (buffered) inputs during
	 * abnormal exit after fork()
	 */
	setvbuf(stdin, NULL, _IONBF, 0);

	while (true) {
		__print_prompt();

		if (!fgets(command, sizeof(command), stdin)) break;

		append_history(command);
		ret = __process_command(command);

		if (!ret) break;
	}

	finalize(argc, argv);

	return EXIT_SUCCESS;
}
