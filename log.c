#include <stdio.h>
#include <string.h>
#include "log.h"

#include <stdlib.h>
#include <time.h>
//
// Created by jeethan on 28/02/26.
//

//Steps:
    //1) read the master file for the latest commit hash
    //2) build the path to that commit file from the hash
    //3) print the contents of that file and then read the parent commit hash
    //4) do this till parent is none

void jit_log(){
    char current_hash[41];
    char line[1024];
    char current_branch[1024];
    char path[1024];
    FILE* head = fopen("./.jit/HEAD","r");
    if (head==NULL) {
        perror("Couldn't open HEAD");
        return;
    }
    fgets(line,1024,head);
    strcpy(current_branch,line+16);
    current_branch[strcspn(current_branch, "\n")] = '\0';
    fclose(head);

    sprintf(path,"./.jit/refs/heads/%s",current_branch);
    FILE* branch = fopen(path,"r");
    if(branch == NULL) {
        perror("Couldn't open master");
        return;
    }
    fread(current_hash,1,40,branch);//read the latest commit hash
    current_hash[40] = '\0';
    fclose(branch);
    char folder[3];
    char filename[1024];
    char commit_file_path[1024];
    char commit_content_line[1024];

    while (strcmp(current_hash,"none")!=0) {
        int line_count = 0;
        strncpy(folder,current_hash,2); //copying the first 2 characters
        folder[2] = '\0';
        strcpy(filename,current_hash+2);
        filename[strcspn(filename, "\n")] = '\0';

        snprintf(commit_file_path,sizeof(commit_file_path),"./.jit/objects/%s/%s",folder,filename);
        FILE* commit_file = fopen(commit_file_path,"r");
        if(commit_file == NULL) {
            perror("Couldn't open commit file");
            return;
        }

        while (fgets(commit_content_line,sizeof(commit_content_line),commit_file) != NULL) {

            if (strncmp(commit_content_line, "timestamp", 9) == 0) {
                time_t t = atol(commit_content_line + 10);  // skip "timestamp "
                struct tm* tm_info = localtime(&t);
                char date[64];
                strftime(date, sizeof(date), "%a %b %d %H:%M:%S %Y", tm_info);
                printf("\n date: %s\n", date);
            } else {
                printf("\n %s", commit_content_line);
            }
            line_count++;
            if (line_count==2) {
                //here the current_hash must have the value of the parent hash in the commit file
                strncpy(current_hash,commit_content_line+7,sizeof(current_hash));
                current_hash[40] = '\0';
                current_hash[strcspn(current_hash, "\n")] = '\0';
            }

        }
        printf("\n--------------------------------------------------------\n");
        fclose(commit_file);
    }

}