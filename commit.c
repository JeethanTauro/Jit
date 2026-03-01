//
// Created by jeethan on 24/02/26.
//
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <time.h>
struct file_and_hash {
    char filename[1024];
    char hash[41];
};
char* hashing_and_storing(char treeContent[]);
int check_for_commit();

int commitFile(char str[]) {
    int successful = 0;
    //first see if jit is initialised
    FILE* i = fopen("./.jit/index", "r");
    if (i == NULL) {
        perror("failed to open index");
        return 0;
    }
    fclose(i);

    //also first see if anything is there to commit or not
    if (check_for_commit() == 0) return 0;


    //step 1 : read the index line by line
    //step 2: build the string with new lines and append the word blob infront of the lines (right now we are handling only files, once we handle subdirectories we would have to append the word tree)
    //step 3: now take the entire string and hash it, and store inside the objects in the same format we did before and then inside the file store the built string (this is the tree hash)
    //step 4: build the commit string and do the same thing, hash it, store it in objects
    //step 5: Read the HEAD file, get the ref, extract branch name and then write the new commit hash into .jit/refs/heads/master


    //sub steps of step 4:
            //1) read the tree hash we just made
            //2) read the parent hash from refs/head (if empty then none)
            //3) get current timestamp
            //4) combine all these then hash and store it in objects

    //<----Step 1 to 3---->
    //reading index file and built the string
    FILE* fptr = fopen("./.jit/index","r");
    if (fptr == NULL) {
        perror("Couldn't open file");
        return 0;
    }
    char treeContent[4096] = "";
    char line[256];
    while (fgets(line, sizeof(line), fptr)) {
        char entry[300]; //this entry holds blob + line
        snprintf(entry, sizeof(entry), "blob %s", line);
        strcat(treeContent, entry); // we are appending it to treeContent
    }
    fclose(fptr);


    char treeHash[41];
    strcpy(treeHash, hashing_and_storing(treeContent));

    //<--step 4--->
    //building the commit message

    //timestamp
    time_t timestamp;
    time(&timestamp);

    //reading parent hash
    char current_branch[1024];
    char path[1024];
    FILE* head = fopen("./.jit/HEAD","r");
    if (head==NULL) {
        perror("Couldn't open HEAD");
        return 0;
    }
    fgets(line,1024,head);
    strcpy(current_branch,line+16);
    current_branch[strcspn(current_branch, "\n")] = '\0';
    fclose(head);

    sprintf(path,"./.jit/refs/heads/%s",current_branch);

    FILE* parentHashFile = fopen(path,"r");
    char parentHash[41] = "none";
    if (parentHashFile == NULL) {
        //that means this file doesnt exist, its the first commit
        perror("failed to open parentHashFile");
    }
    else {
        fread(parentHash, 1, 41, parentHashFile);
        fclose(parentHashFile);
    }



    //writing a commit in memory using buffer and then hash and store it

    char commitHash[41];
    char commitContent[1024];
    snprintf(commitContent, sizeof(commitContent),
        "tree %s\nparent %s\nauthor Jeethan\ntimestamp %ld\nmessage %s\n",
        treeHash, parentHash, (long)timestamp, str);
    // now hash commitContent directly, no need to write to file first
    strcpy(commitHash, hashing_and_storing(commitContent));

    //<---Step 5--->


    char readLine[256];
    char branch[100];
    FILE* HeadFile = fopen("./.jit/HEAD", "r");
    if (HeadFile == NULL) {
        perror("Failed to open heads/HEAD");
        return 0;
    }
    fgets(readLine, sizeof(readLine), HeadFile);
    sscanf(readLine, "ref: refs/heads/%s", branch);
    fclose(HeadFile);


    char masterPath[200];
    snprintf(masterPath, sizeof(masterPath), "./.jit/refs/heads/%s", branch);
    FILE* master = fopen(masterPath, "w");
    fprintf(master, "%s", commitHash);
    fclose(master);
    successful = 1;
    return successful;
}

char* hashing_and_storing(char treeContent[]) {
    size_t len = strlen(treeContent);
    //hashing and storing the string
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(treeContent, len, hash); //got the hashcode

    //converting to hex
    static char hex[41]; // 40 chars + null terminator
    for (int j = 0; j < SHA_DIGEST_LENGTH; j++) {
        sprintf(hex + (j * 2), "%02x", hash[j]);
    }
    hex[40] = '\0'; //final hex hash

    char dir[3];   // 2 chars + null terminator
    char file[39]; // 38 chars + null terminator

    strncpy(dir, hex, 2);
    dir[2] = '\0';

    strncpy(file, hex + 2, 38);
    file[38] = '\0';

    char dirPath[50];
    char filePath[100];

    //building the file path
    snprintf(dirPath, sizeof(dirPath), "./.jit/objects/%s", dir);
    snprintf(filePath, sizeof(filePath), "./.jit/objects/%s/%s", dir, file);

    mkdir(dirPath, 0755);
    FILE* f = fopen(filePath, "w");
    if (f == NULL) {
        perror("failed to create blob");
    }
    fwrite(treeContent, 1, len, f); //write the treeContent contents to the blob file
    fclose(f);

    return hex;

}

int check_for_commit() {
    struct file_and_hash index[30];
    char line[1024];
    FILE* i = fopen("./.jit/index", "r");
    if (i == NULL) {
        perror("failed to open index");
    }
    int k=0;
    while (fgets(line, sizeof(line), i)) {
        strncpy(index[k].hash,line,40); //copy the hash into the struct
        index[k].hash[40] = '\0';
        strcpy(index[k].filename,line+41); //copy the file name into the struct
        index[k].filename[strcspn(index[k].filename, "\n")] = '\0'; //replace the new line with \0
        k++;
    }
    fclose(i);
    if (k==0) {
        printf("\nNothing to commit, staging area empty\n");
        return 0;
    }
    char latest_commit[41];
    char current_branch[1024];
    char path[1024];
    FILE* head = fopen("./.jit/HEAD","r");
    if (head==NULL) {
        perror("Couldn't open HEAD");
        return 0;
    }
    fgets(line,1024,head);
    strcpy(current_branch,line+16);
    current_branch[strcspn(current_branch, "\n")] = '\0';
    fclose(head);

    sprintf(path,"./.jit/refs/heads/%s",current_branch);

    FILE* branch = fopen(path, "r");
    if (branch == NULL) {
         return 1; //first commit so proceed.
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
        return 0;
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
    int changes = 0;
    for (int i = 0; i < k; i++) {
        int found = 0;
        for (int j = 0; j < tree_count; j++) {
            if (strcmp(index[i].filename, tree[j].filename) == 0) {
                found = 1;
                if (strcmp(index[i].hash, tree[j].hash) != 0) {
                    changes++;
                }
                break;
            }
        }
        if (!found) {
           changes++;
        }
    }
    if (changes == 0) {
        printf("\nNothing to commit, no modifications made\n");
        return 0;
    }
    return 1;
}
