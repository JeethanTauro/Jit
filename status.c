//
// Created by jeethan on 26/02/26.
//

//There are mainly three comparisons to make

//Comparison 1:  modified and unstaged
    // SO basically we have to read the index file line by line, get the hash and the file name
    // and then check for that file in the disk and hash it
    // if the file doesnt exist, that means the file has been deleted and its not staged
    // if the file exists and the hash is different , that means that file has benn modified and is not staged
//Comparison 2: tells about untracked files
    // so if the files are present on the disk but they are not present on the index that means
    // those files are untracked

//Comparison 3: staged yet to commit
    // have to read the index files, get the hash and the file name
    // then check for the latest commit in the current branch (master)
    // then we go to that commit file and then get the tree hash
    // then go to that tree objects and read all the content into array of structs (with hash and file name)
    // now compare the hash of the file with the hash in index, if same nothing to commit, if different then staged and yet to commit
    // if the file is not present in the array of struct but present in the index that means file has been deleted

#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <dirent.h>
#include "status.h"

#include <stdlib.h>

char* calc_hashing(char buffer[], size_t len);
void modified();
void untracked();
void staged_yet_to_commit();


struct file_and_hash {
    char filename[1024];
    char hash[41];
};
void status() {


    //Comparison 1 :Working directry and index
    //<-----------modified-------------->
    modified();

    //<-----------untracked files---------------->
    untracked();

    //<---------staged yet to commit---------->
    staged_yet_to_commit();
}

void modified() {
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
            printf("\nChanges not staged for commit:\n\t  modified: %s\n",f.filename);
        }
    }
    fclose(fptr);
}
void untracked() {
    char *ignore_filenames[100];
    int ignore_exists = 0;
    char ignore_line_content[1024];
    int ignore_count = 0;
    FILE* fptr = fopen("./.jit/index", "r");
    if (fptr == NULL) {
        perror("error opening index");
    }
    FILE* ignore_file = fopen("./.jitignore","r"); //open the file in jit ignore
    if (ignore_file != NULL) {
        ignore_exists=1;
        //read the .jitignore file
       while (fgets(ignore_line_content,sizeof(ignore_line_content),ignore_file)!=NULL) {
           ignore_line_content[strcspn(ignore_line_content,"\n")] = '\0';
           ignore_filenames[ignore_count] = malloc(strlen(ignore_line_content) + 1);
           strcpy(ignore_filenames[ignore_count],ignore_line_content);
           ignore_count++;
       }
    }
    char line[1024];
    char filename[1024];
    printf("\n Untracked: \n \t \n");
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
            int ignored = 0;
            if (ignore_exists) {
                for (int i=0; i<ignore_count;i++) {
                    if (strcmp(dir->d_name,ignore_filenames[i]) == 0) {
                        ignored = 1;
                        break;
                    }
                }
            }
            if (ignored) continue;
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
    //freeing the allocated memory
    for (int i=0; i<untracked_count;i++) {
        printf("\t  %s\n",untracked[i]);
        free(untracked[i]); // Free untracked
    }
    for (int i = 0; i < index_count; i++) free(filenames_in_index[i]);
    for (int i = 0; i < dir_count; i++) free(filenames_in_dir[i]);
    for (int i = 0; i < ignore_count; i++) free(ignore_filenames[i]);
}
void staged_yet_to_commit() {
    FILE* fptr = fopen("./.jit/index", "r");
    if (fptr == NULL) {
        perror("error opening index");
    }
    char line[1024];
    struct file_and_hash index[30];
    int k=0;
    while (fgets(line,1024,fptr)!= NULL) {
        strncpy(index[k].hash,line,40); //copy the hash into the struct
        index[k].hash[40] = '\0';
        strcpy(index[k].filename,line+41); //copy the file name into the struct
        index[k].filename[strcspn(index[k].filename, "\n")] = '\0'; //replace the new line with \0
        k++;
    }
    fclose(fptr);
    char latest_commit[1024];

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

    FILE* branch = fopen(path, "r");
    if (branch == NULL) {
        if (k > 0) {
            printf("\nChanges to be committed:\n");
            for (int i = 0; i < k; i++) {
                printf("\t  %s\n", index[i].filename);
            }
        }
        return;
    }
    fread(latest_commit, 1, 40, branch);//reading the latest commit hash
    fclose(branch);
    latest_commit[40] = '\0';


    //building the path to get the tree hash from the commit hash
    char tree_hash_path[1024];
    char folder[3];
    char file[40];
    strncpy(folder,latest_commit,2); //get the folder name
    strncpy(file,latest_commit+2,38); //get the filename
    folder[2] = '\0';
    file[38] = '\0';
    snprintf(tree_hash_path,sizeof(tree_hash_path),"./.jit/objects/%s/%s",folder,file); //built the filepath



    //opening the file to read the tree hash from the commit object
    char tree_hash[1024];
    char commit_path[1024];
    FILE* commit_file = fopen(tree_hash_path,"r");
    if (commit_file == NULL) {
        perror("Could not open commit_file");
    }
    fgets(tree_hash,sizeof(tree_hash),commit_file); //reading the first line to the the 'tree hash'
    fclose(commit_file);
    tree_hash[strcspn(tree_hash, "\n")] = '\0';
    memmove(tree_hash, tree_hash+5, strlen(tree_hash+5)+1);//overwriting it to get only the tree hash
    tree_hash[strcspn(tree_hash, " \n")] = '\0';

    //now build the path to the content of the tree its in the form of 'blob hash filename'
    //we need the hash and the file name
    strncpy(folder,tree_hash,2);
    folder[2] = '\0';
    strncpy(file,tree_hash+2,38);
    file[38] = '\0';
    snprintf(commit_path,sizeof(commit_path),"./.jit/objects/%s/%s",folder,file);

    FILE* tree_file = fopen(commit_path,"r");
    if (tree_file == NULL) {
        perror("Could not open tree file");
        return;
    }

    //reading the hash and file name
    struct file_and_hash tree[30];

    int tree_count =0;
    while (fgets(line,1024,tree_file) != NULL) {
        strncpy(tree[tree_count].hash,line+5,40);
        tree[tree_count].hash[40] = '\0';
        strcpy(tree[tree_count].filename,line+46);
        tree[tree_count].filename[strcspn(tree[tree_count].filename, "\n")] = '\0';
        tree_count++;
    }
    fclose(tree_file);

    // compare index vs tree
    for (int i = 0; i < k; i++) {
        int found = 0;
        for (int j = 0; j < tree_count; j++) {
            if (strcmp(index[i].filename, tree[j].filename) == 0) {
                found = 1;
                if (strcmp(index[i].hash, tree[j].hash) != 0) {
                    printf("\nChanges to be committed:\n\t  modified: %s\n", index[i].filename);
                }
                break;
            }
        }
        if (!found) {
            printf("\nChanges to be committed:\n\t  new file: %s\n", index[i].filename);
        }
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