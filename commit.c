//
// Created by jeethan on 24/02/26.
//
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <time.h>

char* hashing_and_storing(char treeContent[]);

int commitFile(char str[]) {
    int successful = 0;
    //first see if jit is initialised
    FILE* i = fopen("./.jit/index", "r");
    if (i == NULL) {
        perror("failed to open index");
        return 0;
    }
    fclose(i);
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
    FILE* parentHashFile = fopen("./.jit/refs/heads/master","r");
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

