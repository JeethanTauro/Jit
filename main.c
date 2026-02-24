#include <stdio.h>
#include <string.h>
#include "init.h"
#include "add.h"
#include "commit.h"

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
    else if (strcmp(argv[1], "add") == 0) {
        addFile(argv + 2);
        for (int i = 2; i < argc; i++) {
            printf("Jit added %s\n", argv[i]);
        }
    }
    else if (strcmp(argv[1], "commit") == 0 && strcmp(argv[2], "-m") == 0) {
        commitFile(argv[3]);
        printf("\n Jit committed \n");
    }
    else {
        perror("SYNTAX error");
    }

    return 0;
}
