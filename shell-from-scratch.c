#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>

#define MAX_PATH_LENGTH 100
#define LF  10

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
    char c;
    while((c=getchar())!=EOF){
        if(c==LF)   print_prompt();
    }
    return 0;
}
