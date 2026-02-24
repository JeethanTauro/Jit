//
// Created by jeethan on 22/02/26.
//
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "init.h"

void createJitDir() {
    DIR* dir = opendir("./.jit");

    //if the directory exists, then close
    if(dir){
        printf("\n Already Initialised");
        closedir(dir);
    }
    //if the directory doesn't exist
    else if(ENOENT == errno){
        const char* headDir="./.jit/refs/heads";
        const char* refsDir="./.jit/refs";
        const char* objectsDir="./.jit/objects";
        const char* rootDir = "./.jit";


        if (mkdir(rootDir,0755) == 0) {
            printf("\n Jit root directory created");
        }
        if (mkdir(objectsDir,0755) == 0) {
            printf("\n objects directory created");
        }
        if (mkdir(refsDir,0755) == 0) {
            printf("\n refs directory created");
        }
        if (mkdir(headDir,0755) == 0) {
            printf("\n head directory created");

            //creating a HEAD file
            FILE* file_ptr = fopen("./.jit/HEAD","w");
            if(file_ptr == NULL) {
                perror("\n failed to create file");
                return;
            }
            char str[] = "ref: refs/heads/master";

            //writing to the HEAD file
            fprintf(file_ptr,"%s",str);
            fclose(file_ptr);
            printf("\n HEAD file created");
        }
        //creating the index file
        FILE* file_ptr = fopen("./.jit/index","w");
        if(file_ptr == NULL) {
            perror("\n failed to create file");
            return;
        }
        fclose(file_ptr);
        printf("\n index file created");

    }
    else {
        perror("\n Error creating directory");
    }

}