import os
import zlib
import hashlib
from config import OBJECTS_DIR, HEAD_FILE, HEADS_DIR

def read_object(sha):
    path = os.path.join(OBJECTS_DIR, sha[:2], sha[2:])
    with open(path, "rb") as f:
        return zlib.decompress(f.read()).split(b"\0", 1)[1]

def get_branch():
    with open(HEAD_FILE) as f:
        return f.read().strip().split(" ")[1].replace("refs/heads/", "")

def get_commit(branch):
    with open(os.path.join(HEADS_DIR, branch)) as f:
        return f.read().strip()

def get_tree(commit):
    data = read_object(commit).decode()
    for l in data.splitlines():
        if l.startswith("tree "):
            return l.split()[1]

def write_object(data):
    header = f"commit {len(data)}\0".encode()
    store = header + data
    sha = hashlib.sha1(store).hexdigest()

    d = os.path.join(OBJECTS_DIR, sha[:2])
    os.makedirs(d, exist_ok=True)

    with open(os.path.join(d, sha[2:]), "wb") as f:
        f.write(zlib.compress(store))

    return sha

def merge(branch_to_merge):
    current = get_branch()
    parent1 = get_commit(current)
    parent2 = get_commit(branch_to_merge)

    tree = get_tree(parent1)

    commit_data = f"""tree {tree}
parent {parent1}
parent {parent2}

Merge branch '{branch_to_merge}'
"""

    sha = write_object(commit_data.encode())

    with open(os.path.join(HEADS_DIR, current), "w") as f:
        f.write(sha)

    print("Merged", branch_to_merge, "into", current)

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python merge.py <branch>")
    else:
        merge(sys.argv[1])
