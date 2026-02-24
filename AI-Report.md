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

# Jit — Day 3 Progress
### jit commit + Deep dive into C string/file functions and pointers

---

## Table of Contents
1. [What we built today](#built)
2. [Steps taken to implement jit commit](#steps)
3. [The hashing_and_storing helper function](#helper)
4. [char buffer[] vs char *buffer vs char *buffer[]](#buffers)
5. [String Functions — Complete Reference](#strings)
6. [File Functions — Complete Reference](#files)
7. [The time() function](#time)
8. [Full commit.c code](#code)

---

## 1. What we built today <a name="built"></a>

`jit commit -m "message"` — saves a permanent snapshot of the staged files.

**What it creates:**
- A **tree object** in `.jit/objects/` representing the current state of the index
- A **commit object** in `.jit/objects/` containing tree hash, parent hash, author, timestamp, message
- Updates `.jit/refs/heads/master` with the new commit hash

**Verified working:**
```
tree bc278996f2323daca576c0120a761e438e92c4cc
parent none
author Jeethan
timestamp 1771902789
message my first commit
```

And second commit correctly chains back:
```
tree 0e5330dddd5ab339c3df2c36e70291e1410601dd
parent e040c185d806f946804c7c48816147604e1c320a
author Jeethan
timestamp 1771903044
message my second commit
```

---

## 2. Steps taken to implement jit commit <a name="steps"></a>

### Step 1 — Read the index line by line
Open `.jit/index` and read each line using `fgets()`. Each line looks like:
```
4884b6c315133b4693380803751de34594693bb9 main.c
```

### Step 2 — Build the tree string
For each line read from the index, prepend `blob ` to it and append to a large buffer:
```
blob 4884b6c315133b4693380803751de34594693bb9 main.c
blob 9f3a21bc45... add.c
```
This combined string is the tree content.

### Step 3 — Hash and store the tree object
Pass the tree string to `hashing_and_storing()`:
- SHA1 hash the string
- Convert hash to 40 char hex string
- Split hex into first 2 chars (folder) and remaining 38 chars (filename)
- Create folder in `.jit/objects/`
- Write the tree string as file contents
- Return the hex hash

### Step 4 — Find the parent commit hash
Try to open `.jit/refs/heads/master`:
- If it doesn't exist → first commit, parent = `"none"`
- If it exists → read the hash inside it, that's the parent

### Step 5 — Build the commit string in memory
```c
snprintf(commitContent, sizeof(commitContent),
    "tree %s\nparent %s\nauthor Jeethan\ntimestamp %ld\nmessage %s\n",
    treeHash, parentHash, (long)timestamp, str);
```
Build the entire commit as one string in memory. No need to write to a temporary file.

### Step 6 — Hash and store the commit object
Pass `commitContent` to `hashing_and_storing()` — same function used for the tree. Returns the commit hash.

### Step 7 — Update the branch pointer
- Read `.jit/HEAD` to get `ref: refs/heads/master`
- Extract branch name using `sscanf()`
- Build path `.jit/refs/heads/master`
- Write the new commit hash into that file, overwriting the previous one

---

## 3. The hashing_and_storing helper function <a name="helper"></a>

We created a reusable helper function that both the tree and commit use. It:
1. Takes any string as input
2. SHA1 hashes it
3. Converts to hex
4. Splits into dir/file
5. Creates the object file in `.jit/objects/`
6. Returns the hex hash

**Why `static char hex[41]`?**

Local variables in C are destroyed when a function returns. If you return a pointer to a local variable, that memory is gone and the pointer is dangling — this causes crashes or garbage data.

`static` makes the variable persist in memory even after the function returns, so returning a pointer to it is safe:
```c
static char hex[41];  // lives for entire program duration
return hex;           // safe to return this pointer
```

**Important caveat:** because it's static, there is only ONE copy of it. If you call `hashing_and_storing()` twice without copying the result, the second call overwrites the first. That's why we use `strcpy()` to copy the result immediately:
```c
char treeHash[41];
strcpy(treeHash, hashing_and_storing(treeContent));  // copy before calling again
```

---

## 4. char buffer[] vs char *buffer vs char *buffer[] <a name="buffers"></a>

This is one of the most important and confusing topics in C. Let's break it down clearly.

### char buffer[1024] — fixed size array on the stack

```c
char buffer[1024];
```

- Allocates 1024 bytes on the **stack** (local memory)
- Size is fixed at compile time, cannot change
- Memory is automatically freed when function returns
- `buffer` is the address of the first character
- Use when: you know the maximum size ahead of time

```c
char name[50];
strcpy(name, "Jeethan");  // stores "Jeethan" in the array
printf("%s", name);       // prints "Jeethan"
```

### char *buffer — pointer to a character (or string)

```c
char *buffer;
```

- Just a pointer — stores a memory address
- Points to wherever you tell it to point
- Can point to a string literal, an array, or dynamically allocated memory
- Size is flexible — can point to strings of any length
- Use when: you don't know the size, or you're returning strings from functions, or pointing to existing strings

```c
char *name = "Jeethan";        // points to a string literal (read only)
char *ptr = buffer;            // points to an existing array
static char hex[41];
return hex;                    // returning a pointer to static array
```

**Key difference:**
```c
char buffer[10] = "hello";   // buffer IS the memory, contains "hello"
char *ptr = "hello";         // ptr POINTS TO memory containing "hello"
```

Think of it like:
- `char buffer[]` = a house (the actual storage)
- `char *ptr` = an address on a piece of paper (just tells you where the house is)

### char *buffer[] — array of pointers (array of strings)

```c
char *buffer[];
```

- An array where each element is a pointer to a string
- This is what `argv` is — an array of string pointers
- Each `buffer[i]` points to a different string
- The array ends with a NULL pointer

```c
char *fruits[] = {"apple", "banana", "mango", NULL};
printf("%s", fruits[0]);  // prints "apple"
printf("%s", fruits[1]);  // prints "banana"
```

This is exactly what we use in `addFile(char *arr[])`:
```c
void addFile(char *arr[]) {
    for (int i = 0; arr[i] != NULL; i++) {
        // arr[0] = "main.c"
        // arr[1] = "add.c"
        // arr[2] = NULL  ← stops here
    }
}
```

And in main.c we pass `argv + 2` which means "start the array from index 2 onwards":
```c
addFile(argv + 2);
// argv[0] = "./jit"
// argv[1] = "add"
// argv[2] = "main.c"   ← addFile starts here
// argv[3] = "add.c"
// argv[4] = NULL
```

### Summary table:
```
Type          What it is                    Use when
──────────────────────────────────────────────────────────────
char buf[N]   Fixed array, N bytes          Know max size upfront
char *ptr     Pointer to a string           Flexible, returning strings
char *arr[]   Array of string pointers      Multiple strings, like argv
```

---

## 5. String Functions — Complete Reference <a name="strings"></a>

All require `#include <string.h>` unless noted.

### strlen()
Returns the length of a string, not counting the null terminator:
```c
strlen("hello");      // returns 5
strlen("Jeethan");    // returns 7
```
Use when: you need to know how many characters are in a string.

### strcpy()
Copies one string into another:
```c
char dest[41];
strcpy(dest, "hello");         // dest now contains "hello"
strcpy(dest, sourceString);    // copies sourceString into dest
```
**Warning:** dest must be large enough to hold the source. No bounds checking.
Use when: copying a string into a buffer you own.

### strncpy()
Like strcpy but copies at most N characters. Safer:
```c
char dir[3];
strncpy(dir, hex, 2);   // copy only first 2 characters
dir[2] = '\0';           // always manually null terminate after strncpy!
```
**Important:** strncpy does NOT always add null terminator, so always add it manually.
Use when: copying a specific number of characters.

### strcat()
Appends one string to the end of another:
```c
char result[100] = "hello ";
strcat(result, "world");   // result is now "hello world"
```
**Warning:** destination must have enough space for both strings combined.
Use when: building up a string piece by piece.

### strcmp()
Compares two strings. Returns 0 if equal, non-zero if different:
```c
strcmp("init", "init")    // returns 0 (equal)
strcmp("add", "init")     // returns non-zero (different)

if (strcmp(argv[1], "init") == 0) {
    // strings match
}
```
**Why not use ==?** In C, `==` compares memory addresses, not contents. Two strings can contain the same characters but live at different addresses. `strcmp` compares actual characters.
Use when: comparing any two strings in C.

### sprintf()
Like printf but writes into a string instead of the terminal:
```c
char result[50];
sprintf(result, "%02x", 255);     // result = "ff"
sprintf(result, "hello %s", name); // result = "hello Jeethan"
```
Use when: building a string from formatted values.

### snprintf()
Like sprintf but with a size limit — prevents buffer overflow:
```c
char dirPath[50];
snprintf(dirPath, sizeof(dirPath), "./.jit/objects/%s", dir);
// will never write more than sizeof(dirPath) bytes
```
Use when: building path strings or any formatted string. **Prefer this over sprintf always.**

### sscanf()
Like scanf but reads from a string instead of the terminal:
```c
char line[] = "ref: refs/heads/master";
char branch[100];
sscanf(line, "ref: refs/heads/%s", branch);
// branch now contains "master"
```
Use when: parsing/extracting values from a string with a known format.

### memset()
Fills a block of memory with a specific value:
```c
char buffer[1024];
memset(buffer, 0, sizeof(buffer));  // fills entire buffer with zeros
```
Use when: initializing a buffer to zero/empty before use.

### memcpy()
Copies raw bytes from one memory location to another:
```c
memcpy(dest, src, numBytes);
```
Use when: copying binary data (not just strings). Unlike strcpy, works with any data type and doesn't stop at null terminator.

---

## 6. File Functions — Complete Reference <a name="files"></a>

All require `#include <stdio.h>`.

### fopen()
Opens a file. Returns FILE pointer or NULL on failure:
```c
FILE *f = fopen("path/to/file", mode);
```

**Modes:**
```
"r"   read only — file must exist
"w"   write — creates file, OVERWRITES if exists
"a"   append — creates file, adds to END if exists
"r+"  read and write — file must exist
"w+"  read and write — creates file, overwrites if exists
"rb"  read binary
"wb"  write binary
```

Always check for NULL:
```c
FILE *f = fopen("file.txt", "r");
if (f == NULL) {
    perror("failed to open");
    return;
}
```

### fclose()
Closes a file. Always close what you open:
```c
fclose(f);
```
Not closing causes: data not flushed to disk, file descriptor leak, file corruption.

### fread()
Reads binary data from file into buffer. Returns bytes actually read:
```c
size_t bytesRead = fread(buffer, elementSize, count, file);
// buffer     = where to store data
// elementSize = size of each element (usually 1 for bytes)
// count       = max number of elements to read
// file        = file pointer
```

Example:
```c
char buffer[1024];
size_t len = fread(buffer, sizeof(char), 1024, fptr);
// len = actual number of bytes read
```
Use when: reading binary files or when you need exact byte count.

### fwrite()
Writes binary data from buffer to file:
```c
fwrite(buffer, elementSize, count, file);
```

Example:
```c
fwrite(buffer, 1, length, f);  // write exactly 'length' bytes
```
Use when: writing binary data or raw bytes to files.

### fprintf()
Writes formatted text to a file (like printf but to file):
```c
fprintf(f, "tree %s\n", treeHash);
fprintf(f, "timestamp %ld\n", (long)timestamp);
```
Use when: writing human-readable text with formatting to files.

### fgets()
Reads one line at a time from a file. Stops at newline or EOF:
```c
char line[256];
while (fgets(line, sizeof(line), file)) {
    // line contains one line including the \n at the end
    // loop stops when fgets returns NULL (end of file)
}
```
Use when: reading a file line by line.

### fgetc() / fputc()
Read or write one character at a time:
```c
int c = fgetc(file);     // read one char
fputc('A', file);        // write one char
```
Use when: processing character by character.

### rewind()
Moves the file position back to the beginning:
```c
rewind(file);
```
Use when: you want to read a file again from the start after already reading it.

### fseek() and ftell()
Move to a specific position in a file, and get current position:
```c
fseek(file, 0, SEEK_END);   // move to end of file
long size = ftell(file);     // get current position = file size
fseek(file, 0, SEEK_SET);   // move back to beginning
```
Use when: you need to know file size, or jump to a specific byte position.

### When to use fread vs fgets vs fprintf:
```
Reading binary data          → fread
Reading text line by line    → fgets
Writing formatted text       → fprintf
Writing binary/raw bytes     → fwrite
```

---

## 7. The time() function <a name="time"></a>

Requires `#include <time.h>`.

### What is Unix timestamp?
A Unix timestamp is the number of seconds that have passed since **January 1, 1970 at 00:00:00 UTC** (called the Unix Epoch). It's a single large integer that represents any point in time unambiguously.

For example:
```
0           = Jan 1, 1970 00:00:00
1000000000  = Sep 9, 2001
1771902789  = Feb 24, 2026  ← your first commit timestamp
```

### time()
Returns the current Unix timestamp:
```c
time_t timestamp;
time(&timestamp);           // fills timestamp with current time
// OR
time_t timestamp = time(NULL);  // same thing, shorter form
```

`time_t` is just a typedef for `long` — a large integer holding seconds since epoch.

### Printing the timestamp:
```c
printf("%ld", (long)timestamp);   // prints raw number like 1771902789
```

### Converting to human readable with ctime():
```c
printf("%s", ctime(&timestamp));
// prints: Mon Feb 24 12:34:56 2026
```

### Why we use raw timestamp instead of ctime():
Raw timestamp is better for storage because:
- Smaller (just a number)
- No timezone ambiguity
- Easy to compare (bigger number = later time)
- Easy to calculate differences between times

---

## 8. Full commit.c code <a name="code"></a>

```c
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <time.h>
#include "commit.h"

char* hashing_and_storing(char treeContent[]) {
    size_t len = strlen(treeContent);

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(treeContent, len, hash);

    static char hex[41];
    for (int j = 0; j < SHA_DIGEST_LENGTH; j++) {
        sprintf(hex + (j * 2), "%02x", hash[j]);
    }
    hex[40] = '\0';

    char dir[3];
    char file[39];
    strncpy(dir, hex, 2);
    dir[2] = '\0';
    strncpy(file, hex + 2, 38);
    file[38] = '\0';

    char dirPath[50];
    char filePath[100];
    snprintf(dirPath, sizeof(dirPath), "./.jit/objects/%s", dir);
    snprintf(filePath, sizeof(filePath), "./.jit/objects/%s/%s", dir, file);

    mkdir(dirPath, 0755);
    FILE* f = fopen(filePath, "w");
    if (f == NULL) {
        perror("failed to create object");
    }
    fwrite(treeContent, 1, len, f);
    fclose(f);

    return hex;
}

void commitFile(char str[]) {
    // step 1-3: read index, build tree string, hash and store it
    FILE* fptr = fopen("./.jit/index", "r");
    if (fptr == NULL) {
        perror("Couldn't open index");
        return;
    }
    char treeContent[4096] = "";
    char line[256];
    while (fgets(line, sizeof(line), fptr)) {
        char entry[300];
        snprintf(entry, sizeof(entry), "blob %s", line);
        strcat(treeContent, entry);
    }
    fclose(fptr);

    char treeHash[41];
    strcpy(treeHash, hashing_and_storing(treeContent));

    // step 4: get timestamp and parent hash
    time_t timestamp;
    time(&timestamp);

    FILE* parentHashFile = fopen("./.jit/refs/heads/master", "r");
    char parentHash[41] = "none";
    if (parentHashFile != NULL) {
        fread(parentHash, 1, 41, parentHashFile);
        fclose(parentHashFile);
    }

    // build commit string, hash and store it
    char commitHash[41];
    char commitContent[1024];
    snprintf(commitContent, sizeof(commitContent),
        "tree %s\nparent %s\nauthor Jeethan\ntimestamp %ld\nmessage %s\n",
        treeHash, parentHash, (long)timestamp, str);
    strcpy(commitHash, hashing_and_storing(commitContent));

    // step 5: update branch pointer
    char readLine[256];
    char branch[100];
    FILE* HeadFile = fopen("./.jit/HEAD", "r");
    if (HeadFile == NULL) {
        perror("Failed to open HEAD");
        return;
    }
    fgets(readLine, sizeof(readLine), HeadFile);
    sscanf(readLine, "ref: refs/heads/%s", branch);
    fclose(HeadFile);

    char masterPath[200];
    snprintf(masterPath, sizeof(masterPath), "./.jit/refs/heads/%s", branch);
    FILE* master = fopen(masterPath, "w");
    fprintf(master, "%s", commitHash);
    fclose(master);

    printf("\n Jit committed\n");
}
```

---

## Compile command
```bash
gcc main.c init.c add.c commit.c -lssl -lcrypto -o jit
```

## Commands now working:
```
jit init      ✅
jit add       ✅
jit commit    ✅
```

---

*Last updated: February 2026*