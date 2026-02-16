
# Mini Git – A Git Implementation From Scratch (Python)

Mini Git is a from-scratch implementation of Git internals written in pure Python.  
It recreates how Git works under the hood — including object storage, commits, trees, branches, reset, merge, rebase, and stash.

This project is built for deep learning of Git internals, not just usage.

---

## Features

- Initialize repository
- Add single or multiple files
- Staging area (index)
- Commit creation
- Branch creation & listing
- Checkout branches
- Reset (soft / mixed / hard)
- Merge (2-parent commits)
- Rebase (history rewriting)
- Stash (push / apply / list)
- Status (staged / modified / untracked)
- Log (commit history traversal)
- Fully configurable via `constants`
- No external dependencies

---

## Project Structure

git-internals/
├── .env
├── setup.py
├── dir/                # Working directory (auto created)
└── src/
    ├── cli.py
    ├── config.py
    ├── init.py
    ├── add.py
    ├── commit.py
    ├── status.py
    ├── log.py
    ├── branch.py
    ├── checkout.py
    ├── reset.py
    ├── merge.py
    ├── rebase.py
    └── stash.py

---

## Git Internals Overview

Mini Git implements the core Git object model:

Blob   → File content  
Tree   → Directory structure  
Commit → Snapshot + metadata  

Each object is:
- SHA1 hashed
- Zlib compressed
- Stored inside `.git/objects/`

---

## Configuration

All paths are defined in `.env`:

WORKING_DIR=dir
GIT_DIR=.git
OBJECTS_DIR=objects
REFS_DIR=refs
HEADS_DIR=heads
TAGS_DIR=tags
INDEX_FILE=index
HEAD_FILE=HEAD
STASH_FILE=stash

Paths are loaded via `config.py` (pure Python, no external packages).

---

## Setup

Initialize repository structure:

    python setup.py

This creates:

dir/
└── .git/
    ├── objects/
    ├── refs/
    │   ├── heads/
    │   └── tags/
    ├── HEAD
    ├── index
    ├── config
    └── description

---

## Usage

Run all commands through:

    python src/cli.py <command>

Example commands:

    python src/cli.py init
    python src/cli.py add file.txt
    python src/cli.py add .
    python src/cli.py commit
    python src/cli.py status
    python src/cli.py log
    python src/cli.py branch feature
    python src/cli.py checkout feature
    python src/cli.py reset --hard <commit>
    python src/cli.py merge feature
    python src/cli.py rebase main
    python src/cli.py stash push

---

## Internal Flow

Add:
file → blob → object store → index

Commit:
index → tree → commit → branch pointer

Checkout:
commit → tree → restore working directory

Reset:
move HEAD → optionally restore tree & index

Merge:
create commit with 2 parents

Rebase:
replay commits onto new base

Stash:
save working state → reset → reapply later

---

## Learning Outcomes

After exploring this project, you will understand:

- Content-addressable storage
- SHA1 hashing
- Snapshot-based version control
- Commit DAG structure
- Branch pointers
- History rewriting (rebase)
- Merge commits
- Staging area mechanics

---

## Future Improvements

- Conflict resolution engine
- Diff algorithm
- Packfile implementation
- Detached HEAD support
- Tag objects
- Reflog
- Interactive rebase

---

Built to deeply understand Git internals and version control system design.
