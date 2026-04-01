# Mini Git

Mini Git is a small Git-internals project built for learning, not for replacing Git.

The repository now has:

- a C++17 CLI implementation in `cpp/`
- the original Python implementation in `src/`
- parity and behavior tests in `tests/`

Both implementations use the same on-disk repository format so we can compare them directly.

## What It Supports

Mini Git implements a compact subset of Git concepts and commands:

- `init`
- `add`
- `commit`
- `status`
- `log`
- `branch`
- `checkout`
- `reset --soft|--mixed|--hard`
- `merge`
- `rebase`
- `stash push|apply|list`
- `remote add <name> <url> <repo>`
- `remote`
- `remote show <name>`
- `push <remote>`
- `pull <remote> [branch]`

Under the hood it models:

- blobs for file contents
- trees for directory snapshots
- commits for history
- refs for branches and `HEAD`
- a simple text index for staging

Objects are SHA-1 addressed, zlib-compressed, and stored under `.git/objects/`.

## Current Shape Of The Project

This project is intentionally simple. It is useful for understanding Git’s storage model and common workflows, but several real Git behaviors are simplified.

Important simplifications:

- `merge` creates a two-parent commit and does not perform conflict resolution.
- `rebase` replays the first-parent chain only and assumes a simple history shape.
- `reset --mixed` and `reset --hard` both restore the working tree and rebuild the index, matching the current reference behavior.
- `stash` stores a single stash entry instead of a stack.
- `push` uploads the full object store and all branch refs for the repo.
- `pull` downloads the full remote object store and branch refs, then restores one selected branch locally.
- There is no detached `HEAD`, reflog, packfile support, or diff engine.

## Repository Layout

```text
git-internals/
├── cpp/
│   ├── include/
│   └── src/
├── server/
│   └── server.js
├── src/
│   ├── cli.py
│   └── *.py
├── tests/
│   └── test_cpp_port.py
├── Makefile
└── README.md
```

Main areas:

- `cpp/src/commands.cpp`: command behavior and CLI dispatch
- `cpp/src/object_store.cpp`: object read/write and zlib handling
- `cpp/src/working_tree.cpp`: working tree restore, scan, and hashing
- `cpp/src/config.cpp`: config discovery and repo path resolution
- `cpp/src/remote.cpp`: remote protocol serialization and HTTP transport
- `server/server.js`: Node.js server for push/pull storage
- `src/`: Python reference implementation kept for parity testing

## Requirements

To build and test the C++ port, you need:

- `g++` with C++17 support
- zlib development headers and library
- `python3` for the test suite
- `curl` for the `push` and `pull` commands

To run the remote server, you also need:

- `node`

The current setup targets Linux/GCC first.

## Build

Build the C++ binary:

```bash
make
```

This produces:

```text
./mini_git
```

Clean generated artifacts:

```bash
make clean
```

## Quick Start

The default working tree lives under `dir/`, based on the current config file in `src/constants/constants`.

Initialize a repo:

```bash
make
./mini_git init
```

Create and stage a file:

```bash
printf 'hello\n' > dir/hello.txt
./mini_git add hello.txt
```

Create a commit:

```bash
printf 'first commit\n' | ./mini_git commit
```

Inspect the repo:

```bash
./mini_git status
./mini_git log
./mini_git branch feature
./mini_git checkout feature
```

## Command Reference

```bash
./mini_git init
./mini_git add <file>
./mini_git add .
./mini_git commit
./mini_git status
./mini_git log
./mini_git branch
./mini_git branch <name>
./mini_git checkout <branch>
./mini_git reset --soft <commit>
./mini_git reset --mixed <commit>
./mini_git reset --hard <commit>
./mini_git merge <branch>
./mini_git rebase <branch>
./mini_git stash push
./mini_git stash apply
./mini_git stash list
./mini_git remote
./mini_git remote add origin http://127.0.0.1:4000 demo
./mini_git remote show origin
./mini_git push origin
./mini_git pull origin
./mini_git pull origin feature
./mini_git push <url> <repo>
./mini_git pull <url> <repo>
./mini_git pull <url> <repo> <branch>
./mini_git help
```

## Remote Push And Pull

Mini Git now supports a very small remote sync model backed by an HTTP server.

The cleaner local workflow is:

- define a named remote once in `.git/config`
- push with the remote name
- pull with the remote name

What `push` does:

- uploads every stored object under `.git/objects/`
- uploads every branch ref under `.git/refs/heads/`
- uploads the current branch name
- uploads the stash ref if one exists

What `pull` does:

- downloads all remote objects
- downloads all remote branch refs
- restores the selected branch locally
- rebuilds the working tree and index from that branch

This is intentionally much simpler than real Git:

- there is no negotiation of missing objects
- there is no authentication
- there is no packfile transport
- there is no conflict handling
- it syncs the whole small learning repo state each time

### Start The Node Server

The remote server uses only built-in Node.js modules.

Run it from the repo root:

```bash
node server/server.js
```

By default it listens on port `4000` and stores remote repos under:

```text
server/data/
```

Optional environment variables:

```bash
MINI_GIT_SERVER_PORT=5000 node server/server.js
MINI_GIT_SERVER_DATA=/tmp/mini-git-data node server/server.js
```

### Example Remote Workflow

Terminal 1:

```bash
node server/server.js
```

Terminal 2:

```bash
./mini_git init
./mini_git remote add origin http://127.0.0.1:4000 demo
printf 'hello\n' > dir/hello.txt
./mini_git add hello.txt
printf 'initial commit\n' | ./mini_git commit
./mini_git push origin
```

In another local clone or temp project with the same `constants/constants` config:

```bash
./mini_git remote add origin http://127.0.0.1:4000 demo
./mini_git pull origin
./mini_git log
./mini_git status
```

Pulling a specific branch:

```bash
./mini_git pull origin feature
```

Showing configured remotes:

```bash
./mini_git remote
./mini_git remote show origin
```

## Configuration

Mini Git reads paths from:

```text
src/constants/constants
```

Default values:

```text
WORKING_DIR=dir
GIT_DIR=.git
OBJECTS_DIR=objects
REFS_DIR=refs
HEADS_DIR=heads
TAGS_DIR=tags
INDEX_FILE=index
HEAD_FILE=HEAD
STASH_FILE=stash
```

The C++ config loader searches upward from the current directory and looks for:

- `src/constants/constants`
- `constants/constants`

That keeps the binary flexible while preserving the current repo format.

## Python Reference Implementation

The Python implementation is still part of the repository and acts as a compatibility reference.

Run it with:

```bash
python3 src/cli.py <command>
```

This is mainly useful for:

- checking parity with the C++ port
- understanding the simpler reference logic
- debugging behavior changes during development

## Testing

Run the test suite with:

```bash
make test
```

The tests cover:

- end-to-end C++ CLI flows
- reset mode behavior
- merge, rebase, and stash behavior
- Python/C++ parity for shared repository state
- remote push/pull sync against a local mock HTTP server

For deterministic commit and stash timestamps during tests, set:

```bash
MINI_GIT_FIXED_TIME=1700000000
```

Example:

```bash
MINI_GIT_FIXED_TIME=1700000000 ./mini_git commit
```

## Learning Notes

Mini Git is best read as a teaching project. If you want to explore it in order, a good path is:

1. `cpp/src/config.cpp`
2. `cpp/src/object_store.cpp`
3. `cpp/src/working_tree.cpp`
4. `cpp/src/commands.cpp`
5. `tests/test_cpp_port.py`

That path shows how configuration, object storage, working tree state, command behavior, and parity verification fit together.

## Future Improvements

Natural next steps would be:

- conflict handling for merge and rebase
- a real diff mechanism
- better reset semantics
- detached `HEAD`
- multiple stash entries
- tag objects
- reflog support
- packfile support

This project is deliberately small enough to understand, change, and test without a large toolchain or framework.
