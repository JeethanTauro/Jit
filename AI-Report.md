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

# Jit — Day 4
### Compilers, Warnings, Signed vs Unsigned, and Casting

---

## Table of Contents
1. [What happened today](#today)
2. [GCC vs Clang — differences in practice](#compilers)
3. [The warning explained — pointer sign mismatch](#warning)
4. [signed char vs unsigned char — why it matters](#signedness)
5. [Casting in C](#casting)
6. [Compiler warnings — why they matter](#warnings)
7. [Array Decay — How arrays are treated in C](#decay)
8. [The fix](#fix)

---

## 1. What happened today <a name="today"></a>

Switched from GCC to Clang to compile the project:
```bash
clang main.c init.c commit.c add.c -lssl -lcrypto -o jit
```

Clang immediately caught two warnings that GCC had silently ignored — both in the SHA1 function calls in `add.c` and `commit.c`. This was a great learning opportunity about types, pointers, and how compilers differ in strictness.

---

## 2. GCC vs Clang — differences in practice <a name="compilers"></a>

Both GCC and Clang are C compilers that do the same fundamental job — turn C code into a binary executable. But they differ in how strict and how helpful they are.

**GCC:**
- Older, more widely used on Linux
- Sometimes lets questionable code slide without warning
- Industry standard, battle tested

**Clang:**
- Developed by Apple and the LLVM project
- Stricter and more pedantic about type safety
- Produces cleaner, more readable error and warning messages
- Points to the exact character causing the issue
- Default compiler on macOS

**Same syntax:**
```bash
gcc  main.c init.c add.c commit.c -lssl -lcrypto -o jit
clang main.c init.c add.c commit.c -lssl -lcrypto -o jit
```

**The warning clang caught but gcc didn't:**
```
warning: passing 'char *' to parameter of type 'const unsigned char *'
converts between pointers to integer types where one is of the unique
plain 'char' type and the other is not [-Wpointer-sign]
```

GCC compiled silently. Clang caught a real type mismatch. This is why many developers prefer clang during development — it teaches you better habits.

---

## 3. The warning explained <a name="warning"></a>

The warning came from both `add.c` and `commit.c` when calling SHA1:

```c
// in add.c
SHA1(buffer, length, hash);       // buffer is char[]

// in commit.c
SHA1(treeContent, len, hash);     // treeContent is char[]
```

SHA1's actual function signature in OpenSSL is:
```c
unsigned char *SHA1(const unsigned char *d, size_t n, unsigned char *md);
//                   ^^^^^^^^^^^^^^^^^^^^
//                   expects unsigned char pointer
```

But `buffer` and `treeContent` are both declared as `char[]` — plain signed char. Clang is warning:

**"You're passing a signed char pointer into a function that expects an unsigned char pointer. These are different types."**

The code still compiles and works, but it's technically incorrect and could cause subtle bugs on some systems.

---

## 4. signed char vs unsigned char — why it matters <a name="signedness"></a>

This is one of the most important distinctions in C when dealing with binary data.

### plain char / signed char
```c
char c;
// range: -128 to 127
// can hold negative values
// meant for text characters
```

### unsigned char
```c
unsigned char c;
// range: 0 to 255
// only positive values
// meant for raw bytes and binary data
```

### Why does this matter?

A byte is 8 bits and can represent 256 different values. The question is how you interpret those 256 values:

```
Binary: 11111111

As signed char:   = -1   (two's complement)
As unsigned char: = 255  (just the raw value)
```

When you're dealing with file contents, hashes, and binary data — bytes should never be negative. A byte is just a value from 0 to 255. Using `char` instead of `unsigned char` for binary data can cause:
- Incorrect hash values on some platforms
- Wrong comparisons
- Undefined behavior when values exceed 127

**Rule of thumb:**
```
Text/strings     → char
Binary/raw bytes → unsigned char
Hashing          → unsigned char (always!)
```

### Why does char exist at all then?
Historically C was designed for text processing where characters fit in 0-127 (ASCII range). `char` was sufficient. But modern code deals with binary data, network packets, file bytes etc where values go above 127 — hence `unsigned char` became important.

---

## 5. Casting in C <a name="casting"></a>

A cast is a way of telling the compiler "I know what I'm doing, treat this variable as a different type."

### Syntax:
```c
(target_type) variable
```

### Examples:
```c
// cast int to float
int x = 5;
float f = (float)x;        // f = 5.0

// cast char* to unsigned char*
char buffer[1024];
SHA1((unsigned char*)buffer, length, hash);  // tell compiler to treat as unsigned

// cast time_t to long for printing
time_t timestamp;
printf("%ld", (long)timestamp);

// cast pointer to different type
void* ptr = malloc(100);
char* buf = (char*)ptr;    // common pattern with malloc
```

### When to use casting:
- When passing data to a function that expects a slightly different but compatible type
- When printing values that need a specific format specifier
- When working with `void*` pointers from malloc
- When you genuinely need to reinterpret data as a different type

### When NOT to cast:
- Don't cast to hide real type errors — casting silences warnings but doesn't fix bugs
- Don't cast between completely unrelated types without understanding the implications

### The cast we used:
```c
SHA1((unsigned char*)buffer, length, hash);
```
This tells the compiler: "treat `buffer` as `unsigned char*` for this function call." The data doesn't change, just how the compiler interprets the pointer type.

---

## 6. Compiler warnings — why they matter <a name="warnings"></a>

A warning means: **"this compiled successfully but something looks suspicious."**

Never ignore warnings. Every warning is the compiler telling you something could go wrong. Today's warning was a perfect example — technically the code worked but it was using the wrong type.

### Common warning flags:
```bash
-Wall        # enables all common warnings
-Wextra      # enables extra warnings beyond -Wall
-Werror      # treats ALL warnings as errors (won't compile if any warning exists)
-Wpedantic   # strict ISO C compliance warnings
```

You can compile with:
```bash
clang main.c init.c add.c commit.c -lssl -lcrypto -Wall -Wextra -o jit
```

This gives you maximum warnings and helps catch bugs early.

### Warning vs Error:
```
Error   → code cannot compile at all, must fix
Warning → code compiled but something is suspicious, should fix
```

Clang is stricter than GCC about warnings, which makes it a better learning tool. When clang warns you, take it seriously.

---

## 7. Array Decay — How arrays are treated in C <a name="decay"></a>

This is one of the most important and surprising things about C. Arrays and pointers are deeply connected.

### What is array decay?

When you pass an array to a function, C automatically converts it into a pointer to its first element. This is called **array decay**. The array "decays" into a pointer.

```c
char buffer[1024];

// when you write this:
SHA1(buffer, length, hash);

// C automatically treats it as:
SHA1(&buffer[0], length, hash);  // pointer to first element
```

The array name `buffer` by itself, when used in an expression, becomes the address of its first element.

### Visual explanation:

```
char buffer[1024]:

memory: [h][e][l][l][o][\0][...1018 more bytes...]
         ↑
         buffer (address of first element)
         &buffer[0]
         (char*)buffer
         all three mean the same thing!
```

### The decay chain in our code:

```
char buffer[1024]       ← declared as array
      ↓ passed to function
char*                   ← decays to pointer automatically
      ↓ clang warns because SHA1 expects
unsigned char*          ← signedness mismatch!
```

That's the exact chain that caused our warning. The array decayed to `char*` and THAT is what's incompatible with `unsigned char*`.

### What is lost during decay?

When an array decays to a pointer, the size information is lost:

```c
char buffer[1024];
printf("%zu", sizeof(buffer));  // prints 1024 — knows the size

void someFunction(char* buf) {
    printf("%zu", sizeof(buf));  // prints 8 — just the pointer size, size is LOST!
}
```

This is why C functions that take arrays also take a separate `size` or `length` parameter. The function has no way of knowing how big the array is once it decays to a pointer. This is exactly why SHA1 takes `size_t n` as its second argument — it needs to know how many bytes to hash.

### All these are equivalent when passed to a function:

```c
char buffer[1024] = "hello";

someFunc(buffer);          // array decays to pointer
someFunc(&buffer[0]);      // explicit address of first element
someFunc((char*)buffer);   // explicit cast to pointer
```

All three pass the same thing — the address of the first byte of buffer.

### Arrays vs Pointers — what's actually different:

```c
char buffer[1024];   // buffer IS the memory, 1024 bytes allocated on stack
char *ptr;           // ptr is just a variable holding an address, 8 bytes

sizeof(buffer) = 1024   // knows full size
sizeof(ptr)    = 8      // just the pointer size

buffer = someOtherArray;  // ILLEGAL — can't reassign an array
ptr = someOtherArray;     // fine — can reassign a pointer anytime
```

### Why C works this way:

This design comes from C's philosophy of being close to the hardware. Arrays in memory are just contiguous bytes. A pointer is just an address. Passing an entire array by value would mean copying potentially megabytes of data every function call — expensive and wasteful. So C passes just the address (8 bytes) instead. Efficient but requires you to track the size separately.

### Practical rules:

```
Declaring storage      → use char buffer[N]
Passing to functions   → array decays to pointer automatically
Inside functions       → treat as pointer, size is lost
Binary data/hashing    → use unsigned char, not char
Always pass size too   → because decay loses size info
```

---

## 8. The fix <a name="fix"></a>

Cast the buffer to `unsigned char*` when calling SHA1:

**In add.c:**
```c
// before
SHA1(buffer, length, hash);

// after
SHA1((unsigned char*)buffer, length, hash);
```

**In commit.c:**
```c
// before
SHA1(treeContent, len, hash);

// after
SHA1((unsigned char*)treeContent, len, hash);
```

After fixing, recompile with clang:
```bash
clang main.c init.c add.c commit.c -lssl -lcrypto -o jit
```

No warnings should appear now. Clean compilation = correct types throughout.

---

## Compile commands reference:
```bash
# basic
gcc main.c init.c add.c commit.c -lssl -lcrypto -o jit

# with clang
clang main.c init.c add.c commit.c -lssl -lcrypto -o jit

# with all warnings enabled (recommended)
clang main.c init.c add.c commit.c -lssl -lcrypto -Wall -Wextra -o jit
```

---

## Status of commands:
```
jit init      ✅
jit add       ✅
jit commit    ✅
jit status    → in progress
```

---

*Last updated: February 2026*

# Jit — Day 5 Progress
### jit status — Comparison 1 complete + bug hunting

---

## Table of Contents
1. [What we built today](#built)
2. [jit status — Comparison 1 explained](#comparison1)
3. [Structs in C — deep dive](#structs)
4. [The buffer bug — how we found and fixed it](#bug)
5. [strcspn — stripping newlines](#strcspn)
6. [The duplicate index bug in add.c](#duplicate)
7. [Full status.c code so far](#code)

---

## 1. What we built today <a name="built"></a>

Completed **Comparison 1** of `jit status` — detecting files that have been **modified but not staged**.

Also fixed a critical bug in `add.c` where adding the same file twice would create duplicate entries in the index.

**Verified working:**
```bash
$ ./jit add main.c
$ # modify main.c
$ ./jit status
 Modified unstaged:
     main.c

$ ./jit add main.c   # re-stage the file
$ ./jit status
                     # clean, nothing shown
```

---

## 2. jit status — Comparison 1 explained <a name="comparison1"></a>

### What comparison 1 does:
Compares the **index** (what was staged) against the **working directory** (what's on disk right now).

### The logic:
```
for each entry in index:
    read the file from disk
    hash it
    compare with stored hash in index

    if file doesn't exist on disk → deleted not staged
    if hashes differ              → modified not staged
    if hashes match               → clean, nothing to report
```

### Why this works:
When you do `jit add file.c`, the hash of the file at that moment gets stored in the index. If you then modify the file, its contents change, so its hash changes. When status reads the file from disk and hashes it again, it gets a different hash — mismatch detected!

### Step by step in code:
1. Open `.jit/index` and read line by line with `fgets()`
2. Extract hash from first 40 characters of each line using `strncpy()`
3. Extract filename from position 41 onwards using `strcpy(line + 41)`
4. Strip the newline from filename using `strcspn()`
5. Store both in a struct
6. Open the actual file from disk using `fopen(filename, "r")`
7. If NULL → file deleted, print message and continue
8. Read file contents with `fread()` into a buffer
9. Hash the contents with `SHA1()` via `calc_hashing()`
10. Compare the two hashes with `strcmp()`
11. If different → print "Modified unstaged"

---

## 3. Structs in C — deep dive <a name="structs"></a>

### What is a struct?
A struct is a way to group related data together under one name. Like a custom data type you define yourself.

```c
struct file_and_hash {
    char filename[1024];
    char hash[41];
};
```

This creates a type called `file_and_hash` that holds both a filename and its hash together as one unit.

### Declaring and using a struct:
```c
// declare a single struct variable
struct file_and_hash f;

// access fields with dot notation
strcpy(f.filename, "main.c");
strcpy(f.hash, "4884b6c315...");

// read fields
printf("%s", f.filename);
printf("%s", f.hash);
```

### Array of structs:
You can have an array where each element is a struct — perfect for storing multiple index entries:

```c
struct file_and_hash entries[100];
int count = 0;

// add an entry
strcpy(entries[count].filename, "main.c");
strcpy(entries[count].hash, "4884b6...");
count++;

// access entries
for (int k = 0; k < count; k++) {
    printf("%s %s\n", entries[k].hash, entries[k].filename);
}
```

### Why we used structs here:
We needed to store multiple index entries in memory simultaneously to:
1. Check for duplicates in `add.c`
2. Compare against disk files in `status.c`

Without structs we'd need two separate arrays — one for filenames and one for hashes — and keeping them in sync would be error prone. Structs keep related data together cleanly.

### Struct memory layout:
```
entries[0]: [filename: "main.c\0..."][hash: "4884b6...\0"]
entries[1]: [filename: "add.c\0..." ][hash: "9f3a21...\0"]
entries[2]: [filename: "commit.c\0."][hash: "7f3a9b...\0"]
```

Each element in the array contains its own copy of both fields. Total memory per entry = 1024 + 41 = 1065 bytes.

---

## 4. The buffer bug — how we found and fixed it <a name="bug"></a>

### The symptom:
After modifying `main.c` and running `jit status`, it reported nothing — as if the file hadn't changed at all. Even after appending a line directly with:
```bash
echo "// test modification" >> main.c
```
Status still showed no difference.

### The debug process:
Added print statements to compare the two hashes directly:
```c
printf("index hash: %s\n", f.hash);
printf("disk hash:  %s\n", hash);
```

Output:
```
index hash: 25226b34cb0753703182bf56bcd7468c196c40e3
disk hash:  25226b34cb0753703182bf56bcd7468c196c40e3
```

Same hash even though the file was modified. That meant the modification wasn't being read at all.

### Root cause:
```bash
wc -c main.c
1177 main.c
```

`main.c` was **1177 bytes** but the buffer was only **1024 bytes**. `fread` was reading only the first 1024 bytes — the modification appended at the end (bytes 1025-1177) was never read, never hashed.

Both `add.c` and `status.c` were hashing only the first 1024 bytes, so they always produced the same hash regardless of what changed at the end of the file!

### The fix:
Increase the buffer size in BOTH `add.c` and `status.c` to handle larger files:
```c
// before
char buffer[1024];
size_t length = fread(buffer, sizeof(char), 1024, fptr);

// after
char buffer[65536];   // 64KB — handles most source code files
size_t length = fread(buffer, sizeof(char), 65536, fptr);
```

**Critical rule learned:** The buffer size and the fread count must ALWAYS match:
```c
char buffer[65536];
fread(buffer, sizeof(char), 65536, fptr);  // read up to 65536 bytes
//    ^^^^^^                ^^^^^^
//    must match            must match
```

If they don't match, you either read more than the buffer can hold (crash/corruption) or read less than the file contains (wrong hash).

### Why 65536?
64KB (65536 bytes) is a reasonable size for most source code files. Real Git handles this properly using dynamic memory allocation with `malloc()` to read files of any size. For our purposes, 64KB is sufficient.

---

## 5. strcspn — stripping newlines <a name="strcspn"></a>

`fgets()` keeps the `\n` newline character at the end of each line it reads. If you don't strip it, your filename becomes `"main.c\n"` instead of `"main.c"` — and `fopen("main.c\n", "r")` will fail to find the file!

### The fix:
```c
filename[strcspn(filename, "\n")] = '\0';
```

### How strcspn works:
`strcspn(str, chars)` returns the index of the first character in `str` that matches any character in `chars`. So:

```c
char filename[] = "main.c\n";
strcspn(filename, "\n")  // returns 6 (index of \n)
filename[6] = '\0';       // replaces \n with null terminator
// filename is now "main.c"
```

It's the cleanest one-liner for stripping newlines in C. Always use it after `fgets()` when you need the string without the newline.

---

## 6. The duplicate index bug in add.c <a name="duplicate"></a>

### The bug:
Running `jit add main.c` twice would create two entries in the index:
```
25226b34... main.c
25226b34... main.c
```

This caused status to process the same file twice and would cause all sorts of confusion later.

### Root cause:
The original `add.c` always appended to the index with `"a"` mode regardless of whether the file was already there:
```c
FILE* index = fopen("./.jit/index", "a");
fprintf(index, "%s %s\n", hex, arr[i]);
```

### The fix — read, check, update, rewrite:
Instead of blindly appending, we now:
1. Read ALL existing index entries into an array of structs
2. Search through them for the current filename
3. If found → update the hash in that entry
4. If not found → add a new entry at the end
5. Rewrite the ENTIRE index from scratch with `"w"` mode

```c
// read all existing entries
struct file_and_hash entries[100];
int count = 0;
FILE* indexRead = fopen("./.jit/index", "r");
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

// check if file already exists in index
int found = 0;
for (int k = 0; k < count; k++) {
    if (strcmp(entries[k].filename, arr[i]) == 0) {
        strcpy(entries[k].hash, hex);  // update hash
        found = 1;
        break;
    }
}
if (!found) {
    strcpy(entries[count].hash, hex);      // add new entry
    strcpy(entries[count].filename, arr[i]);
    count++;
}

// rewrite entire index
FILE* indexWrite = fopen("./.jit/index", "w");
for (int k = 0; k < count; k++) {
    fprintf(indexWrite, "%s %s\n", entries[k].hash, entries[k].filename);
}
fclose(indexWrite);
```

---

## 7. Full status.c code so far <a name="code"></a>

```c
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include "status.h"

char* calc_hashing(char buffer[], size_t len);

struct file_and_hash {
    char filename[1024];
    char hash[41];
};

void status() {

    // Comparison 1: Working directory vs Index
    struct file_and_hash f;
    FILE* fptr = fopen("./.jit/index", "r");
    if (fptr == NULL) {
        perror("error opening index");
        return;
    }
    char line[1024];
    char filename[1024];
    char hash[41];

    while (fgets(line, 1024, fptr) != NULL) {
        strncpy(hash, line, 40);
        hash[40] = '\0';

        strcpy(filename, line + 41);
        filename[strcspn(filename, "\n")] = '\0';

        strcpy(f.filename, filename);
        strcpy(f.hash, hash);

        char buffer[65536];
        FILE* file = fopen(filename, "r");
        if (file == NULL) {
            printf("File %s deleted but not staged\n", f.filename);
            continue;
        }
        size_t len = fread(buffer, sizeof(char), 65536, file);
        fclose(file);

        strcpy(hash, calc_hashing(buffer, len));
        if (strcmp(f.hash, hash) != 0) {
            printf("\nModified unstaged:\n\t%s\n", f.filename);
        }
    }
    fclose(fptr);

    // Comparison 2: Index vs Last Commit Tree — coming next
    // Comparison 3: Untracked files — coming next
}

char* calc_hashing(char buffer[], size_t len) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)buffer, len, hash);

    static char hex[41];
    for (int j = 0; j < SHA_DIGEST_LENGTH; j++) {
        sprintf(hex + (j * 2), "%02x", hash[j]);
    }
    hex[40] = '\0';

    return hex;
}
```

---

## Status of commands:
```
jit init      ✅
jit add       ✅ (duplicate bug fixed)
jit commit    ✅
jit status    🔄 comparison 1 done, comparisons 2 and 3 remaining
```

---

*Last updated: February 2026*

# Jit — Day 6 Progress
### jit status — All Three Comparisons Complete

---

## Table of Contents
1. [What we completed today](#built6)
2. [Refactoring into three functions](#refactor)
3. [Comparison 2 — untracked() in detail](#comparison2)
4. [Comparison 3 — staged_yet_to_commit() in detail](#comparison3)
5. [The object chain — navigating commit → tree → blobs](#objectchain)
6. [memmove vs strcpy — overlapping memory](#memmove)
7. [malloc and free — dynamic memory](#malloc)
8. [opendir and readdir — listing directory contents](#opendir)
9. [The found flag pattern](#foundflag)
10. [Full final status.c code](#finalcode)

---

## 1. What we completed today <a name="built6"></a>

Completed all three comparisons of `jit status`. The command now behaves like real Git:

```bash
$ ./jit status

Changes not staged for commit:
    modified: main.c

Untracked:
    init.c
    add.c
    commit.c

Changes to be committed:
    new file: status.c
    modified: main.c
```

Also refactored the entire status.c into three clean separate functions.

**Commands now complete:**
```
jit init      ✅
jit add       ✅
jit commit    ✅
jit status    ✅  — completed today
```

---

## 2. Refactoring into three functions <a name="refactor"></a>

The original implementation had all three comparisons jammed into one huge `status()` function. We split it into three independent functions:

```c
void modified();             // comparison 1 — working dir vs index
void untracked();            // comparison 2 — disk vs index
void staged_yet_to_commit(); // comparison 3 — index vs last commit tree

void status() {
    modified();
    untracked();
    staged_yet_to_commit();
}
```

Benefits:
- Each function does exactly one thing
- Easy to debug one comparison without touching others
- `status()` itself becomes just three lines — a clean high level overview
- Easy to add or modify one comparison independently

---

## 3. Comparison 2 — untracked() in detail <a name="comparison2"></a>

**Question: what files exist on disk that jit does not know about at all?**

### Algorithm:

**Step 1 — Read all filenames from index into memory:**
```c
char *filenames_in_index[1000];
int index_count = 0;

FILE* fptr = fopen("./.jit/index", "r");
while (fgets(line, 1024, fptr) != NULL) {
    strcpy(filename, line + 41);               // extract filename from position 41
    filename[strcspn(filename, "\n")] = '\0';  // strip newline

    filenames_in_index[index_count] = malloc(strlen(filename) + 1); // fresh memory for each
    strcpy(filenames_in_index[index_count], filename);
    index_count++;
}
fclose(fptr);
```

**Step 2 — List all files on disk using opendir/readdir:**
```c
char *filenames_in_dir[1000];
int dir_count = 0;

DIR* d = opendir(".");
struct dirent* dir;
while ((dir = readdir(d)) != NULL) {
    if (dir->d_name[0] == '.') continue;   // skip hidden files and .jit
    if (dir->d_type == DT_DIR) continue;   // skip directories
    filenames_in_dir[dir_count] = malloc(strlen(dir->d_name) + 1);
    strcpy(filenames_in_dir[dir_count], dir->d_name);
    dir_count++;
}
closedir(d);
```

**Step 3 — Compare using found flag:**
```c
for (int i = 0; i < dir_count; i++) {
    if (strcmp(filenames_in_dir[i], "jit") == 0) continue; // skip executable

    int found = 0;
    for (int j = 0; j < index_count; j++) {
        if (strcmp(filenames_in_dir[i], filenames_in_index[j]) == 0) {
            found = 1;
            break;
        }
    }
    if (!found) {
        // file exists on disk but not in index → untracked
        printf("\t  %s\n", filenames_in_dir[i]);
    }
}
```

**Step 4 — Free all malloc'd memory:**
```c
for (int i = 0; i < index_count; i++) free(filenames_in_index[i]);
for (int i = 0; i < dir_count; i++) free(filenames_in_dir[i]);
```

---

## 4. Comparison 3 — staged_yet_to_commit() in detail <a name="comparison3"></a>

**Question: what have you staged that has not been committed yet?**

### Algorithm:

**Step 1 — Read all index entries into array of structs:**
```c
struct file_and_hash index[30];
int k = 0;

while (fgets(line, 1024, fptr) != NULL) {
    strncpy(index[k].hash, line, 40);
    index[k].hash[40] = '\0';
    strcpy(index[k].filename, line + 41);
    index[k].filename[strcspn(index[k].filename, "\n")] = '\0';
    k++;
}
```

**Step 2 — Check if any commits exist:**
```c
FILE* master = fopen("./.jit/refs/heads/master", "r");
if (master == NULL) {
    // no commits yet — every file in index is new and staged
    if (k > 0) {
        printf("\nChanges to be committed:\n");
        for (int i = 0; i < k; i++) {
            printf("\t  new file: %s\n", index[i].filename);
        }
    }
    return;
}
```

**Step 3 — Read commit hash from master:**
```c
char latest_commit[1024];
fread(latest_commit, 1, 40, master);
latest_commit[40] = '\0';
fclose(master);
```

**Step 4 — Build path to commit object and read tree hash:**
```c
char folder[3], file[40];
strncpy(folder, latest_commit, 2);  folder[2] = '\0';
strncpy(file, latest_commit + 2, 38);  file[38] = '\0';

char tree_hash_path[1024];
snprintf(tree_hash_path, sizeof(tree_hash_path), "./.jit/objects/%s/%s", folder, file);

char tree_hash[1024];
FILE* commit_file = fopen(tree_hash_path, "r");
fgets(tree_hash, sizeof(tree_hash), commit_file);  // reads "tree xxxxxxxxx"
fclose(commit_file);

tree_hash[strcspn(tree_hash, "\n")] = '\0';
memmove(tree_hash, tree_hash + 5, strlen(tree_hash + 5) + 1); // remove "tree " prefix
tree_hash[strcspn(tree_hash, " \n")] = '\0';
```

**Step 5 — Build path to tree object and read all blob entries:**
```c
strncpy(folder, tree_hash, 2);  folder[2] = '\0';
strncpy(file, tree_hash + 2, 38);  file[38] = '\0';

char commit_path[1024];
snprintf(commit_path, sizeof(commit_path), "./.jit/objects/%s/%s", folder, file);

struct file_and_hash tree[30];
int tree_count = 0;
FILE* tree_file = fopen(commit_path, "r");
while (fgets(line, 1024, tree_file) != NULL) {
    // line format: "blob xxxxxxxxx filename"
    // blob_ = 5 chars, hash = 40 chars, _ = 1 char, filename at 46
    strncpy(tree[tree_count].hash, line + 5, 40);
    tree[tree_count].hash[40] = '\0';
    strcpy(tree[tree_count].filename, line + 46);
    tree[tree_count].filename[strcspn(tree[tree_count].filename, "\n")] = '\0';
    tree_count++;
}
fclose(tree_file);
```

**Step 6 — Compare index against tree:**
```c
for (int i = 0; i < k; i++) {
    int found = 0;
    for (int j = 0; j < tree_count; j++) {
        if (strcmp(index[i].filename, tree[j].filename) == 0) {
            found = 1;
            if (strcmp(index[i].hash, tree[j].hash) != 0) {
                // same file but different hash → modified and staged
                printf("\nChanges to be committed:\n\t  modified: %s\n", index[i].filename);
            }
            break;
        }
    }
    if (!found) {
        // file in index but not in last commit → brand new file staged
        printf("\nChanges to be committed:\n\t  new file: %s\n", index[i].filename);
    }
}
```

---

## 5. The object chain — navigating commit → tree → blobs <a name="objectchain"></a>

The most complex part of comparison 3 is navigating two levels of objects:

```
.jit/refs/heads/master
    contains: e04f5ea4f27d...         ← latest commit hash

.jit/objects/e0/4f5ea4f27d...         ← commit object
    tree 4eab9de00dc8...              ← first line = tree hash
    parent none
    author Jeethan
    timestamp 1772168490
    message first commit

.jit/objects/4e/ab9de00dc8...         ← tree object
    blob 80722e96... main.c           ← one line per staged file
    blob 9f3a21bc... status.c
```

Two hops to get to the actual file hashes:
1. Read master → get commit hash → open commit object → read tree hash
2. Open tree object → read all blob entries → now you have hashes and filenames to compare

---

## 6. memmove vs strcpy — overlapping memory <a name="memmove"></a>

When extracting the tree hash from `"tree xxxxxxxxx"`, we need to shift the string left by 5 characters. The wrong way:

```c
strcpy(tree_hash, tree_hash + 5);  // WRONG — source and dest overlap!
```

`strcpy` is not designed for overlapping memory. When source and destination are in the same buffer, it can corrupt the data silently — no error, no warning, just wrong output.

The right way:
```c
memmove(tree_hash, tree_hash + 5, strlen(tree_hash + 5) + 1);  // CORRECT
```

`memmove` handles overlapping memory correctly by copying through a temporary buffer internally. Always use `memmove` when source and destination might overlap.

```
before: tree_hash = "tree 4eab9de00dc8..."
                          ↑
                          tree_hash + 5

after memmove:  tree_hash = "4eab9de00dc8..."
```

---

## 7. malloc and free — dynamic memory <a name="malloc"></a>

For the untracked comparison we needed to store many filenames in separate memory locations. The naive approach fails:

```c
// WRONG — all entries point to the same buffer:
char filename[1024];
filenames_in_index[i] = filename;  // every entry points to same address!
// after loop: all entries contain the LAST filename only
```

The correct approach uses `malloc` to allocate fresh memory for each string:

```c
// CORRECT — each entry gets its own unique memory:
filenames_in_index[i] = malloc(strlen(filename) + 1);  // allocate exact size needed
strcpy(filenames_in_index[i], filename);                // copy into fresh memory
```

`malloc(n)` allocates n bytes of memory on the heap and returns a pointer to it. Each call returns a different address.

**Every malloc must be paired with a free:**
```c
// after you're done with the data:
for (int i = 0; i < index_count; i++) free(filenames_in_index[i]);
```

Not freeing is called a memory leak. The memory stays allocated even after you're done with it.

---

## 8. opendir and readdir — listing directory contents <a name="opendir"></a>

From `#include <dirent.h>`. Used to list files in a directory, like `ls` on the terminal.

```c
DIR* d = opendir(".");          // open current directory
struct dirent* dir;             // pointer to directory entry

while ((dir = readdir(d)) != NULL) {
    dir->d_name    // filename as string
    dir->d_type    // type: DT_REG = regular file, DT_DIR = directory
}
closedir(d);
```

**Filters we applied:**
```c
if (dir->d_name[0] == '.') continue;  // skip .jit, ., .. and hidden files
if (dir->d_type == DT_DIR) continue;  // skip directories like cmake-build-debug
if (strcmp(dir->d_name, "jit") == 0) continue;  // skip our own executable
```

---

## 9. The found flag pattern <a name="foundflag"></a>

Used consistently across all three comparisons. The idea: loop through a list looking for a match, set a flag if found, check the flag after the loop.

```c
int found = 0;
for (int j = 0; j < count; j++) {
    if (strcmp(a[i], b[j]) == 0) {
        found = 1;
        break;  // stop searching once found
    }
}
if (!found) {
    // item was not in the list
}
```

This pattern appears in:
- `add.c` — checking if file already exists in index before updating
- `untracked()` — checking if disk file exists in index
- `staged_yet_to_commit()` — checking if index file exists in last commit tree

---

## 10. Full final status.c code <a name="finalcode"></a>

```c
#include 
#include 
#include <openssl/sha.h>
#include 
#include 
#include "status.h"

char* calc_hashing(char buffer[], size_t len);
void modified();
void untracked();
void staged_yet_to_commit();

struct file_and_hash {
    char filename[1024];
    char hash[41];
};

void status() {
    modified();
    untracked();
    staged_yet_to_commit();
}

void modified() {
    struct file_and_hash f;
    FILE* fptr = fopen("./.jit/index", "r");
    if (fptr == NULL) { perror("error opening index"); return; }
    char line[1024], filename[1024], hash[41];

    while (fgets(line, 1024, fptr) != NULL) {
        strncpy(hash, line, 40);
        hash[40] = '\0';
        strcpy(filename, line + 41);
        filename[strcspn(filename, "\n")] = '\0';
        strcpy(f.filename, filename);
        strcpy(f.hash, hash);

        char buffer[65536];
        FILE* file = fopen(filename, "r");
        if (file == NULL) {
            printf("File %s deleted but not staged\n", f.filename);
            continue;
        }
        size_t len = fread(buffer, sizeof(char), 65536, file);
        fclose(file);
        strcpy(hash, calc_hashing(buffer, len));
        if (strcmp(f.hash, hash) != 0) {
            printf("\nChanges not staged for commit:\n\t  modified: %s\n", f.filename);
        }
    }
    fclose(fptr);
}

void untracked() {
    FILE* fptr = fopen("./.jit/index", "r");
    char line[1024], filename[1024];
    printf("\nUntracked files:\n");
    int index_count = 0, dir_count = 0;
    char *filenames_in_index[1000], *filenames_in_dir[1000];

    while (fgets(line, 1024, fptr) != NULL) {
        strcpy(filename, line + 41);
        filename[strcspn(filename, "\n")] = '\0';
        filenames_in_index[index_count] = malloc(strlen(filename) + 1);
        strcpy(filenames_in_index[index_count], filename);
        index_count++;
    }
    fclose(fptr);

    DIR* d = opendir(".");
    struct dirent* dir;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] == '.') continue;
            if (dir->d_type == DT_DIR) continue;
            filenames_in_dir[dir_count] = malloc(strlen(dir->d_name) + 1);
            strcpy(filenames_in_dir[dir_count], dir->d_name);
            dir_count++;
        }
        closedir(d);
    }

    char *untracked[1000];
    int untracked_count = 0;
    for (int i = 0; i < dir_count; i++) {
        if (strcmp(filenames_in_dir[i], "jit") == 0) continue;
        int found = 0;
        for (int j = 0; j < index_count; j++) {
            if (strcmp(filenames_in_dir[i], filenames_in_index[j]) == 0) {
                found = 1; break;
            }
        }
        if (!found) {
            untracked[untracked_count] = malloc(strlen(filenames_in_dir[i]) + 1);
            strcpy(untracked[untracked_count], filenames_in_dir[i]);
            untracked_count++;
        }
    }
    for (int i = 0; i < untracked_count; i++) {
        printf("\t  %s\n", untracked[i]);
        free(untracked[i]);
    }
    for (int i = 0; i < index_count; i++) free(filenames_in_index[i]);
    for (int i = 0; i < dir_count; i++) free(filenames_in_dir[i]);
}

void staged_yet_to_commit() {
    FILE* fptr = fopen("./.jit/index", "r");
    char line[1024];
    struct file_and_hash index[30];
    int k = 0;
    while (fgets(line, 1024, fptr) != NULL) {
        strncpy(index[k].hash, line, 40);
        index[k].hash[40] = '\0';
        strcpy(index[k].filename, line + 41);
        index[k].filename[strcspn(index[k].filename, "\n")] = '\0';
        k++;
    }
    fclose(fptr);

    FILE* master = fopen("./.jit/refs/heads/master", "r");
    if (master == NULL) {
        if (k > 0) {
            printf("\nChanges to be committed:\n");
            for (int i = 0; i < k; i++)
                printf("\t  new file: %s\n", index[i].filename);
        }
        return;
    }
    char latest_commit[1024];
    fread(latest_commit, 1, 40, master);
    fclose(master);
    latest_commit[40] = '\0';

    char folder[3], file[40], tree_hash_path[1024];
    strncpy(folder, latest_commit, 2); folder[2] = '\0';
    strncpy(file, latest_commit + 2, 38); file[38] = '\0';
    snprintf(tree_hash_path, sizeof(tree_hash_path), "./.jit/objects/%s/%s", folder, file);

    char tree_hash[1024], commit_path[1024];
    FILE* commit_file = fopen(tree_hash_path, "r");
    fgets(tree_hash, sizeof(tree_hash), commit_file);
    fclose(commit_file);
    tree_hash[strcspn(tree_hash, "\n")] = '\0';
    memmove(tree_hash, tree_hash + 5, strlen(tree_hash + 5) + 1);
    tree_hash[strcspn(tree_hash, " \n")] = '\0';

    strncpy(folder, tree_hash, 2); folder[2] = '\0';
    strncpy(file, tree_hash + 2, 38); file[38] = '\0';
    snprintf(commit_path, sizeof(commit_path), "./.jit/objects/%s/%s", folder, file);

    struct file_and_hash tree[30];
    int tree_count = 0;
    FILE* tree_file = fopen(commit_path, "r");
    while (fgets(line, 1024, tree_file) != NULL) {
        strncpy(tree[tree_count].hash, line + 5, 40);
        tree[tree_count].hash[40] = '\0';
        strcpy(tree[tree_count].filename, line + 46);
        tree[tree_count].filename[strcspn(tree[tree_count].filename, "\n")] = '\0';
        tree_count++;
    }
    fclose(tree_file);

    for (int i = 0; i < k; i++) {
        int found = 0;
        for (int j = 0; j < tree_count; j++) {
            if (strcmp(index[i].filename, tree[j].filename) == 0) {
                found = 1;
                if (strcmp(index[i].hash, tree[j].hash) != 0)
                    printf("\nChanges to be committed:\n\t  modified: %s\n", index[i].filename);
                break;
            }
        }
        if (!found)
            printf("\nChanges to be committed:\n\t  new file: %s\n", index[i].filename);
    }
}

char* calc_hashing(char buffer[], size_t len) {
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)buffer, len, hash);
    static char hex[41];
    for (int j = 0; j < SHA_DIGEST_LENGTH; j++)
        sprintf(hex + (j * 2), "%02x", hash[j]);
    hex[40] = '\0';
    return hex;
}
```

---

## Status of commands:
```
jit init      ✅
jit add       ✅
jit commit    ✅
jit status    ✅  — fully complete
```

---

*Last updated: February 2026*

# Jit — Day 7 Progress
### jit commit — Nothing to commit guard added

---

## What we built today

Added a `check_for_commit()` function that runs before every commit to block it if there is nothing new to save. This makes `jit commit` behave exactly like real Git.

**Verified working:**
```bash
$ ./jit commit -m "first commit"
Nothing to commit, staging area empty   ← blocked ✅

$ ./jit add main.c
$ ./jit commit -m "first commit"
Jit committed                           ← allowed ✅

$ ./jit commit -m "second commit"
Nothing to commit, no modifications made ← blocked ✅

$ # modify main.c
$ ./jit add main.c
$ ./jit commit -m "some commit"
Jit committed                           ← allowed ✅
```

---

## The check_for_commit() function

A new helper function added to `commit.c` that returns `1` if there is something to commit, `0` if not. Called at the very top of `commitFile()`:

```c
if (check_for_commit() == 0) return 0;
```

### Algorithm — step by step:

**Step 1 — Read index into array of structs:**
```c
struct file_and_hash index[30];
int k = 0;
FILE* i = fopen("./.jit/index", "r");
while (fgets(line, sizeof(line), i)) {
    strncpy(index[k].hash, line, 40);
    index[k].hash[40] = '\0';
    strcpy(index[k].filename, line + 41);
    index[k].filename[strcspn(index[k].filename, "\n")] = '\0';
    k++;
}
fclose(i);
```

**Step 2 — If index is empty, nothing to commit:**
```c
if (k == 0) {
    printf("\nNothing to commit, staging area empty\n");
    return 0;
}
```

**Step 3 — If no previous commits, proceed:**
```c
FILE* master = fopen("./.jit/refs/heads/master", "r");
if (master == NULL) {
    return 1;  // first commit ever, index has files, go ahead
}
```

**Step 4 — Navigate commit → tree object:**
```c
// read commit hash from master
fread(latest_commit, 1, 40, master);
fclose(master);
latest_commit[40] = '\0';

// build path to commit object
strncpy(folder, latest_commit, 2); folder[2] = '\0';
strncpy(file, latest_commit + 2, 38); file[38] = '\0';
snprintf(tree_hash_path, sizeof(tree_hash_path), "./.jit/objects/%s/%s", folder, file);

// read first line of commit object to get tree hash
FILE* commit_file = fopen(tree_hash_path, "r");
fgets(tree_hash, sizeof(tree_hash), commit_file);
fclose(commit_file);
tree_hash[strcspn(tree_hash, "\n")] = '\0';
memmove(tree_hash, tree_hash + 5, strlen(tree_hash + 5) + 1);
tree_hash[strcspn(tree_hash, " \n")] = '\0';

// build path to tree object
strncpy(folder, tree_hash, 2); folder[2] = '\0';
strncpy(file, tree_hash + 2, 38); file[38] = '\0';
snprintf(commit_path, sizeof(commit_path), "./.jit/objects/%s/%s", folder, file);
```

**Step 5 — Read tree object entries into structs:**
```c
struct file_and_hash tree[30];
int tree_count = 0;
FILE* tree_file = fopen(commit_path, "r");
while (fgets(line, 1024, tree_file) != NULL) {
    strncpy(tree[tree_count].hash, line + 5, 40);
    tree[tree_count].hash[40] = '\0';
    strcpy(tree[tree_count].filename, line + 46);
    tree[tree_count].filename[strcspn(tree[tree_count].filename, "\n")] = '\0';
    tree_count++;
}
fclose(tree_file);
```

**Step 6 — Compare index vs tree, count differences:**
```c
int changes = 0;
for (int i = 0; i < k; i++) {
    int found = 0;
    for (int j = 0; j < tree_count; j++) {
        if (strcmp(index[i].filename, tree[j].filename) == 0) {
            found = 1;
            if (strcmp(index[i].hash, tree[j].hash) != 0) {
                changes++;  // same file, different hash → modified
            }
            break;
        }
    }
    if (!found) {
        changes++;  // file in index but not in tree → new file
    }
}
```

**Step 7 — Block or allow based on changes count:**
```c
if (changes == 0) {
    printf("\nNothing to commit, no modifications made\n");
    return 0;
}
return 1;  // changes found, proceed with commit
```

---

## Why rewrite instead of calling staged_yet_to_commit()?

`staged_yet_to_commit()` in status.c is a **display function** — its job is to print output to the user. `check_for_commit()` is a **logic function** — its job is to return yes or no.

Mixing display logic with commit logic would be bad design. Each function should do exactly one thing. So we rewrote the comparison logic inside commit.c as a clean helper that only returns a value, never prints anything (except the blocking messages).

---

## Status of commands:
```
jit init      ✅
jit add       ✅
jit commit    ✅  — now blocks empty/duplicate commits
jit status    ✅
```

---

*Last updated: February 2026*

---
---

# Jit — Day 8 Progress
### jit log — Complete Implementation

---

## Table of Contents
1. [What we built today](#built8)
2. [How jit log works — the algorithm](#algorithm)
3. [The full traversal loop](#loop)
4. [Formatting the timestamp](#timestamp)
5. [Bugs we hit](#bugs)
6. [New C functions learned](#functions)
7. [Full log.c code](#code)

---

## 1. What we built today <a name="built8"></a>

Implemented `jit log` — traverses the full commit chain from latest commit back
to the very first one, printing each commit's details with a formatted date.

**Verified output:**
```bash
$ ./jit log
 tree 52f0a7c8748e...
 parent 016b3dae3b0b...
 author Jeethan
 date: Sat Feb 28 09:53:47 2026
 message new commit
--------------------------------------------------------
 tree 4c5545dcd870...
 parent a924fa3a850e...
 author Jeethan
 date: Fri Feb 27 15:36:27 2026
 message some commit
--------------------------------------------------------
 tree 8ac9400052f1...
 parent none
 author Jeethan
 date: Fri Feb 27 15:35:13 2026
 message first commit
--------------------------------------------------------
```

**Commands now complete:**
```
jit init      ✅
jit add       ✅
jit commit    ✅
jit status    ✅
jit log       ✅  — completed today
```

---

## 2. How jit log works — the algorithm <a name="algorithm"></a>

Git's commit history is a linked list. Each commit object contains the hash of
its parent commit. To traverse the full history you just follow the chain:
```
latest commit
    ↓ parent hash
second commit
    ↓ parent hash
first commit
    ↓ parent = "none"
    stop
```

**Step by step:**

1. Open `.jit/refs/heads/master` and read the latest commit hash
2. Build the path to that commit object using the hash
3. Open the commit object and read all its lines
4. While reading, detect the `parent` line and extract the parent hash
5. Print the commit details (with formatted date)
6. Set `current_hash` to the parent hash
7. If `current_hash` is `"none"` — stop, we've reached the first commit
8. Otherwise loop back to step 2 with the new hash

---

## 3. The full traversal loop <a name="loop"></a>
```c
void jit_log() {
    char current_hash[41];
    FILE* master = fopen("./.jit/refs/heads/master", "r");
    if (master == NULL) {
        perror("Couldn't open master");
        return;
    }
    fread(current_hash, 1, 40, master);
    current_hash[40] = '\0';  // CRITICAL — always null terminate after fread
    fclose(master);

    char folder[3], filename[40], commit_file_path[1024], line[1024];

    while (strcmp(current_hash, "none") != 0) {
        int line_count = 0;  // reset for every commit

        // build path to commit object
        strncpy(folder, current_hash, 2);
        folder[2] = '\0';
        strcpy(filename, current_hash + 2);
        filename[strcspn(filename, "\n")] = '\0';
        snprintf(commit_file_path, sizeof(commit_file_path),
                 "./.jit/objects/%s/%s", folder, filename);

        FILE* commit_file = fopen(commit_file_path, "r");
        if (commit_file == NULL) {
            perror("Couldn't open commit file");
            return;
        }

        // read and print each line
        while (fgets(line, sizeof(line), commit_file) != NULL) {
            line_count++;

            if (line_count == 2) {
                // line 2 is "parent xxxx" — extract for next iteration
                strncpy(current_hash, line + 7, 40);
                current_hash[40] = '\0';
                current_hash[strcspn(current_hash, "\n")] = '\0';
            }

            if (strncmp(line, "timestamp", 9) == 0) {
                // format timestamp into readable date
                time_t t = atol(line + 10);
                struct tm* tm_info = localtime(&t);
                char date[64];
                strftime(date, sizeof(date), "%a %b %d %H:%M:%S %Y", tm_info);
                printf("\n date: %s\n", date);
            } else {
                printf("\n %s", line);
            }
        }
        fclose(commit_file);
        printf("--------------------------------------------------------\n");
    }
}
```

---

## 4. Formatting the timestamp <a name="timestamp"></a>

Commits store time as a Unix timestamp — an integer counting seconds since
January 1 1970. For example `1772186787`. We convert it to a readable string
using three functions from `<time.h>`:
```c
#include <time.h>
#include <stdlib.h>

time_t t = atol(line + 10);          // convert string "1772186787" to integer
struct tm* tm_info = localtime(&t);  // convert to broken-down local time struct
char date[64];
strftime(date, sizeof(date), "%a %b %d %H:%M:%S %Y", tm_info);
// produces: "Sat Feb 28 09:53:47 2026"
```

### strftime format codes:
| Code | Meaning | Example |
|------|---------|---------|
| `%a` | Short weekday | Sat |
| `%b` | Short month | Feb |
| `%d` | Day of month | 28 |
| `%H:%M:%S` | Time | 09:53:47 |
| `%Y` | Full year | 2026 |

---

## 5. Bugs we hit <a name="bugs"></a>

**Bug 1 — Missing null terminator after fread:**
```c
fread(current_hash, 1, 40, master);
// forgot: current_hash[40] = '\0';
```
Without the null terminator, `current_hash` contains garbage after the 40
bytes, causing the path builder to produce a wrong path and fopen to fail.

**Bug 2 — Function name conflict with math library:**
Named the function `log()` which conflicts with C's built-in `log()` math
function (calculates logarithm). Clang caught this:
```
warning: incompatible redeclaration of library function 'log'
```
Fix — rename to `jit_log()` everywhere: log.h, log.c, and main.c.

**Bug 3 — line_count not resetting between commits:**
Declared `line_count` outside the while loop so it kept incrementing across
commits. The `if (line_count == 2)` check never triggered for second and
third commits, meaning the parent hash was never read — infinite loop!
Fix — declare `int line_count = 0` inside the while loop so it resets
for every commit.

**Bug 4 — atol needs stdlib.h:**
`atol()` converts a string to a long integer. It lives in `<stdlib.h>` —
forgetting the include causes a compile error.

---

## 6. New C functions learned <a name="functions"></a>

### atol()
Converts a string to a `long` integer. From `<stdlib.h>`:
```c
long atol(const char* str);

atol("1772186787")  // returns 1772186787 as a long
atol("line + 10")   // skips "timestamp " prefix, converts the number
```

### localtime()
Converts a Unix timestamp (`time_t`) into a broken-down time struct with
separate fields for year, month, day, hour, minute, second. From `<time.h>`:
```c
time_t t = 1772186787;
struct tm* tm_info = localtime(&t);
// tm_info->tm_year, tm_info->tm_mon, tm_info->tm_hour etc.
```

### strftime()
Formats a `struct tm` into a human-readable string using format codes.
From `<time.h>`:
```c
char date[64];
strftime(date, sizeof(date), "%a %b %d %H:%M:%S %Y", tm_info);
// date is now "Sat Feb 28 09:53:47 2026"
```

### strncmp()
Like `strcmp()` but only compares the first `n` characters. Used to detect
line prefixes without caring about the rest:
```c
if (strncmp(line, "timestamp", 9) == 0) {
    // line starts with "timestamp"
}
```

---

## 7. Full log.c code <a name="code"></a>
```c
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "log.h"

void jit_log() {
    char current_hash[41];
    FILE* master = fopen("./.jit/refs/heads/master", "r");
    if (master == NULL) {
        perror("Couldn't open master");
        return;
    }
    fread(current_hash, 1, 40, master);
    current_hash[40] = '\0';
    fclose(master);

    char folder[3], filename[40], commit_file_path[1024], line[1024];

    while (strcmp(current_hash, "none") != 0) {
        int line_count = 0;

        strncpy(folder, current_hash, 2);
        folder[2] = '\0';
        strcpy(filename, current_hash + 2);
        filename[strcspn(filename, "\n")] = '\0';
        snprintf(commit_file_path, sizeof(commit_file_path),
                 "./.jit/objects/%s/%s", folder, filename);

        FILE* commit_file = fopen(commit_file_path, "r");
        if (commit_file == NULL) {
            perror("Couldn't open commit file");
            return;
        }

        while (fgets(line, sizeof(line), commit_file) != NULL) {
            line_count++;
            if (line_count == 2) {
                strncpy(current_hash, line + 7, 40);
                current_hash[40] = '\0';
                current_hash[strcspn(current_hash, "\n")] = '\0';
            }
            if (strncmp(line, "timestamp", 9) == 0) {
                time_t t = atol(line + 10);
                struct tm* tm_info = localtime(&t);
                char date[64];
                strftime(date, sizeof(date), "%a %b %d %H:%M:%S %Y", tm_info);
                printf("\n date: %s\n", date);
            } else {
                printf("\n %s", line);
            }
        }
        fclose(commit_file);
        printf("--------------------------------------------------------\n");
    }
}
```

---

## Status of commands:
```
jit init      ✅
jit add       ✅
jit commit    ✅
jit status    ✅
jit log       ✅
```

---

*Last updated: February 2026*

---

# jit unstage — Implementation

---

## What it does
Removes a file from the staging area (index) without touching the file on disk.
The opposite of `jit add`.

**Usage:**
```bash
./jit unstage main.c
```

---

## Algorithm

**Step 1 — Read entire index into array of structs:**
Open `.jit/index` and read every line into memory. Each line gives you a hash
and a filename. Store both in a `file_and_hash` struct array.

**Step 2 — Check if the file exists in the index:**
Loop through the array using the found flag pattern. If the filename doesn't
match any entry, print an error and return early — don't touch the index.

**Step 3 — Rewrite the index skipping that entry:**
Open `.jit/index` with `"w"` mode (wipes it clean) and loop through all
entries. Write every entry EXCEPT the one that matches the filename. That
entry simply never gets written — it's gone from the index.

---

## The key trick — how the entry gets skipped
```c
for (int i = 0; i < count; i++) {
    if (strcmp(entries[i].filename, filename) != 0) {
        fprintf(indexWrite, "%s %s\n", entries[i].hash, entries[i].filename);
    }
}
```

Condition is `!= 0` — write everyone who does NOT match. The target entry
fails the condition and never gets written. Everyone else passes through
unchanged.

---

## Why the found flag if the loop handles removal?

The found flag is purely for error handling — not for the removal itself.
Without it, if the user types a filename that doesn't exist in the index:
- The loop rewrites the index completely unchanged
- Prints "unstaged: blahblah.c" — lying to the user!

The found flag catches this case and prints an honest error:
```
error: blahblah.c not in staging area
```

---

## Full code
```c
void unstage(char* filename) {
    struct file_and_hash entries[100];
    int count = 0;
    char line[1024];

    FILE* indexRead = fopen("./.jit/index", "r");
    if (indexRead == NULL) {
        perror("Could not open index");
        return;
    }
    while (fgets(line, sizeof(line), indexRead) != NULL) {
        strncpy(entries[count].hash, line, 40);
        entries[count].hash[40] = '\0';
        strcpy(entries[count].filename, line + 41);
        entries[count].filename[strcspn(entries[count].filename, "\n")] = '\0';
        count++;
    }
    fclose(indexRead);

    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].filename, filename) == 0) {
            found = 1;
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
    printf("unstaged: %s\n", filename);
}
```

---

## What changes and what doesn't:

| Thing | Changed? |
|-------|----------|
| `.jit/index` | ✅ entry removed |
| `.jit/objects/` | ❌ blob still exists |
| file on disk | ❌ completely untouched |
| `.jit/refs/heads/master` | ❌ unchanged |

The blob object for that file still exists in `.jit/objects/` — unstage only
removes the index entry. If you want to stage it again just run `jit add`!

---

*Last updated: February 2026*
