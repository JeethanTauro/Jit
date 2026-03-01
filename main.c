#include <stdio.h>
#include <string.h>
#include "init.h"
#include "add.h"
#include "commit.h"
#include  "status.h"
#include "log.h"
#include "unstage.h"
#include "branch.h"
#include "check_branch.h"
#include "delete_branch.h"
#include "switch_branch.h"
#include "reset.h"

int main(int argc, char* argv[]) {
    //argv[0] is always the file name
    //argv[1] I want it to be the command init
    if(argc < 2) {
        printf("Usage: jit <command>\n");
        return 1;
    }
    if (strcmp(argv[1],"init")==0) {
        if (argc<3) {
            createJitDir();
            printf("\n Jit initialised created \n");
        }
        else {
            printf("usage: jit init \n");
        }

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
        if (argc > 2) {
            printf("\nUsage: jit status\n");
            return 1;
        }
        status();
    }
    else if (strcmp(argv[1],"log")==0) {
        if (argc<3) {
            jit_log();
        }
        else {
            printf("\nUsage: jit log\n");
        }

    }
    else if (strcmp(argv[1],"unstage")==0) {
        if (argc < 3) {
            printf("usage: jit unstage <filename>\n");
            return 1;
        }
        unstage(argv[2]);
    }
    else if (strcmp(argv[1], "branch") == 0) {
        if (argc == 3) {
            branch(argv[2]);
        }
        else if (argc == 4 && strcmp(argv[2], "-d") == 0) {
            delete_branch(argv[3]);  // argv[3] is the branch name!
        }
        else {
            printf("usage: jit branch <name>\n");
            printf("       jit branch -d <name>\n");
        }
    }
    else if (strcmp(argv[1],"switch")==0) {
        if (argc < 4) {
            switch_branch(argv[2]);
        }
        else{
            printf("usage: jit switch <branch name>\n");
        }
    }
    else if (strcmp(argv[1],"where")==0) {
        if (argc < 3) {
            check_branch();
        }
        else {
            printf("usage: jit where\n");
        }
    }
    else if (strcmp(argv[1], "reset") == 0) {
        if (argc < 3) {
            printf("usage: jit reset <commit-hash>\n");
            return 1;
        }
        reset(argv[2]);
    }
    else {
        printf("SYNTAX error: unknown command '%s'\n", argv[1]);
        printf("available commands: init, add, commit, status, log, unstage, reset\n");
        return 1;
    }

    return 0;
}
