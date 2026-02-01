import os
import hashlib
import zlib
import time

WORKING_DIR = "../dir"
GIT_DIR = f"{WORKING_DIR}/.git"
OBJECTS_DIR = f"{GIT_DIR}/objects"
INDEX_FILE = f"{GIT_DIR}/index"
HEAD_FILE = f"{GIT_DIR}/HEAD"

# -------------------------
# Write object
# -------------------------
def write_object(data, obj_type):
    header = f"{obj_type} {len(data)}\0".encode()
    store = header + data
    sha1 = hashlib.sha1(store).hexdigest()

    obj_dir = f"{OBJECTS_DIR}/{sha1[:2]}"
    obj_file = f"{obj_dir}/{sha1[2:]}"
    os.makedirs(obj_dir, exist_ok=True)

    with open(obj_file, "wb") as f:
        f.write(zlib.compress(store))

    return sha1

# -------------------------
# Read index
# -------------------------
def read_index():
    entries = []
    with open(INDEX_FILE) as f:
        for line in f:
            path, sha = line.strip().split()
            entries.append((path, sha))
    return entries

# -------------------------
# Build directory tree
# -------------------------
def build_structure(entries):
    root = {}
    for path, sha in entries:
        parts = path.split("/")
        cur = root
        for p in parts[:-1]:
            cur = cur.setdefault(p, {})
        cur[parts[-1]] = sha
    return root

# -------------------------
# Write trees recursively
# -------------------------
def write_tree(node):
    content = b""

    for name, value in node.items():
        if isinstance(value, dict):
            tree_hash = write_tree(value)
            content += f"40000 {name}\0".encode() + bytes.fromhex(tree_hash)
        else:
            content += f"100644 {name}\0".encode() + bytes.fromhex(value)

    return write_object(content, "tree")

# -------------------------
# Read HEAD
# -------------------------
def read_head():
    with open(HEAD_FILE) as f:
        data = f.read().strip()
    return data.split(" ")[1]   # refs/heads/main

# -------------------------
# Get parent commit
# -------------------------
def get_parent(ref):
    path = f"{GIT_DIR}/{ref}"
    if not os.path.exists(path):
        return None
    with open(path) as f:
        return f.read().strip()

# -------------------------
# Create commit
# -------------------------
def create_commit(message):
    entries = read_index()
    structure = build_structure(entries)
    root_tree = write_tree(structure)

    ref = read_head()
    parent = get_parent(ref)

    ts = int(time.time())

    lines = []
    lines.append(f"tree {root_tree}")
    if parent:
        lines.append(f"parent {parent}")
    lines.append(f"author You <you@example.com> {ts}")
    lines.append(f"committer You <you@example.com> {ts}")
    lines.append("")
    lines.append(message)

    commit_content = "\n".join(lines).encode()
    commit_hash = write_object(commit_content, "commit")

    with open(f"{GIT_DIR}/{ref}", "w") as f:
        f.write(commit_hash)

    print("Committed:", commit_hash)

# -------------------------
if __name__ == "__main__":
    msg = input("Commit message: ")
    create_commit(msg)
