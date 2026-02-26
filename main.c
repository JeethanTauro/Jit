#include <stdio.h>
#include <string.h>
#include "init.h"
#include "add.h"
#include "commit.h"
#include  "status.h"

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
        if (argc < 3) {
            printf("\nUsage: jit add <filename>\n");
            return 1;
        }
        if (addFile(argv + 2)==1) {
            for (int i = 2; i < argc; i++) {
                //still prints this if there's no modification in the file and we do add, so have to see about it
                printf("Jit added %s\n", argv[i]);
            }
        }

    }
    else if (strcmp(argv[1], "commit") == 0) {
        if (argc < 4) {
            printf("Usage: jit commit -m \"message\"\n");
            return 1;
        }
        if (commitFile(argv[3])) {
            printf("\n Jit committed \n");
        }
    }
    else if (strcmp(argv[1],"status")==0) {
        status();
    }
    else {
        perror("SYNTAX error");
    }

    return 0;
}