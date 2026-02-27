//
// Created by jeethan on 26/02/26.
//

//There are mainly three comparisons to make

//Comparison 1:  modified and unstaged
    // SO basically we have to read the index file line by line, get the hash and the file name
    // and then check for that file in the disk and hash it
    // if the file doesnt exist, that means the file has been deleted and its not staged
    // if the file exists and the hash is different , that means that file has benn modified and is not staged

#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <dirent.h>
#include "status.h"

#include <stdlib.h>

char* calc_hashing(char buffer[], size_t len);

struct file_and_hash {
    char filename[1024];
    char hash[41];
};
void status() {


    //Comparison 1 :Working directry and index
    struct file_and_hash f;
    FILE* fptr = fopen("./.jit/index", "r");
    if (fptr == NULL) {
        perror("error opening index");
    }
    char line[1024];
    char filename[1024];
    char hash[41];
    //reading the index line by line
    while (fgets(line, 1024, fptr) != NULL) {
        //getting the hash value from the index
        strncpy(hash,line,40);
        hash[40] = '\0';

        //getting the filename from the index
        strcpy(filename,line+41);
        filename[strcspn(filename, "\n")] = '\0';

        //storing in struct
        strcpy(f.filename,filename);
        strcpy(f.hash,hash);

        //search for the file in current directory and hash and then compare it
        char buffer[65536];
        FILE* file = fopen(filename,"r");
        if (file == NULL) {
            printf("File %s deleted but not staged", f.filename);
            continue;
        }
        size_t len  = fread(buffer,sizeof(char),65536,file);
        fclose(file);
        //get the hash of the file
        strcpy(hash,calc_hashing(buffer, len));
        if (strcmp(f.hash,hash) != 0) {
            printf("\n Modified unstaged: \n \t %s \n",f.filename);
        }
    }
    fclose(fptr);

    //untracked files
    printf("\n Untracked: \n \t \n");
    fptr = fopen("./.jit/index", "r");
    int  index_count = 0;
    char *filenames_in_index[1000];//list of the filenames in th eindex file
    char *filenames_in_dir[1000];
    while (fgets(line, 1024, fptr) != NULL) {
        strcpy(filename,line+41);
        filename[strcspn(filename, "\n")] = '\0';
        // filenames_in_index[i++] = filename; this is wrong because after the iteration, all the indexes point to the varibale filename which is wrong

        filenames_in_index[index_count] = malloc(strlen(filename) + 1); // here every filename will return a new address in memory
        strcpy(filenames_in_index[index_count], filename); //this is correct
        index_count++;
    }
    fclose(fptr);
    DIR* d;
    struct dirent* dir; //dir is a pointer to a structure of the type dirent
    d = opendir(".");
    int dir_count=0;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] == '.') {
                continue;
            }
            if (dir->d_type == DT_DIR) continue;
            filenames_in_dir[dir_count] = malloc(strlen(dir->d_name) + 1);
            strcpy(filenames_in_dir[dir_count],dir->d_name); //storing the filename which are in the current directory
            dir_count++;
        }
        closedir(d);
    }
    //now have to compare these two arrays
    char *untracked[1000];
    int untracked_count = 0;
    for (int i = 0; i < dir_count; i++) {
        if (strcmp(filenames_in_dir[i], "jit") == 0) continue;
        int found = 0;

        for (int j = 0; j < index_count; j++) {
            if (strcmp(filenames_in_dir[i], filenames_in_index[j]) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            untracked[untracked_count] = malloc(strlen(filenames_in_dir[i]) + 1);
            strcpy(untracked[untracked_count], filenames_in_dir[i]);
            untracked_count++;
        }
    }
    for (int i=0; i<untracked_count;i++) {
        printf("\t  %s\n",untracked[i]);
    }
}


char* calc_hashing(char buffer[] ,size_t len) {
    //hashing and storing the string
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(buffer, len, hash); //got the hashcode

    //converting to hex
    static char hex[41]; // 40 chars + null terminator
    for (int j = 0; j < SHA_DIGEST_LENGTH; j++) {
        sprintf(hex + (j * 2), "%02x", hash[j]);
    }
    hex[40] = '\0'; //final hex hash

    return hex;
}