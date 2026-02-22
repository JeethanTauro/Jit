#include <stdio.h>
#include <string.h>
#include "init.h"


int main(int argc, char* argv[]) {
    //argv[0] is always the file name
    //argv[1] I want it to be the command init
    if(argc < 2) {
        printf("Usage: jit <command>\n");
        return 1;
    }
    if (strcmp(argv[1],"init")==0) {
        createJitDir();
        printf("\n Jit initialised created \n");
    }
    else {
        perror("SYNTAX : filename jit init");
    }

    return 0;
}
