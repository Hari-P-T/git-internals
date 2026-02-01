# import os
# import sys

# WORKING_DIR = "../dir"
# GIT_DIR = f"{WORKING_DIR}/.git"
# HEAD_FILE = f"{GIT_DIR}/HEAD"
# REFS_HEADS = f"{GIT_DIR}/refs/heads"

# # -------------------------
# def checkout(branch):
#     path = f"{REFS_HEADS}/{branch}"

#     if not os.path.exists(path):
#         print("Branch does not exist")
#         return

#     with open(HEAD_FILE, "w") as f:
#         f.write(f"ref: refs/heads/{branch}\n")

#     print("Switched to branch", branch)

# # -------------------------
# if __name__ == "__main__":
#     if len(sys.argv) != 2:
#         print("Usage: python mini_git_checkout.py <branch>")
#     else:
#         checkout(sys.argv[1])



#---------------------------------------------------------------------------------#



# import os
# import sys
# import zlib

# WORKING_DIR = "../dir"
# GIT_DIR = f"{WORKING_DIR}/.git"
# OBJECTS_DIR = f"{GIT_DIR}/objects"
# HEAD_FILE = f"{GIT_DIR}/HEAD"
# REFS_HEADS = f"{GIT_DIR}/refs/heads"

# # -------------------------
# def read_object(sha1):
#     path = f"{OBJECTS_DIR}/{sha1[:2]}/{sha1[2:]}"
#     with open(path, "rb") as f:
#         return zlib.decompress(f.read()).split(b"\0",1)[1]

# # -------------------------
# def get_commit_hash(branch):
#     with open(f"{REFS_HEADS}/{branch}") as f:
#         return f.read().strip()

# # -------------------------
# def get_tree_hash(commit_hash):
#     data = read_object(commit_hash).decode()
#     for line in data.splitlines():
#         if line.startswith("tree "):
#             return line.split()[1]

# # -------------------------
# def clear_working_dir():
#     for item in os.listdir(WORKING_DIR):
#         if item == ".git":
#             continue
#         path = os.path.join(WORKING_DIR,item)
#         if os.path.isfile(path):
#             os.remove(path)
#         else:
#             import shutil
#             shutil.rmtree(path)

# # -------------------------
# def restore_tree(tree_hash, base_path):
#     data = read_object(tree_hash)
#     i = 0

#     while i < len(data):
#         end = data.find(b"\0", i)
#         header = data[i:end].decode()
#         mode, name = header.split(" ")
#         sha1 = data[end+1:end+21].hex()

#         i = end + 21

#         full = os.path.join(base_path,name)

#         if mode == "40000":  # directory
#             os.makedirs(full, exist_ok=True)
#             restore_tree(sha1, full)
#         else:  # file
#             content = read_object(sha1)
#             os.makedirs(os.path.dirname(full), exist_ok=True)
#             with open(full,"wb") as f:
#                 f.write(content)

# # -------------------------
# def checkout(branch):
#     path = f"{REFS_HEADS}/{branch}"
#     if not os.path.exists(path):
#         print("Branch does not exist")
#         return

#     # move HEAD
#     with open(HEAD_FILE,"w") as f:
#         f.write(f"ref: refs/heads/{branch}\n")

#     commit = get_commit_hash(branch)
#     tree = get_tree_hash(commit)

#     clear_working_dir()
#     restore_tree(tree, WORKING_DIR)

#     print("Switched to",branch)

# # -------------------------
# if __name__ == "__main__":
#     if len(sys.argv)!=2:
#         print("Usage: python mini_git_checkout.py <branch>")
#     else:
#         checkout(sys.argv[1])





#---------------------------------------------------------------------------------------------------#



import os
import sys
import zlib
import hashlib
import shutil

WORKING_DIR = "../dir"
GIT_DIR = f"{WORKING_DIR}/.git"
OBJECTS_DIR = f"{GIT_DIR}/objects"
HEAD_FILE = f"{GIT_DIR}/HEAD"
REFS_HEADS = f"{GIT_DIR}/refs/heads"
INDEX_FILE = f"{GIT_DIR}/index"

# -------------------------
def read_object(sha1):
    path = f"{OBJECTS_DIR}/{sha1[:2]}/{sha1[2:]}"
    with open(path, "rb") as f:
        return zlib.decompress(f.read()).split(b"\0", 1)[1]

# -------------------------
def get_commit_hash(branch):
    with open(f"{REFS_HEADS}/{branch}") as f:
        return f.read().strip()

# -------------------------
def get_tree_hash(commit_hash):
    data = read_object(commit_hash).decode()
    for line in data.splitlines():
        if line.startswith("tree "):
            return line.split()[1]

# -------------------------
def clear_working_dir():
    for item in os.listdir(WORKING_DIR):
        if item == ".git":
            continue
        path = os.path.join(WORKING_DIR, item)
        if os.path.isfile(path):
            os.remove(path)
        else:
            shutil.rmtree(path)

# -------------------------
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

        if mode == "40000":  # directory
            os.makedirs(full, exist_ok=True)
            restore_tree(sha1, full)
        else:  # file
            content = read_object(sha1)
            os.makedirs(os.path.dirname(full), exist_ok=True)
            with open(full, "wb") as f:
                f.write(content)

# -------------------------
def hash_file(path):
    with open(path, "rb") as f:
        data = f.read()
    store = f"blob {len(data)}\0".encode() + data
    return hashlib.sha1(store).hexdigest()

# -------------------------
def rebuild_index():
    with open(INDEX_FILE, "w") as idx:
        for root, dirs, files in os.walk(WORKING_DIR):
            if ".git" in dirs:
                dirs.remove(".git")
            for f in files:
                full = os.path.join(root, f)
                rel = os.path.relpath(full, WORKING_DIR)
                idx.write(f"{rel} {hash_file(full)}\n")

# -------------------------
def checkout(branch):
    ref_path = f"{REFS_HEADS}/{branch}"

    if not os.path.exists(ref_path):
        print("Branch does not exist")
        return

    # 1) Move HEAD
    with open(HEAD_FILE, "w") as f:
        f.write(f"ref: refs/heads/{branch}\n")

    # 2) Load commit + tree
    commit = get_commit_hash(branch)
    tree = get_tree_hash(commit)

    # 3) Replace working directory
    clear_working_dir()
    restore_tree(tree, WORKING_DIR)

    # 4) Rebuild index
    rebuild_index()

    print("Switched to", branch)

# -------------------------
if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python checkout.py <branch>")
    else:
        checkout(sys.argv[1])
