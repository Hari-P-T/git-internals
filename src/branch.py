import os
from config import HEAD_FILE, HEADS_DIR

def get_current_branch():
    with open(HEAD_FILE) as f:
        line = f.read().strip()
    return line.split(" ")[1].replace("refs/heads/", "")

def get_head_commit():
    ref = get_current_branch()
    with open(os.path.join(HEADS_DIR, ref)) as f:
        return f.read().strip()

def list_branches():
    current = get_current_branch()
    for name in os.listdir(HEADS_DIR):
        prefix = "* " if name == current else "  "
        print(prefix + name)

def create_branch(name):
    path = os.path.join(HEADS_DIR, name)

    if os.path.exists(path):
        print("Branch already exists")
        return

    commit = get_head_commit()

    with open(path, "w") as f:
        f.write(commit)

    print("Created branch", name)

if __name__ == "__main__":
    import sys
    if len(sys.argv) == 1:
        list_branches()
    else:
        create_branch(sys.argv[1])
