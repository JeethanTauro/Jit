//
// Created by jeethan on 01/03/26.
//

#include <stdio.h>
#include <string.h>
#include "delete_branch.h"
void delete_branch(char *branch_name) {
    char path[1024];
    char link[1024];
    char current_branch[1024];
    FILE* head = fopen("./.jit/HEAD", "r");
    if (head == NULL) {
        printf("error opening head\n");
        return;
    }
    fgets(link, 1024, head);
    strcpy(current_branch, link+16);
    current_branch[strcspn(current_branch,"\n")] = '\0';

    if (strcmp(current_branch, branch_name) == 0) {
        printf("cannot delete the branch you are in : %s\n", branch_name);
        return;
    }

    sprintf(path, "./.jit/refs/heads/%s", branch_name);
    if (remove(path) == 0) {
        printf("Deleted branch %s\n", branch_name);
    }
    else {
        printf("Could not delete branch %s\n", branch_name);
    }

}