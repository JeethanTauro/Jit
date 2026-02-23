//
// Created by jeethan on 23/02/26.
//
//normally an array of strings in C is just a 2d array like this
// arr[m][n] where m is the total words and n max number of letters in each word
// but it is not memory efficient
// so we use an array of pointers pointing to the first character of the string, and as strings are contiguos in memory we can access them easily

#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <sys/stat.h>

void addFile(char *arr[]) {
    //steps:
    //step 1: read the content of the file
    //step 2: store the content of the file in a buffer
    //step 3: run the sha1 algorithm on the buffer (the output has is 20 bytes raw which has to be converted to hex to get 40 characters)
    //step 4: divide the hash into 2 parts, the first 2 characters and then the rest.
    //step 5: create a directory in the objects of the name as the first 2 characters
    //step 6: create a file name in the created directory as the rest of the hashcode and add contents of buffer
    //step 7: update the index file
    char buffer[1024];
    for (int i=0; arr[i] != NULL; i++) {
        FILE* fptr = fopen(arr[i], "r");
        if (fptr == NULL) {
            printf("file not found: %s\n", arr[i]);
            continue;
        }

        const size_t length = fread(buffer, sizeof(char), 1024, fptr);
        fclose(fptr);
        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1(buffer, length, hash); //got the hashcode

        //converting to hex
        char hex[41]; // 40 chars + null terminator
        for (int j = 0; j < SHA_DIGEST_LENGTH; j++) {
            sprintf(hex + (j * 2), "%02x", hash[j]);
        }
        hex[40] = '\0';

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
            return;
        }
        fwrite(buffer, 1, length, f); //write the buffer contents to the blob file
        fclose(f);

        //updating the index
        FILE* index = fopen("./.jit/index", "a");
        if (index == NULL) {
            perror("failed to open index");
            return;
        }
        fprintf(index, "%s %s\n", hex, arr[i]);
        fclose(index);
    }
}
