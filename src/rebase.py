import os
import zlib
import hashlib
from config import OBJECTS_DIR, HEAD_FILE, HEADS_DIR

def read_object(sha):
    path = os.path.join(OBJECTS_DIR, sha[:2], sha[2:])
    with open(path, "rb") as f:
        return zlib.decompress(f.read()).split(b"\0", 1)[1]

def write_commit(data):
    store = f"commit {len(data)}\0".encode() + data
    sha = hashlib.sha1(store).hexdigest()
    d = os.path.join(OBJECTS_DIR, sha[:2])
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, sha[2:]), "wb") as f:
        f.write(zlib.compress(store))
    return sha

def get_current_branch():
    with open(HEAD_FILE) as f:
        return f.read().strip().split(" ")[1].replace("refs/heads/", "")

def get_commit(branch):
    with open(os.path.join(HEADS_DIR, branch)) as f:
        return f.read().strip()

def get_parent(commit):
    data = read_object(commit).decode()
    for line in data.splitlines():
        if line.startswith("parent "):
            return line.split()[1]
    return None

def get_tree(commit):
    data = read_object(commit).decode()
    for line in data.splitlines():
        if line.startswith("tree "):
            return line.split()[1]

def collect_commits_until(base, start):
    commits = []
    cur = start
    while cur and cur != base:
        commits.append(cur)
        cur = get_parent(cur)
    commits.reverse()
    return commits

def rebase(target_branch):
    current = get_current_branch()
    head_commit = get_commit(current)
    target_commit = get_commit(target_branch)

    commits_to_replay = collect_commits_until(target_commit, head_commit)

    new_parent = target_commit

    for old in commits_to_replay:
        tree = get_tree(old)
        msg = read_object(old).decode().split("\n\n", 1)[1]

        commit_data = f"""tree {tree}
parent {new_parent}

{msg}
"""
        new_parent = write_commit(commit_data.encode())

    with open(os.path.join(HEADS_DIR, current), "w") as f:
        f.write(new_parent)

    print("Rebased", current, "onto", target_branch)

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 2:
        print("Usage: python rebase.py <branch>")
    else:
        rebase(sys.argv[1])
