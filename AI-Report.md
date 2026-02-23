# Jit — My Custom Version Control System
### A learning journal + project reference

---

## Table of Contents
1. [What is Jit?](#what-is-jit)
2. [Git Internals — The Mental Model](#git-internals)
3. [File Structure](#file-structure)
4. [Header Files Explained](#header-files)
5. [C Concepts Used](#c-concepts)
6. [Functions Reference](#functions-reference)
7. [jit init — Deep Dive](#jit-init)
8. [Build & Run](#build-and-run)

---

## 1. What is Jit? <a name="what-is-jit"></a>

Jit is a simplified version control system built in C, inspired by Git. The goal is to understand how Git works internally by rebuilding its core features from scratch.

---

## 2. Git Internals — The Mental Model <a name="git-internals"></a>

### Git is a DAG
Git's commit history is a **Directed Acyclic Graph**. Each commit points to its parent. Branches can diverge and merge, but there are never any cycles.

```
a1 ← a2 ← a3 (master)
```

### The Three Areas
```
Working Directory → (git add) → Index/Staging Area → (git commit) → Repository
```
- **Working Directory** — your actual files on disk
- **Index (Staging Area)** — `.jit/index`, a list of files ready to be committed
- **Repository** — `.jit/objects/`, where all snapshots are permanently stored

### Git Objects — Everything is One of Three Things

#### Blob
- Stores the **complete contents** of a file at the moment it was added
- Does NOT know its own filename — the filename lives in the index/tree, not the blob
- Named by the SHA1 hash of its contents
- If a file doesn't change between commits, the same blob is reused — no duplication
- Every change to a file = new blob

#### Tree
- Represents a **directory**
- Contains a list of entries, each with a mode (file or folder), a filename, and a hash pointing to a blob or another tree
- Trees are recursive — a tree can point to other trees for subdirectories

```
root tree
  ├── blob → main.c
  ├── blob → README.md
  └── tree → src/
        ├── blob → utils.c
        └── blob → helper.c
```

When a file changes, a new blob is created. Because the blob changed, its parent tree must be recreated to point to the new blob. That new tree causes its parent tree to be recreated, all the way up to the root. Unchanged files and folders are reused as-is.

#### Commit
- Points to one **root tree** (the full snapshot of your project)
- Points to its **parent commit** (except the very first commit)
- Contains author, timestamp, and message

### Hashing (SHA1)
- Every object (blob, tree, commit) is named by its **SHA1 hash**
- SHA1 takes any input and produces a 40 character hex string
- Same input always = same hash. One character of difference = completely different hash
- This is how Git detects changes — if the hash changes, the content changed

### Object Storage
Objects are stored in `.jit/objects/` using the first 2 characters of the hash as a folder name and the remaining 38 characters as the filename:

```
hash:      2e65ff9abc123...
stored at: .jit/objects/2e/65ff9abc123...
```
This avoids having thousands of files in one folder which would slow down the filesystem.

### HEAD
- A file at `.jit/HEAD`
- Points to the current branch
- Contents: `ref: refs/heads/master`
- The branch file (`.jit/refs/heads/master`) will eventually contain the hash of the latest commit on that branch

### Index File
- Located at `.jit/index`
- Maps filenames to blob hashes
- Updated every time you do `jit add`
- In our simplified version, stored as plain text:
```
2e65ff9a main.c
8f3a21bc utils.c
```

---

## 3. File Structure <a name="file-structure"></a>

### Project source files:
```
Jit/
  main.c          ← entry point, parses commands and calls functions
  init.c          ← jit init implementation
  init.h          ← init function declarations
  CMakeLists.txt  ← tells CLion/CMake how to build the project
  .gitignore      ← tells Git what files to ignore
```

### .jit folder (created by jit init):
```
.jit/
  objects/              ← will store all blobs, trees, commits
  refs/
    heads/
      HEAD              ← will contain hash of latest commit on this branch
  HEAD                  ← contains "ref: refs/heads/master"
  index                 ← staging area, starts empty
```

---

## 4. Header Files Explained <a name="header-files"></a>

Header files (`.h`) are like a "table of contents" for a `.c` file. They declare what functions exist so other files can use them without seeing the full implementation.

### The pattern:
```
init.h    → declares:  void createJitDir();
init.c    → defines:   void createJitDir() { ... actual code ... }
main.c    → includes:  #include "init.h" and calls createJitDir()
```

### Header guard:
Every header file is wrapped in a header guard to prevent it being included twice accidentally:
```c
#ifndef INIT_H      // "if INIT_H is not defined yet"
#define INIT_H      // "then define it now"

void createJitDir();

#endif              // end of the guard block
```

The name (`INIT_H`) is just the filename in uppercase with `.` replaced by `_`. So `init.h` → `INIT_H`, `utils.h` → `UTILS_H` etc.

### Difference between `<>` and `""` in includes:
```c
#include <stdio.h>    // <> = system library, installed on your computer
#include "init.h"     // "" = your own file, lives in the same project folder
```

### Why separate `.c` and `.h`?
- Keeps code organized — each file has one responsibility
- Other files only need to know WHAT a function does (header), not HOW it does it (implementation)
- Makes large projects manageable

---

## 5. C Concepts Used <a name="c-concepts"></a>

### argc and argv
Every C program's `main` function can receive command line arguments:

```c
int main(int argc, char* argv[])
```

- `argc` — argument count (total number of arguments including the program name itself)
- `argv` — argument vector, an array of strings
- `argv[0]` — always the program name (`./jit`)
- `argv[1]` — first argument (`init`, `add`, etc.)

Example: running `./jit init` gives:
- `argc = 2`
- `argv[0] = "./jit"`
- `argv[1] = "init"`

Always check `argc` before accessing `argv` to avoid a crash:
```c
if (argc < 2) {
    printf("Usage: jit <command>\n");
    return 1;
}
```

### Pointers
A variable that stores a memory address rather than a value directly:
```c
FILE* file_ptr    // a pointer to a FILE
DIR* dir          // a pointer to a DIR
const char* path  // a pointer to a string (char array)
```

### NULL
A special pointer value meaning "points to nothing." System functions return NULL when they fail. Always check for it:
```c
FILE* f = fopen("file.txt", "w");
if (f == NULL) {
    perror("failed to open file");
    return;
}
```

### const char*
A pointer to a string that cannot be modified. Used for storing fixed path strings:
```c
const char* rootDir = "./.jit";
const char* objectsDir = "./.jit/objects";
```

### errno
A global variable automatically set by system calls when they fail. Tells you WHY something failed:
```c
#include <errno.h>

if (ENOENT == errno) {
    // ENOENT means "No such file or directory"
    // this is how we know the .jit folder doesn't exist yet
}
```

---

## 6. Functions Reference <a name="functions-reference"></a>

### opendir()
Opens a directory and returns a pointer to it. Returns NULL if the directory doesn't exist.
```c
#include <dirent.h>

DIR* dir = opendir("./.jit");
if (dir) {
    // directory exists
    closedir(dir);
}
```
We use this to check if `.jit` already exists before trying to create it.

### closedir()
Closes a directory that was opened with `opendir()`. Always close what you open.
```c
closedir(dir);
```

### mkdir()
Creates a new directory. Returns 0 on success, -1 on failure.
```c
#include <sys/stat.h>

mkdir("./.jit", 0755);
```
The `0755` is the permission setting:
- Owner can read, write, execute
- Everyone else can read and execute
- The leading `0` means it's an octal number

### fopen()
Opens or creates a file. Returns a FILE pointer, or NULL on failure.
```c
#include <stdio.h>

FILE* file_ptr = fopen("./.jit/refs/heads/HEAD", "w");
// "w" mode = write, creates the file if it doesn't exist
```
Modes:
- `"w"` — write mode, creates file, overwrites if it already exists

### fprintf()
Writes formatted text into a file, exactly like printf but targets a file:
```c
fprintf(file_ptr, "%s", str);
// writes the string str into the file pointed to by file_ptr
```

### fclose()
Closes a file after you're done with it. Always close files:
```c
fclose(file_ptr);
```
Not closing files can cause data to not be written (buffering issue) or file descriptor leaks.

### strcmp()
Compares two strings. Returns 0 if they are equal, non-zero if different:
```c
#include <string.h>

if (strcmp(argv[1], "init") == 0) {
    // argv[1] is exactly "init"
}
```
You cannot use `==` to compare strings in C because that compares memory addresses, not the actual characters.

### perror()
Prints an error message based on the current errno value. Useful for system call failures:
```c
perror("Error creating directory");
// prints something like: "Error creating directory: Permission denied"
```

---

## 7. jit init — Deep Dive <a name="jit-init"></a>

### What it does:
Creates the entire `.jit` folder structure needed for a repository to function.

### Logic flow:
```
1. Try to open .jit with opendir()
2. If it opens successfully → already initialised, stop
3. If errno is ENOENT → folder doesn't exist, proceed
4. Create .jit/
5. Create .jit/objects/
6. Create .jit/refs/
7. Create .jit/refs/heads/
8. Create and write .jit/refs/heads/HEAD file
9. Create empty .jit/index file
10. Print success
```

### main.c:
```c
#include <stdio.h>
#include <string.h>
#include "init.h"

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: jit <command>\n");
        return 1;
    }
    if (strcmp(argv[1], "init") == 0) {
        createJitDir();
        printf("\n Jit initialised created \n");
    }
    else {
        perror("SYNTAX : filename jit init");
    }
    return 0;
}
```

### init.h:
```c
#ifndef JIT_INIT_H
#define JIT_INIT_H

void createJitDir();

#endif //JIT_INIT_H
```

### init.c:
```c
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include "init.h"

void createJitDir() {
    DIR* dir = opendir("./.jit");

    if (dir) {
        printf("\n Already Initialised");
        closedir(dir);
    }
    else if (ENOENT == errno) {
        const char* headDir = "./.jit/refs/heads";
        const char* refsDir = "./.jit/refs";
        const char* objectsDir = "./.jit/objects";
        const char* rootDir = "./.jit";

        if (mkdir(rootDir, 0755) == 0) {
            printf("\n Jit root directory created");
        }
        if (mkdir(objectsDir, 0755) == 0) {
            printf("\n objects directory created");
        }
        if (mkdir(refsDir, 0755) == 0) {
            printf("\n refs directory created");
        }
        if (mkdir(headDir, 0755) == 0) {
            printf("\n head directory created");

            FILE* file_ptr = fopen("./.jit/refs/heads/HEAD", "w");
            if (file_ptr == NULL) {
                perror("\n failed to create file");
                return;
            }
            char str[] = "ref: refs/heads/master";
            fprintf(file_ptr, "%s", str);
            fclose(file_ptr);
            printf("\n HEAD file created");
        }

        FILE* file_ptr = fopen("./.jit/index", "w");
        if (file_ptr == NULL) {
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
```

### Output when run:
```bash
$ ./jit init
 Jit root directory created
 objects directory created
 refs directory created
 head directory created
 HEAD file created
 index file created
 Jit initialised created

$ ./jit init
 Already Initialised

$ ./jit
Usage: jit <command>
```

---

## 8. Build & Run <a name="build-and-run"></a>

### CMakeLists.txt
Tells CLion how to build the project:
```cmake
cmake_minimum_required(VERSION 4.1)
project(Jit C)
set(CMAKE_C_STANDARD 23)
add_executable(Jit main.c init.c init.h)
```
Every new `.c` file added to the project must be listed here in `add_executable`.

### Compile manually via terminal:
```bash
gcc main.c init.c -o jit
```
Always include ALL `.c` files. The `-o jit` names the output executable.

### Run:
```bash
./jit init
```

### Why `./` before jit?
Linux doesn't look in the current folder for executables by default. The `./` explicitly means "run the file called jit that is right here in this folder."

---

*Last updated: February 2026*


# Jit — Day 2 Progress
### jit add command + SHA1 deep dive

---

## What we built today: jit add

`jit add filename` stages a file by:
1. Reading its contents
2. SHA1 hashing the contents
3. Storing the contents as a blob in `.jit/objects/`
4. Updating `.jit/index` with the hash and filename

---

## New C Concepts Used

### fread()
Reads binary data from a file into a buffer. Returns the number of bytes actually read:
```c
char buffer[1024];
size_t length = fread(buffer, sizeof(char), 1024, fptr);
// buffer = where to store the data
// sizeof(char) = size of each element (1 byte)
// 1024 = max number of elements to read
// fptr = the file to read from
// length = how many bytes were actually read
```
We use `length` later for hashing and writing — important to use the actual bytes read, not just 1024.

### fwrite()
Writes binary data from a buffer into a file:
```c
fwrite(buffer, 1, length, f);
// buffer = data to write
// 1 = size of each element
// length = number of elements to write
// f = file to write into
```

### snprintf()
Builds a string safely into a buffer. Like printf but into a char array instead of the terminal:
```c
char dirPath[50];
snprintf(dirPath, sizeof(dirPath), "./.jit/objects/%s", dir);
// result: "./.jit/objects/48"
```
The `sizeof(dirPath)` prevents writing more characters than the buffer can hold — avoids buffer overflow.

### strncpy()
Copies a specific number of characters from one string to another:
```c
char dir[3];
strncpy(dir, hex, 2);   // copy first 2 characters of hex into dir
dir[2] = '\0';           // manually add null terminator

char file[39];
strncpy(file, hex + 2, 38);  // copy from position 2 onwards
file[38] = '\0';
```
`hex + 2` is pointer arithmetic — it means "start reading hex from index 2 onwards."

### Variable shadowing — what to avoid
If you name two variables the same thing in nested loops, the inner one "shadows" the outer one causing bugs:
```c
for (int i = 0; arr[i] != NULL; i++) {       // outer i
    for (int i = 0; i < SHA_DIGEST_LENGTH; i++) {  // BAD! shadows outer i
    }
}
```
Fix by using a different name for the inner loop:
```c
for (int j = 0; j < SHA_DIGEST_LENGTH; j++) {  // j instead of i
    sprintf(hex + (j * 2), "%02x", hash[j]);
}
```

### Array of pointers — char *arr[]
An array of strings in C. Used to pass multiple filenames to `addFile`:
```c
void addFile(char *arr[])
// arr[0] = first filename
// arr[1] = second filename
// arr[i] == NULL means end of array (argv is NULL terminated)
```
In main.c we pass `argv + 2` which means "start from argv[2] onwards, skipping program name and 'add'."

---

## SHA1 — Deep Dive

### What is SHA1?
SHA1 (Secure Hash Algorithm 1) is a hashing function. It takes any input (a file, a string, anything) and produces a fixed size output called a digest. No matter how big or small the input is, the output is always the same size.

Key properties:
- Same input always produces the same output
- Even a tiny change in input produces a completely different output
- You cannot reverse it — you can't get the input back from the hash
- This is why Git uses it — if two files have the same hash, they are identical

### What SHA1 actually produces — 20 raw bytes
```c
unsigned char hash[SHA_DIGEST_LENGTH];  // SHA_DIGEST_LENGTH = 20
SHA1(buffer, length, hash);
```

`hash` is an array of 20 `unsigned char` values. Each `unsigned char` is one byte, which can hold a value from 0 to 255.

For example, `hash` might look like this in memory:
```
hash[0]  = 72   (decimal) = 0x48 (hex)
hash[1]  = 132  (decimal) = 0x84 (hex)
hash[2]  = 182  (decimal) = 0xb6 (hex)
...
hash[19] = 185  (decimal) = 0xb9 (hex)
```

These are raw binary values — not human readable text.

### Converting 20 bytes to 40 hex characters

Each byte (0-255) can be represented as exactly 2 hex digits (00 to ff). So 20 bytes × 2 hex digits = 40 characters total.

```c
char hex[41]; // 40 characters + 1 null terminator
for (int j = 0; j < SHA_DIGEST_LENGTH; j++) {
    sprintf(hex + (j * 2), "%02x", hash[j]);
}
hex[40] = '\0';
```

**Breaking this down line by line:**

`char hex[41]` — array of 41 characters. 40 for the hex string, 1 extra for the null terminator `\0` that marks the end of a string in C.

`sprintf(hex + (j * 2), "%02x", hash[j])` — this is the key line. Let's break it apart:

- `sprintf` — like printf but writes into a string instead of the terminal
- `hex + (j * 2)` — pointer arithmetic. On iteration 0: write at position 0. On iteration 1: write at position 2. On iteration 2: write at position 4. Each byte takes 2 characters so we jump by 2 each time.
- `"%02x"` — format specifier meaning: print as hexadecimal (`x`), minimum 2 digits (`2`), pad with zeros if needed (`0`). So the value 8 prints as `08` not just `8`.
- `hash[j]` — the raw byte value to convert

**Example walkthrough for first two bytes:**
```
j=0: hash[0] = 72 = 0x48
     sprintf(hex + 0, "%02x", 72)
     writes "48" at position 0 of hex

j=1: hash[1] = 132 = 0x84
     sprintf(hex + 2, "%02x", 132)
     writes "84" at position 2 of hex

result so far: hex = "4884..."
```

After all 20 iterations, hex contains the full 40 character string like:
```
4884b6c315133b4693380803751de34594693bb9
```

`hex[40] = '\0'` — manually null terminate the string so C knows where it ends.

---

## add.h
```c
#ifndef ADD_H
#define ADD_H

void addFile(char *arr[]);

#endif
```

## add.c — Full Code
```c
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include "add.h"

void addFile(char *arr[]) {
    char buffer[1024];
    for (int i = 0; arr[i] != NULL; i++) {

        // step 1 & 2: open and read file into buffer
        FILE* fptr = fopen(arr[i], "r");
        if (fptr == NULL) {
            printf("file not found: %s\n", arr[i]);
            continue;
        }
        const size_t length = fread(buffer, sizeof(char), 1024, fptr);
        fclose(fptr);

        // step 3: SHA1 hash the contents
        unsigned char hash[SHA_DIGEST_LENGTH];
        SHA1(buffer, length, hash);

        // convert 20 raw bytes to 40 hex characters
        char hex[41];
        for (int j = 0; j < SHA_DIGEST_LENGTH; j++) {
            sprintf(hex + (j * 2), "%02x", hash[j]);
        }
        hex[40] = '\0';

        // step 4: split hash into dir (first 2) and filename (rest 38)
        char dir[3];
        char file[39];
        strncpy(dir, hex, 2);
        dir[2] = '\0';
        strncpy(file, hex + 2, 38);
        file[38] = '\0';

        // step 5 & 6: create blob in .jit/objects/
        char dirPath[50];
        char filePath[100];
        snprintf(dirPath, sizeof(dirPath), "./.jit/objects/%s", dir);
        snprintf(filePath, sizeof(filePath), "./.jit/objects/%s/%s", dir, file);

        mkdir(dirPath, 0755);
        FILE* f = fopen(filePath, "w");
        if (f == NULL) {
            perror("failed to create blob");
            return;
        }
        fwrite(buffer, 1, length, f);
        fclose(f);

        // step 7: update the index
        FILE* index = fopen("./.jit/index", "a");
        if (index == NULL) {
            perror("failed to open index");
            return;
        }
        fprintf(index, "%s %s\n", hex, arr[i]);
        fclose(index);
    }
}
```

## main.c update
```c
if (strcmp(argv[1], "add") == 0) {
    addFile(argv + 2);
}
```

## Compile command
```bash
gcc main.c init.c add.c -lssl -lcrypto -o jit
```
`-lssl -lcrypto` flags are required because SHA1 comes from the OpenSSL library.

## Result
```
$ ./jit add main.c
```
Creates:
- `.jit/objects/48/84b6c315133b4693380803751de34594693bb9` — the blob file containing main.c contents
- `.jit/index` updated with: `4884b6c315133b4693380803751de34594693bb9 main.c`

---

*Last updated: February 2026*