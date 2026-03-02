#include <stdio.h>
#include <string.h>
#include "restore.h"
//
// Created by jeethan on 02/03/26.
//
void restore(char *filename) {
    int found = 0;
    char buffer[1024];
    char line[1024];
    char path[1024];
    char hash[41];
    char folder[3];
    char file[39];
    char content[65536];
    char disk_path[1024];

    FILE *fptr = fopen("./.jit/index","r");
    if(fptr == NULL) {
        printf("Error opening index file\n");
        return;
    }
    while (fgets(buffer,sizeof(buffer),fptr)!=NULL) {
        buffer[strcspn(buffer,"\n")]='\0';
        strcpy(line,buffer+41);
        line[strcspn(line,"\n")]='\0';

        if (strcmp(line,filename)==0) {
            found = 1;
            strncpy(hash,buffer,40);
            hash[40]='\0';
            strncpy(folder,hash,2);
            folder[2]='\0';

            strncpy(file, hash + 2, 38);
            file[38] = '\0';
            sprintf(path,"./.jit/objects/%s/%s",folder,file);
            FILE *f2 = fopen(path,"r");
            if (f2 == NULL) {
                printf("Error opening content file\n");
                return;
            }
            size_t len = fread(content,1,sizeof(content),f2);
            fclose(f2);

            sprintf(disk_path,"./%s",filename);

            FILE* f = fopen(disk_path,"w");
            if (f == NULL) {
                printf("Error opening file on disk\n");
            }
            fwrite(content,1,len,f);
            fclose(f);
            printf("\nRestored file %s\n",filename);
        }
    }
    fclose(fptr);
    if (!found) {
        printf("\nFile not found in staging area\n");
    }

}