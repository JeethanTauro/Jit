# Jit — A Git Implementation in C

Built from scratch in 10 days. Jit implements the core version control features of Git including content addressed object storage, SHA1 hashing, commit history traversal, staging area management and branch switching.

This project was built as a deep dive into understanding how Git actually works internally — not just how to use it, but how blobs, trees, commits, and the object store fit together.

---

## How it works internally

Jit stores everything inside a `.jit/` directory created in your project root. Under the hood it uses the same design as real Git:

- **Blobs** — raw file contents, stored as SHA1-hashed objects
- **Trees** — snapshots of the staging area at commit time, stored as objects
- **Commits** — metadata (author, timestamp, message) pointing to a tree and a parent commit
- **Index** — the staging area, a flat file listing currently staged files and their hashes
- **Refs** — branch files in `.jit/refs/heads/` each containing a commit hash
- **HEAD** — a file pointing to the currently active branch

Every object is stored in `.jit/objects/xx/xxxxxxxx` where the first two characters of the SHA1 hash are the folder and the remaining 38 are the filename. This is identical to how real Git stores objects.

---

## Requirements

- Linux or macOS
- GCC compiler
- OpenSSL library (`libssl-dev`)

```bash
sudo apt install libssl-dev   # Ubuntu/Debian
brew install openssl          # macOS
```

---

## Installation

**Clone the repo:**
```bash
git clone https://github.com/jeethan/jit
cd jit
```

**Compile:**
```bash
gcc main.c init.c add.c commit.c status.c log.c unstage.c branch.c delete_branch.c switch_branch.c reset.c check_branch.c -lssl -lcrypto -o jit
```

**Add to PATH so you can use it anywhere:**
```bash
sudo mv jit /usr/local/bin/jit
```

---

## Commands

### `jit init`
Initialises a new jit repository in the current directory. Creates the `.jit/` folder structure including `objects/`, `refs/heads/`, `HEAD`, and `index`.

```bash
jit init
```

---

### `jit add <filename>`
Stages a file for commit. Hashes the file contents with SHA1, stores the blob object in `.jit/objects/`, and updates the index. Supports multiple files at once.

```bash
jit add main.c
jit add main.c add.c commit.c
```

If the file is already in the index, its hash is updated — no duplicate entries.

---

### `jit commit -m "message"`
Creates a commit from the current staging area. Builds a tree object from the index, then creates a commit object pointing to the tree, the parent commit, author, timestamp and message. Updates the current branch to point to the new commit.

```bash
jit commit -m "first commit"
```

Blocked if:
- Nothing is staged
- Index matches the last commit exactly (nothing changed)

---

### `jit status`
Shows the current state of the working directory compared to the index and last commit.

```bash
jit status
```

Three sections:
- **Changes not staged for commit** — files modified on disk since last `jit add`
- **Untracked files** — files on disk not tracked by jit
- **Changes to be committed** — files staged that differ from last commit

Respects `.jitignore` — files listed there are hidden from the untracked list.

---

### `jit log`
Traverses the commit chain from the latest commit back to the first, printing each commit's details with a formatted timestamp.

```bash
jit log
```

Output:
```
tree abc123...
parent def456...
author Jeethan
date: Sun Mar 01 15:30:00 2026
message first commit
--------------------------------------------------------
```

---

### `jit unstage <filename>`
Removes a file from the staging area without touching the file on disk.

```bash
jit unstage main.c
```

---

### `jit branch <name>`
Creates a new branch pointing to the current commit.

```bash
jit branch feature
```

### `jit branch -d <name>`
Deletes a branch. Cannot delete the branch you are currently on.

```bash
jit branch -d feature
```

---

### `jit where`
Lists all branches, marking the current branch.

```bash
jit where
```

Output:
```
Branches created:
    master <-current branch
    feature
```

---

### `jit switch <branchname>`
Switches to another branch. Restores all files on disk to match the target branch's last commit. Updates the index and HEAD.

```bash
jit switch feature
```

Blocked if:
- Branch doesn't exist
- Already on that branch
- You have unstaged modifications on disk
- You have staged but uncommitted changes

---

### `jit reset <commit-hash>`
Resets the current branch to a previous commit. Restores all files to match that commit's state. Deletes files that didn't exist at that commit. Moves the branch pointer back.

```bash
jit reset a9f3b2e1...
```

Shows a warning and asks for confirmation before proceeding — this operation is destructive and commits after the target are lost from the branch.

```
warning: this will reset your branch to commit a9f3b2e
all commits after this point will be lost!
do you want to continue? (yes/no):
```

---

## .jitignore

Create a `.jitignore` file in your project root to hide files from the untracked list:

```
CMakeLists.txt
todo
build
```

One filename per line. These files will not appear in `jit status` as untracked. Note that you can still manually `jit add` an ignored file if you want to — jit does not block this.

---

## How jit compares to Git

| Feature | Git | Jit |
|---------|-----|-----|
| Object storage (blobs, trees, commits) | ✅ | ✅ |
| SHA1 content addressing | ✅ | ✅ |
| Staging area (index) | ✅ | ✅ |
| Commit history | ✅ | ✅ |
| Branching | ✅ | ✅ |
| Branch switching | ✅ | ✅ |
| Hard reset | ✅ | ✅ |
| .gitignore / .jitignore | ✅ | ✅ |
| Subdirectory support | ✅ | ❌ |
| Merging | ✅ | ❌ |
| git revert | ✅ | ❌ |
| git stash | ✅ | ❌ |
| git diff | ✅ | ❌ |
| git remote / push / pull | ✅ | ❌ |
| Reflog (recovering lost commits) | ✅ | ❌ |
| Packed objects | ✅ | ❌ |
| Dynamic memory allocation | ✅ | ❌ (static arrays) |
| Windows support | ✅ | ❌ (Linux/macOS only) |

---

## Known Limitations

**No subdirectory support:**
Jit only tracks files in the root directory where `jit init` was run. Files inside subdirectories like `src/helper.c` are not tracked. Supporting subdirectories would require recursive tree objects and recursive directory traversal — a significant addition.

**No merge:**
`jit switch` works but there is no `jit merge`. Branches can be created, committed on independently, and switched between — but merging two diverged branches is not implemented. A fast-forward merge (where one branch is directly ahead of the other) would be a reasonable next step.

**No git revert:**
`jit reset` moves the branch pointer backward (destructive). Real Git's `git revert` creates a NEW commit that undoes a previous one without destroying history. Jit does not have this — once you reset, the newer commits are gone from that branch.

**No stash:**
If you have uncommitted changes and want to switch branches, jit blocks you and tells you to commit first. Real Git offers `git stash` to temporarily save your changes and restore them later. Jit does not have this — you must commit or discard before switching.

**No reflog:**
Real Git keeps a log of every position HEAD has ever pointed to. This means even after a hard reset you can recover lost commits using `git reflog`. Jit has no such safety net — a reset is final.

**Static memory allocation:**
All internal arrays use fixed sizes (e.g. max 100 files in index, max 30 tree entries). A proper implementation would use dynamic memory with `realloc()` to support repositories of any size.

**No remote support:**
There is no `jit push`, `jit pull`, `jit clone`, or `jit remote`. Jit is a local-only version control system.

**Linux and macOS only:**
Uses POSIX APIs (`opendir`, `readdir`, `dirent.h`, `mkdir`) and OpenSSL which are not available on Windows without additional tooling like WSL or MinGW.

**File size limit:**
Files larger than 64KB (65536 bytes) will be truncated when staged. A proper implementation would dynamically allocate a buffer sized to the actual file.

---

## Project structure

```
jit/
    main.c              ← entry point, command routing
    init.c / init.h     ← jit init
    add.c / add.h       ← jit add
    commit.c / commit.h ← jit commit
    status.c / status.h ← jit status
    log.c / log.h       ← jit log
    unstage.c           ← jit unstage
    branch.c            ← jit branch
    delete_branch.c     ← jit branch -d
    check_branch.c      ← jit where
    switch_branch.c     ← jit switch
    reset.c             ← jit reset
```

---

## What I learned building this

- How Git's content addressed object store works internally
- SHA1 hashing and hex encoding
- The blob → tree → commit object chain
- How branches are just files containing a commit hash
- How the index (staging area) relates to tree objects
- Memory management in C — stack vs heap, buffer sizing
- File I/O in C — fopen, fread, fwrite, fgets
- Directory traversal with opendir and readdir
- String manipulation — strncpy, strcpy, memmove for overlapping memory
- Why memmove is needed when source and destination overlap
- Dynamic memory with malloc, realloc, free
- Struct arrays for storing multiple related values
- The found flag pattern for searching arrays
- Recursive data structures — how commits chain via parent hashes

---
## Looking Forward
- Implement Dynamic Memory allocation throughout the codebase
- Implement more of Git features like merge,diff,stash etc

*Built in 10 days — March 2026*
