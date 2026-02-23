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