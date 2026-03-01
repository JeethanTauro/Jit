#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include "check_branch.h"
//
// Created by jeethan on 01/03/26.
//
void check_branch() {
    //read head to get current branch
    //read heads to get all branches
    char line[1024];
    char current_branch[1024];
    FILE *head = fopen("./.jit/HEAD","r");
    if (head==NULL) {
        perror("Could not read head");
        return;
    }
    fgets(line,1024,head);
    strcpy(current_branch,line+16);
    current_branch[strcspn(current_branch,"\n")]='\0';

    DIR *d;
    struct dirent *dir;
    d = opendir("./.jit/refs/heads");
    printf("\n Branches created : ");
    if (d) {
        while ((dir=readdir(d)) != NULL) {
            if (strcmp(dir->d_name,".")==0 || strcmp(dir->d_name,"..")==0) {
                continue;
            }
            if (strcmp(dir->d_name,current_branch)==0) {
                printf("\n \t%s <-current branch\n",current_branch);
                continue;
            }
            printf("\n \t%s\n",dir->d_name);

        }
    }
}