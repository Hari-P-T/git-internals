import zlib

WORKING_DIR = "../dir"
GIT_DIR = f"{WORKING_DIR}/.git"
OBJECTS_DIR = f"{GIT_DIR}/objects"
HEAD_FILE = f"{GIT_DIR}/HEAD"

# -------------------------
def read_object(sha1):
    path = f"{OBJECTS_DIR}/{sha1[:2]}/{sha1[2:]}"
    with open(path, "rb") as f:
        return zlib.decompress(f.read()).split(b"\0",1)[1]

# -------------------------
def get_head_commit():
    with open(HEAD_FILE) as f:
        ref = f.read().strip().split(" ")[1]
    with open(f"{GIT_DIR}/{ref}") as f:
        return f.read().strip()

# -------------------------
def parse_commit(sha):
    data = read_object(sha).decode()

    parent = None
    message = ""

    for line in data.splitlines():
        if line.startswith("parent "):
            parent = line.split()[1]
        if line == "":
            message = data.split("\n\n",1)[1]
            break

    return parent, message.strip()

# -------------------------
def git_log():
    commit = get_head_commit()

    while commit:
        parent, msg = parse_commit(commit)

        print("commit", commit)
        print("    ", msg)
        print()

        commit = parent

# -------------------------
if __name__ == "__main__":
    git_log()
