#include <stdio.h>
#include <string.h>
#include <dirent.h>
//
// Created by jeethan on 01/03/26.
//
struct file_and_hash_reset {
    char filename[1024];
    char hash[41];
};

void reset(char* commit_hash) {
    printf("warning: this will reset your branch to commit %.7s\n", commit_hash);
    printf("all commits after this point will be lost!\n");
    printf("do you want to continue? (yes/no): ");

    char response[10];
    fgets(response, sizeof(response), stdin);
    response[strcspn(response, "\n")] = '\0';

    if (strcmp(response, "yes") != 0) {
        printf("reset aborted\n");
        return;
    }
    char folder[3];
    char file[39];
    char line[1024];
    char commit_path[1024];
    char tree_hash[1024];
    char tree_path[1024];

    // step 2 - verify commit exists
    printf("\n commit hash given as argument : %s \n",commit_hash);
    strncpy(folder, commit_hash, 2);
    folder[2] = '\0';
    strncpy(file, commit_hash + 2, 38);
    file[38] = '\0';
    snprintf(commit_path, sizeof(commit_path), "./.jit/objects/%s/%s", folder, file);
    printf("\n commit path %s\n",commit_path);
    FILE* commit_file = fopen(commit_path, "r");
    if (commit_file == NULL) {
        printf("fatal: commit not found\n");
        return;
    }

    // step 3 - extract tree hash from commit object
    fgets(line, sizeof(line), commit_file);
    printf("\nline read %s\n",line);
    fclose(commit_file);
    strcpy(tree_hash, line + 5);
    tree_hash[strcspn(tree_hash, " \n")] = '\0';

    printf("\n tree hash : %s\n",tree_hash);
    // step 4 - open tree object and read all blob entries
    strncpy(folder, tree_hash, 2);
    folder[2] = '\0';
    strncpy(file, tree_hash + 2, 38);
    file[38] = '\0';
    snprintf(tree_path, sizeof(tree_path), "./.jit/objects/%s/%s", folder, file);

    FILE* tree_file = fopen(tree_path, "r");
    if (tree_file == NULL) {
        printf("fatal: tree object not found\n");
        return;
    }

    struct file_and_hash_reset entries[100];
    int count = 0;
    while (fgets(line, sizeof(line), tree_file) != NULL) {

        printf("raw tree line: '%s'\n", line);
        strncpy(entries[count].hash, line + 5, 40);
        entries[count].hash[40] = '\0';
        strcpy(entries[count].filename, line + 46);
        entries[count].filename[strcspn(entries[count].filename, "\n")] = '\0';
        count++;
    }
    fclose(tree_file);

    // step 5 - restore every file to disk
    char blob_path[1024];
    char buffer[65536];
    char output_path[1024];
    for (int i = 0; i < count; i++) {
        strncpy(folder, entries[i].hash, 2);
        folder[2] = '\0';
        strncpy(file, entries[i].hash + 2, 38);
        file[38] = '\0';
        snprintf(blob_path, sizeof(blob_path), "./.jit/objects/%s/%s", folder, file);
        FILE* blob = fopen(blob_path, "r");
        if (blob == NULL) {
            printf("fatal: blob not found for %s\n", entries[i].filename);
            return;
        }
        size_t len = fread(buffer, 1, sizeof(buffer), blob);
        fclose(blob);

        snprintf(output_path, sizeof(output_path), "./%s", entries[i].filename);
        FILE* restored = fopen(output_path, "w");
        if (restored == NULL) {
            printf("fatal: could not restore %s\n", entries[i].filename);
            return;
        }
        fwrite(buffer, 1, len, restored);
        fclose(restored);
    }

    // step 6 - delete files not in target tree
    DIR* d = opendir(".");
    struct dirent* dir;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] == '.') continue;
            if (dir->d_type == DT_DIR) continue;
            if (strcmp(dir->d_name, "jit") == 0) continue;

            int found = 0;
            for (int i = 0; i < count; i++) {
                if (strcmp(dir->d_name, entries[i].filename) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                char delete_path[1024];
                snprintf(delete_path, sizeof(delete_path), "./%s", dir->d_name);
                remove(delete_path);
            }
        }
        closedir(d);
    }

    // step 7 - update index
    FILE* index = fopen("./.jit/index", "w");
    if (index == NULL) {
        printf("fatal: could not update index\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        fprintf(index, "%s %s\n", entries[i].hash, entries[i].filename);
    }
    fclose(index);

    // step 8 - update current branch to point to reset commit
    char current_branch[1024];
    char branch_path[1024];
    FILE* head = fopen("./.jit/HEAD", "r");
    if (head == NULL) {
        printf("fatal: could not open HEAD\n");
        return;
    }
    fgets(line, sizeof(line), head);
    fclose(head);
    strcpy(current_branch, line + 16);
    current_branch[strcspn(current_branch, "\n")] = '\0';

    snprintf(branch_path, sizeof(branch_path), "./.jit/refs/heads/%s", current_branch);
    FILE* branch = fopen(branch_path, "w");
    if (branch == NULL) {
        printf("fatal: could not update branch\n");
        return;
    }
    fprintf(branch, "%s", commit_hash);
    fclose(branch);

    // step 9 - confirmation
    printf("HEAD is now at %.7s\n", commit_hash);
}