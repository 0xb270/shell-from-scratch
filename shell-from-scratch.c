#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_PATH_LENGTH 100
#define LF  10

#define FALSE 0
#define TRUE 1

#define EOL	1
#define ARG	2
#define AMPERSAND 3
#define RDIN  4 // '<' Redirection In.
#define RDOUT 5 // '>' Redirection Out.
#define PIPE  6 // '|' Pipe.

#define FOREGROUND 0
#define BACKGROUND 1

static char	input[512];
static char	tokens[1024];
char		*ptr, *tok;

int get_token(char **outptr)
{
	int	type;

	*outptr = tok;
	while ((*ptr == ' ') || (*ptr == '\t')) ptr++;

	*tok++ = *ptr;

	switch (*ptr++) {
		case '\0' : type = EOL; break;
		case '&': type = AMPERSAND; break;
		case '<': type = RDIN;  break; /* 리다이렉션 토큰 추가 */
		case '>': type = RDOUT; break; /* 리다이렉션 토큰 추가 */
		case '|': type = PIPE;  break; /* 파이프 토큰 추가 */
		default : type = ARG;
			while ((*ptr != ' ') && (*ptr != '&') &&
				(*ptr != '\t') && (*ptr != '\0'))
				*tok++ = *ptr++;
	}
	*tok++ = '\0';
	return(type);
}

int execute(char **comm, int how)
{
	int	pid;

	if ((pid = fork()) < 0) {
		fprintf(stderr, "minish : fork error\n");
		return(-1);
	}
	else if (pid == 0) {
		execvp(*comm, comm);
		fprintf(stderr, "minish : command not found\n");
		exit(127);
	}
	if (how == BACKGROUND) {	/* Background execution */
		printf("[%d]\n", pid);
		return 0;
	}		
	/* Foreground execution */
	while (waitpid(pid, NULL, 0) < 0)
		if (errno != EINTR) return -1;
	return 0;
}

int parse_and_execute(char *input)
{
	char *arg[1024];
	int	 type, how;
	int	 quit = FALSE;
	int	 narg = 0;
	int	 finished = FALSE;
	int  pid[2], p[2];
	int  fd;
	int  status;

	ptr = input;
	tok = tokens;
	while (!finished) {
		switch (type = get_token(&arg[narg])) {
  		case ARG :
  			narg++;
  			break;
  		case EOL :
  		case AMPERSAND:
  			if (!strcmp(arg[0], "quit")) quit = TRUE;
  			else if (!strcmp(arg[0], "exit")) quit = TRUE;
  			else if (!strcmp(arg[0], "cd")) chdir(arg[1]); // cd 기능 구현
  			else if (!strcmp(arg[0], "type")) {
  				if (narg > 1) {
  					int	 i, fid;
  					int	 readcount;
  					char buf[512];
  					fid = open(arg[1], O_RDONLY);
  					if (fid >= 0) {
  						readcount = read(fid, buf, 512);
  						while (readcount > 0) {
  							for (i = 0; i < readcount; i++)
  								putchar(buf[i]);
  							readcount = read(fid, buf, 512);
  						}
  					}
  					close(fid);
  				}
  			}
  			else {
  				how = (type == AMPERSAND) ? BACKGROUND : FOREGROUND;
  				arg[narg] = NULL;
  				if (narg != 0)
  					execute(arg, how);
  			}
  			narg = 0;
  			if (type == EOL)
  				finished = TRUE;
  			break; 
  		case RDIN: /* 리다이렉션 인 */
        type = get_token(&arg[++narg]); /* 2번째 인자 받아오기 */
        pid[0] = fork(); /* 자식 프로세스 생성 */
        if(pid[0] < 0) { /* Handling fork() error */
          perror("minish: fork error");
          exit(-1);
        }
        else if(pid[0] == 0) { /* child */
          fd = open(arg[2], O_RDONLY); 
		  /* 2번째 인자로 받은 파일 읽기 전용으로 열기 */
          if(fd < 0) {
            perror("minish: open error");
            exit(-1);
          }
          dup2(fd, STDIN_FILENO); /* 자식 프로세스의 표준입력을 arg[2]로 설정 */
          close(fd);
          execlp(arg[0], arg[0], (char*)0); /* 자식 프로세스로 arg[0] 실행 */
          perror("minish: command not found"); exit(-1);
        }
        else if(pid[0] > 0) { /* Parent: Shell */
          while(waitpid(pid[0], NULL, 0) < 0)
            if(errno != EINTR) return -1;
        }
        return quit=FALSE;
  		case RDOUT: /* 리다이렉션 아웃 */
        type = get_token(&arg[++narg]); /* 2번째 인자 받아오기 */
        pid[0] = fork(); /* 자식 프로세스 생성 */
        if(pid[0] < 0){ /* Handling fork() error */
          perror("minish: fork error");
          exit(-1);
        }
        else if(pid[0] == 0) { /* child */
          fd = open(arg[2], O_RDWR | O_CREAT | S_IROTH, 0644);
		  /* 2번째 인자로 받은 파일을 열기(없으면 생성, 있으면 덮어쓰기) */
          if(fd < 0) {
            perror("minish: open error");
            exit(-1);
          }
          dup2(fd, STDOUT_FILENO); /* 자식 프로세스의 표준출력을 arg[2]로 설정 */
          close(fd);
          execlp(arg[0], arg[0], (char*)0); /* 자식 프로세스로 arg[0] 실행 */
          perror("minish: command not found"); exit(-1);
        }
        else if(pid[0] > 0) { /* Parent: Shell */
          while(waitpid(pid[0], NULL, 0) < 0)
            if(errno != EINTR) return -1;
        }
  		return quit=FALSE;
      case PIPE: /* 파이프 */
        type = get_token(&arg[++narg]); /* 2번째 인자 받아오기 */
        pipe(p);

        pid[0] = fork(); /* 1번째 자식 프로세스 생성 */
        if(pid[0] == 0) {
          dup2(p[1], STDOUT_FILENO); /* 1번째 자식 프로세스의 표준출력을 파이프의 출력으로 설정 */
          close(p[0]); close(p[1]);
          execlp(arg[0], arg[0], (char*)0); /* 1번째 자식 프로세스로 arg[0] 실행 */
          perror("minish: first command not found"); exit(-1);
        }
        
        pid[1] = fork(); /* 2번째 자식 프로세스 생성 */
        if(pid[1] == 0) {
          dup2(p[0], STDIN_FILENO); /* 2번째 자식 프로세스의 표준입력을 파이프의 입력으로 설정 */
          close(p[0]); close(p[1]);
          execlp(arg[2], arg[2], (char*)0); /* 2번째 자식 프로세스로 arg[2] 실행 */
          perror("minish: second command not found"); exit(-1);
        }
        
		close(p[0]); close(p[1]);
        while(waitpid(pid[0], NULL, 0) < 0)
          if(errno != EINTR) return -1;
        while(waitpid(pid[1], NULL, 0) < 0)
          if(errno != EINTR) return -1;
        return quit=FALSE;
		}
	}
	return quit;
}

// Return '#' if root else '$'.
char check_su()
{
  uid_t uid = geteuid();
  if(uid == 0) return '#';
  return '$';
}

// Return the pointer of current username.
const char *get_username()
{
  uid_t uid = geteuid();
  struct passwd *pw = getpwuid(uid);
  if (pw)    return pw->pw_name;
  return "";
}

// Print status of prompt.
int print_prompt()
{
    char *buffer = (char *)malloc(MAX_PATH_LENGTH);
    char *value = (char *)getcwd(buffer, MAX_PATH_LENGTH);

    if (value != 0) fprintf(stdout, "shell-from-scratch : %s\n%s%c ", buffer, get_username(), check_su());
    free(buffer);

    return 0;
}

int main(){
    char *arg[1024];
	int	quit;
	print_prompt();
    while(gets(input)){
		quit = parse_and_execute(input);
		if(quit) break;
        print_prompt();
    }
    return 0;
}
