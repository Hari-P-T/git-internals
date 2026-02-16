import os
import sys
import hashlib
import zlib

from config import (
    WORKING_DIR,
    OBJECTS_DIR,
    INDEX_FILE
)

# -------------------------
# Create blob
# -------------------------
def create_blob(file_path):
    with open(file_path, "rb") as f:
        content = f.read()

    header = f"blob {len(content)}\0".encode()
    store_data = header + content

    sha1 = hashlib.sha1(store_data).hexdigest()

    obj_dir = os.path.join(OBJECTS_DIR, sha1[:2])
    obj_file = os.path.join(obj_dir, sha1[2:])
    os.makedirs(obj_dir, exist_ok=True)

    with open(obj_file, "wb") as f:
        f.write(zlib.compress(store_data))

    return sha1


# -------------------------
# Update index safely
# -------------------------
def update_index(path, blob_hash):
    entries = {}

    # Read existing index
    if os.path.exists(INDEX_FILE):
        with open(INDEX_FILE) as f:
            for line in f:
                p, h = line.strip().split()
                entries[p] = h

    # Update or insert
    entries[path] = blob_hash

    # Rewrite index (avoid duplicates)
    with open(INDEX_FILE, "w") as f:
        for p, h in entries.items():
            f.write(f"{p} {h}\n")


# -------------------------
# Add single file
# -------------------------
def add_file(file_path):
    full_path = os.path.join(WORKING_DIR, file_path)

    if not os.path.exists(full_path):
        print("File not found:", file_path)
        return

    if os.path.isdir(full_path):
        add_directory(file_path)
        return

    rel_path = os.path.relpath(full_path, WORKING_DIR)
    blob = create_blob(full_path)

    update_index(rel_path, blob)

    print("Staged:", rel_path)


# -------------------------
# Add directory recursively
# -------------------------
def add_directory(dir_path):
    base = os.path.join(WORKING_DIR, dir_path)

    for root, dirs, files in os.walk(base):
        if ".git" in dirs:
            dirs.remove(".git")

        for name in files:
            full_path = os.path.join(root, name)
            rel_path = os.path.relpath(full_path, WORKING_DIR)
            blob = create_blob(full_path)
            update_index(rel_path, blob)
            print("Staged:", rel_path)


# -------------------------
# Main add function
# -------------------------
def mini_git_add(paths):

    if not paths or paths == ["."]:
        add_directory(".")
        return

    for path in paths:
        add_file(path)


# -------------------------
if __name__ == "__main__":
    args = sys.argv[1:]
    mini_git_add(args)
