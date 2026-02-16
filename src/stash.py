import os
import time
from commit import write_object, read_index, build_structure, write_tree, read_head
from reset import clear_working_dir, restore_tree, rebuild_index, get_tree
from config import WORKING_DIR, GIT_DIR, STASH_FILE

def get_head_commit():
    ref = read_head()
    path = os.path.join(GIT_DIR, ref)
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return f.read().strip()

def stash_push():
    entries = read_index()
    structure = build_structure(entries)
    root_tree = write_tree(structure)

    head = get_head_commit()
    ts = int(time.time())

    lines = []
    lines.append(f"tree {root_tree}")
    if head:
        lines.append(f"parent {head}")
    lines.append(f"author stash <stash@example.com> {ts}")
    lines.append("")
    lines.append("WIP Stash")

    commit_content = "\n".join(lines).encode()
    stash_commit = write_object(commit_content, "commit")

    with open(STASH_FILE, "w") as f:
        f.write(stash_commit)

    if head:
        tree = get_tree(head)
        clear_working_dir()
        restore_tree(tree, WORKING_DIR)
        rebuild_index()

    print("Saved working directory to stash:", stash_commit)

def stash_apply():
    if not os.path.exists(STASH_FILE):
        print("No stash found")
        return

    with open(STASH_FILE) as f:
        stash_commit = f.read().strip()

    tree = get_tree(stash_commit)

    clear_working_dir()
    restore_tree(tree, WORKING_DIR)
    rebuild_index()

    print("Applied stash:", stash_commit)

def stash_list():
    if not os.path.exists(STASH_FILE):
        print("No stash entries")
        return

    with open(STASH_FILE) as f:
        commit = f.read().strip()

    print("stash@{0}:", commit)

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("Usage: python stash.py push|apply|list")
    elif sys.argv[1] == "push":
        stash_push()
    elif sys.argv[1] == "apply":
        stash_apply()
    elif sys.argv[1] == "list":
        stash_list()
