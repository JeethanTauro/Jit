#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include "switch_branch.h"

#include <dirent.h>
//
// Created by jeethan on 01/03/26.
//
struct filename_hash {
    char filename[1024];
    char hash[41];
};
int check(char *branch_name);
int already_present_branch(char *branch_name);
char* commit_hash_from_branch(char *branch_name);
int check_local_modified();
char* calc_hashing(char buffer[] ,size_t len);

void switch_branch(char *branch_name) {
    //the current code i am only doing tree v/s index comparison
    // but i need to see index v/s working directory
    // current tree v/s target tree comparison too

    struct filename_hash f[100];
    struct filename_hash current_tree[100];
    int current_tree_count = 0;
    char commit_hash[41];
    char commit_file_path[1024];
    char folder[3];
    char file[39];
    char line[1024];
    char tree_hash[1024];
    char tree_path[1024];
    //first check if the branch exists or not
    if (!check(branch_name)) {
        printf("branch name is invalid\n");
        return;
    }

    //we have to check if we are already on this branch
    if (already_present_branch(branch_name)) {
        printf("already in this branch : %s \n",branch_name);
        return;
    }
    //if the user has made some local changes in that branch but hasnt staged the changes
    if (check_local_modified()) {
        printf("\nChanges not staged for commit:\n\t");
        return;
    }

    //the target tree has some files missing (in that branch its deleted), in the current tree/brnanch files exist so have to delete files


    //tree v/s index comparison, so if the user has staged but not commited yet
    //also what if the user has deleted something and then staged, that part is not implemented yet
    // read current branch commit hash
    char current_commit[41];
    char current_branch[1024];
    char head_line[1024];

    FILE* head_check = fopen("./.jit/HEAD", "r");
    if (head_check==NULL) {
        perror("HEAD could not be read\n");
        return;
    }
    fgets(head_line, 1024, head_check);
    fclose(head_check);
    strcpy(current_branch, head_line + 16);
    current_branch[strcspn(current_branch, "\n")] = '\0';

    char current_branch_path[1024];
    sprintf(current_branch_path, "./.jit/refs/heads/%s", current_branch);
    FILE* current_branch_file = fopen(current_branch_path, "r");
    if (current_branch_file==NULL) {
        perror("could not open current branch file");
        return;
    }

    fread(current_commit, 1, 40, current_branch_file);
    current_commit[40] = '\0';
    fclose(current_branch_file);

    // read current tree from current commit
    char current_commit_path[1024];
    char current_tree_hash[41];
    strncpy(folder, current_commit, 2); folder[2] = '\0';
    strncpy(file, current_commit + 2, 38); file[38] = '\0';
    sprintf(current_commit_path, "./.jit/objects/%s/%s", folder, file);
    FILE* current_commit_file = fopen(current_commit_path, "r");
    if (current_commit_file == NULL) {
        perror("could not open current commit file");
        return;
    }
    fgets(line, 1024, current_commit_file);
    fclose(current_commit_file);
    strcpy(current_tree_hash, line + 5);
    current_tree_hash[strcspn(current_tree_hash, " \n")] = '\0';

    // read current tree entries
    strncpy(folder, current_tree_hash, 2); folder[2] = '\0';
    strncpy(file, current_tree_hash + 2, 38); file[38] = '\0';
    char current_tree_path[1024];
    sprintf(current_tree_path, "./.jit/objects/%s/%s", folder, file);
    FILE* current_tree_file = fopen(current_tree_path, "r");
    if (current_tree_file == NULL) {
        perror("Could not read current read file \n");
        return;
    }
    while (fgets(line, 1024, current_tree_file) != NULL) {
        strncpy(current_tree[current_tree_count].hash, line + 5, 40);
        current_tree[current_tree_count].hash[40] = '\0';
        strcpy(current_tree[current_tree_count].filename, line + 46);
        current_tree[current_tree_count].filename[strcspn(current_tree[current_tree_count].filename, "\n")] = '\0';
        current_tree_count++;
        }
    fclose(current_tree_file);


    // read index
    struct filename_hash index_entries[100];
    int index_count = 0;
    FILE* index_read = fopen("./.jit/index", "r");
    if (index_read == NULL) {
        perror("Could not read index \n");
        return;
    }
    while (fgets(line, 1024, index_read) != NULL) {
        strncpy(index_entries[index_count].hash, line, 40);
        index_entries[index_count].hash[40] = '\0';
        strcpy(index_entries[index_count].filename, line + 41);
        index_entries[index_count].filename[strcspn(index_entries[index_count].filename, "\n")] = '\0';index_count++;

    }
    fclose(index_read);


    // compare index vs current tree
    int uncommitted = 0;
    for (int i = 0; i < index_count; i++) {
        int found = 0;
        for (int j = 0; j < current_tree_count; j++) {
            if (strcmp(index_entries[i].filename, current_tree[j].filename) == 0) {
                found = 1;
                if (strcmp(index_entries[i].hash, current_tree[j].hash) != 0) {
                    uncommitted++;
                }
                break;
            }
        }
        if (!found) uncommitted++;
    }

    if (uncommitted > 0) {
        printf("error: you have uncommitted changes\n");
        printf("please commit before switching branches\n");
        return;
    }
    for (int i = 0; i < current_tree_count; i++) {
        int found = 0;
        for (int j = 0; j < index_count; j++) {
            if (strcmp(current_tree[i].filename, index_entries[j].filename) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            uncommitted++;
        }
    }
    if (uncommitted > 0) {
        printf("error: you have uncommitted changes\n");
        printf("please commit before switching branches\n");
        return;
    }


    //now have to read the commit hash
    char *ptr = commit_hash_from_branch(branch_name);
    if ( ptr==NULL) {
        perror("could not get commit hash from branch\n");
        return;
    }
    strcpy(commit_hash,ptr);
    if (commit_hash[0] == '\0') {
        printf("could not open branch file\n");
        return;
    }
    commit_hash[strcspn(commit_hash,"\n")] = '\0';

    //now from the commit hash, we build the path to that commit file
    //from that commit file we get the tree hash
    //from the tree hash we get to the file where blobs are stored with the hash and filename
    strncpy(folder,commit_hash,2);
    folder[2]='\0';
    strncpy(file, commit_hash + 2, 38);
    file[38] = '\0';
    sprintf(commit_file_path, "./.jit/objects/%s/%s",folder,file);
    FILE* commit_file = fopen(commit_file_path, "r");
    if (commit_file == NULL) {
        perror("could not open file\n");
        return;
    }
    fgets(line, 1024, commit_file);
    fclose(commit_file);
    strcpy(tree_hash, line+5);
    tree_hash[strcspn(tree_hash,"\n")] = '\0';

    //we got the tree hash, now we have to go to the blob file by building the path
    strncpy(folder,tree_hash,2);
    folder[2]='\0';
    strncpy(file, tree_hash + 2, 38);
    file[38] = '\0';
    sprintf(tree_path,"./.jit/objects/%s/%s",folder,file);

        //now we will open the blob file and read the filename and the hash
    FILE* tree_file = fopen(tree_path, "r");
    if (tree_file == NULL) {
        perror("could not open file\n");
        return;
    }
    int file_count=0;
    //an array of structs of file name and hash
    while (fgets(line, 1024, tree_file) != NULL) {
        strncpy(f[file_count].hash,line+5,40); //got the hash
        f[file_count].hash[strcspn(f[file_count].hash,"\n")] = '\0';

        strcpy(f[file_count].filename,line+46); //got the filename
        f[file_count].filename[strcspn(f[file_count].filename,"\n")] = '\0';
        file_count++;
    }
    fclose(tree_file);
    for (int i = 0; i < current_tree_count; i++) {
        int found = 0;
        for (int j = 0; j < file_count; j++) {
            if (strcmp(current_tree[i].filename, f[j].filename) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            char delete_path[1024];
            sprintf(delete_path, "./%s", current_tree[i].filename);
            remove(delete_path);
        }
    }

    //compare the current working directory with the target tree
    DIR *dir;
    struct dirent *entry;
    dir = opendir(".");
    if (dir == NULL) {
        perror("could not open working directory");
        return;
    }
    while ((entry = readdir(dir)) != NULL) {

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        if (strcmp(entry->d_name, ".jit") == 0)
            continue;
        int found = 0;
        for (int i = 0; i < file_count; i++) {
            if (strcmp(entry->d_name, f[i].filename) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            char delete_path[1024];
            sprintf(delete_path, "./%s", entry->d_name);
            remove(delete_path);
        }
    }

    closedir(dir);

    //for each entry in the struct array
    //we have to go that file content and then overrite those files in disk
    //so first build the path to the file content from the hashes
    //read the entire content
    //open the file in disk and ovewrite that file
    char file_content_path[1024];
    char buffer[65536];
    char output_file_path[1024];
    for (int i=0;i<file_count;i++) {
        memset(buffer,'\0',65536);
        strncpy(folder,f[i].hash,2);
        folder[2]='\0';
        strncpy(file, f[i].hash + 2, 38);
        file[38] = '\0';

        sprintf(file_content_path, "./.jit/objects/%s/%s",folder,file);
        FILE* file_content = fopen(file_content_path, "r");
        if (file_content==NULL) {
            perror("could not opent the file content");
            return;
        }
        size_t len = fread(buffer, 1, sizeof(buffer), file_content);  //read that file
        fclose(file_content);

        //build the file path to be overwritten

        sprintf(output_file_path, "./%s",f[i].filename);
        FILE* fptr = fopen(output_file_path, "w");
        if (fptr == NULL) {
            perror("could not open file to be overwritten\n");
            return;
        }
        fwrite(buffer, 1, len, fptr); ; //overwriting the file
        fclose(fptr);
    }
    //now we have to update the index and then update the head

    FILE* index = fopen("./.jit/index", "w");

    if (index == NULL) {
        perror("could not open index\n");
        return;
    }
    for (int i=0;i<file_count;i++) {
        fprintf(index,"%s %s\n",f[i].hash,f[i].filename);
    }
    fclose(index);

    FILE* head = fopen("./.jit/HEAD", "w");
    if (head == NULL) {
        perror("could not open head\n");
        return;
    }
    fprintf(head,"ref: refs/heads/%s",branch_name);
    fclose(head);
    printf("\n Switched to branch %s\n",branch_name);
}

int check(char *branch_name){
    char branch_path[1024];
    sprintf(branch_path,"./.jit/refs/heads/%s",branch_name);
    FILE* f = fopen(branch_path, "r");
    if (f == NULL) {
        return 0;
    }
    fclose(f);
    return 1;
}
int already_present_branch(char *branch_name) {
    char line[1024];
    char current_branch_name[1024];
    FILE *head = fopen("./.jit/HEAD", "r");
    if (head == NULL) {
        printf("could not open head\n");
        return 1;
    }
    fgets(line, 1024, head);
    fclose(head);
    strcpy(current_branch_name, line+16);
    current_branch_name[strcspn(current_branch_name,"\n")] = '\0';

    if (strcmp(current_branch_name, branch_name) == 0) {
        return 1;
    }
    return 0;
}
char* commit_hash_from_branch(char* branch_name) {
    char branch_path[1024];
    char line[1024];
    static char commit_hash[1024];
    sprintf(branch_path,"./.jit/refs/heads/%s",branch_name);
    FILE* f = fopen(branch_path, "r");
    if (f != NULL) {
        fgets(line, 1024, f);
        fclose(f);
        strcpy(commit_hash, line);
        commit_hash[strcspn(commit_hash,"\n")] = '\0';
        return commit_hash;
    }
    return NULL;

}

int check_local_modified() {
    struct filename_hash f;
    int changes = 0;
    FILE* fptr = fopen("./.jit/index", "r");
    if (fptr == NULL) {
        perror("error opening index");
        return 1;
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
            changes++;
            continue;
        }
        size_t len  = fread(buffer,sizeof(char),65536,file);
        fclose(file);
        //get the hash of the file
        strcpy(hash,calc_hashing(buffer, len));
        if (strcmp(f.hash,hash) != 0) {
            changes++;
        }
    }
    if (changes) {
        fclose(fptr);
        return 1;
    }
    fclose(fptr);
    return 0;
}