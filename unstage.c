//
// Created by jeethan on 28/02/26.
//

#include <stdio.h>
#include <string.h>
#include "unstage.h"
struct file_and_hash {
    char filename[100];
    char hash[41];
};

void unstage(char *filename) {
    struct file_and_hash entries[100];
    FILE* indexRead = fopen("./.jit/index","r");
    if (indexRead == NULL) {
        perror("\n Error opening index file");
        return;
    }
    int count = 0;
    //reading all the hashes and the file names
    if (indexRead != NULL) {
        char indexLine[1024];
        while (fgets(indexLine, sizeof(indexLine), indexRead) != NULL) {
            strncpy(entries[count].hash, indexLine, 40);
            entries[count].hash[40] = '\0';
            strcpy(entries[count].filename, indexLine + 41);
            entries[count].filename[strcspn(entries[count].filename, "\n")] = '\0';
            count++;
        }
        fclose(indexRead);
    }
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].filename, filename) == 0) {
            found = 1;// the found flag here is for error handling to check whether the file name the user gave exists or not
            break;
        }
    }
    if (!found) {
        printf("error: %s not in staging area\n", filename);
        return;
    }
    FILE* indexWrite = fopen("./.jit/index", "w");
    if (indexWrite == NULL) {
        perror("Could not open index");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].filename, filename) != 0) {
            fprintf(indexWrite, "%s %s\n", entries[i].hash, entries[i].filename);
        }
    }
    fclose(indexWrite);
    printf("\n unstaged: %s\n", filename);
}
