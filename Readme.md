MINI GIT – BUILD GIT FROM SCRATCH (PYTHON)
==========================================

A complete from‑scratch implementation of Git internals in Python.

This project recreates how Git works under the hood including:

• Blob storage
• Tree objects
• Commits
• Branches
• Index (staging area)
• Checkout
• Reset (soft / mixed / hard)
• Merge
• Rebase
• Log
• Status


WHY THIS PROJECT?
----------------
Most developers USE Git.
Very few actually UNDERSTAND Git.

This project helps you learn:

• How Git stores objects
• How commits form a DAG
• How branches are pointers
• How staging works
• How reset/merge/rebase behave internally

After implementing this, Git stops feeling like magic.


PROJECT STRUCTURE
-----------------
src/

init.py        → git init
add.py         → git add
commit.py      → git commit
status.py      → git status
log.py         → git log
branch.py      → git branch
checkout.py    → git checkout
reset.py       → git reset
merge.py       → git merge
rebase.py      → git rebase


GIT INTERNALS OVERVIEW
---------------------

Git is a content‑addressable database.

Everything is stored as OBJECTS:

1) Blob   → file content
2) Tree   → directory structure
3) Commit → snapshot + metadata

Each object is:

• SHA1 hashed
• zlib compressed
• stored in .git/objects/


STORAGE LAYOUT
--------------

.git/
 ├─ objects/
 ├─ refs/heads/
 ├─ HEAD
 ├─ index
 └─ config


SETUP
-----

Python 3.8+

Run inside src folder:

    python init.py


END‑TO‑END WORKFLOW
-------------------

1) Initialize repository
   python init.py

2) Add files
   python add.py

3) Commit
   python commit.py

4) Status
   python status.py

5) Log
   python log.py

6) Create branch
   python branch.py feature

7) Checkout branch
   python checkout.py feature

8) Reset
   python reset.py --soft <commit>
   python reset.py --mixed <commit>
   python reset.py --hard <commit>

9) Merge
   python merge.py branch-name

10) Rebase
    python rebase.py target-branch


COMMAND DETAILS
---------------

INIT
Creates .git directory structure and HEAD.

ADD
Creates blob objects and writes entries to index.

COMMIT
Reads index → builds tree → creates commit → updates branch ref.

STATUS
Compares working directory, index, and last commit.

LOG
Walks commit parents and prints history.

BRANCH
Creates simple commit pointers under refs/heads.

CHECKOUT
Moves HEAD and restores working tree from commit snapshot.

RESET
soft  → move HEAD only
mixed → reset index
hard  → reset working directory + index

MERGE
Creates commit with two parents.

REBASE
Replays commits on new base (history rewriting).


INTERNAL FLOW
-------------

Add:
file → blob → objects → index

Commit:
index → tree → commit → branch pointer

Checkout:
commit → tree → restore files

Reset:
move pointer → optionally restore files

Merge:
two parents → merge commit

Rebase:
replay commits → rewrite history


SAMPLE WORKFLOW
---------------

echo hello > a.txt
python add.py
python commit.py

python branch.py feature
python checkout.py feature

echo world >> a.txt
python add.py
python commit.py

python checkout.py main
python merge.py feature

python log.py


LEARNING OUTCOMES
----------------
After this project you understand:

• Object database design
• SHA1 hashing
• Snapshot based version control
• Branch pointers
• DAG history
• Merge vs rebase
• Git plumbing commands


FUTURE IDEAS
------------
• Packfiles
• Diff engine
• Conflict resolution
• Tags
• Reflog
• Interactive rebase
• Partial staging


Built for learning Git deeply.
