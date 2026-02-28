#include <stdio.h>
#include <string.h>
#include "branch.h"
//
// Created by jeethan on 28/02/26.
//
//to create a branch ( a branch is just a file that points to the current commit nothing complicated)
//step 1: create a file named as the branch name
//step 2: read the hash from the current branch (get it from head)
//step 3: write that hash into the newly created branch file
void branch(char *branch_name) {


    char new_branch_path[1024];
    //checking if the file exists

    char line[1024];
    char curent_branch[1024];
    sprintf(new_branch_path,"./.jit/refs/heads/%s",branch_name);
    FILE* check = fopen(new_branch_path, "r");
    if (check != NULL) {
        printf("error: branch %s already exists\n", branch_name);
        fclose(check);
        return;
    }

    FILE* new_branch = fopen(new_branch_path, "w"); //create the new branch file
    if (new_branch == NULL) {
        perror("Error creating branch");
        return;
    }
    //read the current branch from the HEAD
    FILE* head = fopen("./.jit/HEAD", "r");
    if (head == NULL) {
        perror("Error reading head");
        return;
    }
    fgets(line, 1024, head);
    line[strcspn(line, "\n")] = '\0';
    fclose(head);
    strcpy(curent_branch, line+16);//will get the current branch name

    //open that current branch
    char curent_branch_path[1024];
    sprintf(curent_branch_path,"./.jit/refs/heads/%s",curent_branch);
    FILE* curent = fopen(curent_branch_path, "r");
    if (curent == NULL) {
        printf("error: no commits yet, nothing to branch from\n");
        return;
    }
    fgets(line, 1024, curent); //read the commit hash
    line[strcspn(line, "\n")] = '\0';
    fclose(curent);

    //finally write that hash in newly created branch file
    fprintf(new_branch, "%s", line);
    fclose(new_branch);
    printf("Branch %s created\n", branch_name);




}