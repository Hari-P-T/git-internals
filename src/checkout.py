import os
import zlib
import hashlib
import shutil
from config import (
    WORKING_DIR,
    OBJECTS_DIR,
    INDEX_FILE,
    HEAD_FILE,
    HEADS_DIR
)

def read_object(sha1):
    path = os.path.join(OBJECTS_DIR, sha1[:2], sha1[2:])
    with open(path, "rb") as f:
        return zlib.decompress(f.read()).split(b"\0", 1)[1]

def get_commit_hash(branch):
    with open(os.path.join(HEADS_DIR, branch)) as f:
        return f.read().strip()

def get_tree_hash(commit_hash):
    data = read_object(commit_hash).decode()
    for line in data.splitlines():
        if line.startswith("tree "):
            return line.split()[1]

def clear_working_dir():
    for item in os.listdir(WORKING_DIR):
        if item == ".git":
            continue
        path = os.path.join(WORKING_DIR, item)
        if os.path.isfile(path):
            os.remove(path)
        else:
            shutil.rmtree(path)

def restore_tree(tree_hash, base_path):
    data = read_object(tree_hash)
    i = 0

    while i < len(data):
        end = data.find(b"\0", i)
        header = data[i:end].decode()
        mode, name = header.split(" ")
        sha1 = data[end + 1:end + 21].hex()
        i = end + 21

        full = os.path.join(base_path, name)

        if mode == "40000":
            os.makedirs(full, exist_ok=True)
            restore_tree(sha1, full)
        else:
            content = read_object(sha1)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "wb") as f:
                f.write(content)

def hash_file(path):
    with open(path, "rb") as f:
        data = f.read()
    store = f"blob {len(data)}\0".encode() + data
    return hashlib.sha1(store).hexdigest()

def rebuild_index():
    with open(INDEX_FILE, "w") as idx:
        for root, dirs, files in os.walk(WORKING_DIR):
            if ".git" in dirs:
                dirs.remove(".git")
            for f in files:
                full = os.path.join(root, f)
                rel = os.path.relpath(full, WORKING_DIR)
                idx.write(f"{rel} {hash_file(full)}\n")

def checkout(branch):
    ref_path = os.path.join(HEADS_DIR, branch)

    if not os.path.exists(ref_path):
        print("Branch does not exist")
        return

    with open(HEAD_FILE, "w") as f:
        f.write(f"ref: refs/heads/{branch}\n")

    commit = get_commit_hash(branch)
    tree = get_tree_hash(commit)

    clear_working_dir()
    restore_tree(tree, WORKING_DIR)
    rebuild_index()

    print("Switched to", branch)

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python checkout.py <branch>")
    else:
        checkout(sys.argv[1])
