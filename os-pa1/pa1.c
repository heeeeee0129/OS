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
}; ///// 히스토리 커맨드 사용시 커맨드들을 각 entry에 저장함.  --- append_history


LIST_HEAD(history); //// 커맨드 엔트리들을 순서대로 저장하는 스택, 0,1,2,,, 차례대로 아래에서부터 저장

static int __process_command(char * command);
static int historyCounter=0;  //// 히스토리 저장 시 순서index 결정


static void append_history(char * const command) ////커맨드를 history 스택에 저장해두는 함수
{	struct entry *ptr = (struct entry*)malloc(sizeof(struct entry));
    if(ptr!=NULL){
	ptr->command = (char*)malloc(sizeof(char)*80);
	strcpy(ptr->command, command);
	ptr->index = historyCounter++;
	list_add(&(ptr->list), &history);
}

}



static int run_command(int nr_tokens, char *tokens[]) ///명령어 처리
{	
	int startingPoint=0;

	if (strcmp(tokens[0], "exit") == 0) return 0; /////exit 입력할 경우 터미널 종료


	for(int i=0; tokens[i]!=NULL; i++){ ///// 커맨드 라인 전체를 훑음
		if(strcmp(tokens[i],"|")==0){   /////파이프 문자가 있다면 파이프 연산으로 처리
			tokens[i]=NULL;    ///// 파이프 문자를 null로 처리하여 커맨드를 두 개로 나눠서 처리할 수 있도록 함. 
			startingPoint = i+1 ;  //// 파이프 바로 다음 칸부터 두번째 명령어 시작. 두번째 명령어가 시작하는 주소값을 저장해둠. 
			int pid; int status;
						int pipes[2];  //// 파이프를 위한 배열

						pipe(pipes); /// 파이프 생성
						pid = fork(); /// 포크 {부모 프로세스, 자식 프로세스} <-- 파이프를 공유하게 됨.

						if(pid==0) ///자식 프로세스라면,,
						{
							dup2(pipes[1], STDOUT_FILENO);   ///파이프 입력칸을 표준출력 파일으로 복사
							close(pipes[1]);   ///파이프 입력 칸 닫음
							if(execvp(tokens[0],tokens) < 0) { /// 파이프 앞의 첫번째 커맨드 실행
							if (errno == 2) {
								fprintf(stderr, "Unable to execute %s\n", tokens[0]);
								return -1;  // 존재하지 않는 커맨드 에러 처리
							}}
						}
						else
						{ 
							 int pid2=fork();

							if(pid2==0)
							{
								dup2(pipes[0], STDIN_FILENO);  /// 파이프 출력칸을 표준입력 파일로 복사
								close(pipes[1]); /// 파이프 입력 칸 닫음
								if(execvp(tokens[startingPoint],tokens+startingPoint) < 0) { /// 파이프 뒤의 두번째 커맨드 실행
								if (errno == 2) {
									fprintf(stderr, "Unable to execute %s\n", tokens[startingPoint]);
									return -1; //존재하지 않는 커맨드 에러처리
								}	}
							}
							else
							{
								int status2;
								close(pipes[0]);
								close(pipes[1]); /// 파이프 입출력 모두 닫음
								waitpid(pid2, &status2, 0); // 두번째 포크로 만든 자식 프로세스가 종료될때까지 대기
							}
							waitpid(pid, &status, 0); // 첫번째 포크로 만든 자식 프로세스가 종료될 때까지 대기
						}
		return 1;} ////// 파이프 처리 완료하면 다시 입력받으러 메인으로
		
	}


	if(strncmp(tokens[0], "cd", 2)==0){ //////// cd 커맨드 처리
			if(nr_tokens == 1 || strncmp(tokens[1], "~", 1)==0){ //// cd ~ 의 경우 ~을 HOME 경로로 변경해주어야 함. 
				chdir(getenv("HOME")); //// HOME의 경로로 디렉토리 이동
				chdir(tokens[1]+2);   /////  ``` ~/ 뒤에 경로를 입력 시에 해당하는 경로로 이동
				return 1;
			}
			else{
				chdir(tokens[1]); /// 일반 경로를 입력받은 경우 해당경로로 디렉토리 이동
				return 1;
			}
	}

	else if(strncmp(tokens[0], "history", 7)==0){   /// history 명령어 처리
		struct entry* pos;
		list_for_each_entry_reverse(pos, &history, list){   /// history 스택의 모든 엔트리를  아래에서 순서대로 출력
			fprintf(stderr, "%2d: %s", pos->index, pos->command);
		}
		
	}


	else if(strncmp(tokens[0], "!", 1)==0){   /// ! 명령어 처리
		struct entry* pos;
		list_for_each_entry_reverse(pos, &history, list){ ///히스토리 스택 순회
			if(pos->index == *tokens[1]-'0'){ /// ! 뒤에 입력받은 인덱스 숫자와 일치하는 엔트리를 탐색
				char temp[80]={'\0'};
				strcpy(temp, pos->command);  //// 해당하는 엔트리의 커맨드를 가져옴
				__process_command(temp);    /// 해당 커맨드 실행
				return 1;
				}
		}
		}

	else{
			int pid;
			int status;
	        pid = fork();

			if(pid == 0){
				
				if(execvp(tokens[0],tokens) < 0) {
					if (errno == 2) {
						fprintf(stderr, "Unable to execute %s\n", tokens[0]);
						return -1;
					}	
					for (int i=0;tokens[i]!=NULL;i++) {
					free(tokens[i]);
					}
					free(tokens);
				} 
			}
			else{
				waitpid(pid, &status, 0);
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
