#include <stdio.h>
#include <stdlib.h>

#define MAX_PATH_LENGTH 100
#define LF  10

int print_prompt()
{
    char *buffer = (char *)malloc(MAX_PATH_LENGTH);
    char *value = (char *)getcwd(buffer, MAX_PATH_LENGTH);

    if (value != 0) fprintf(stdout, "shell-from-scratch : %s\n-> ", buffer);
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
