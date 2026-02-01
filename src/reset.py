import os
import sys
import zlib
import hashlib
import shutil

WORKING_DIR = "../dir"
GIT_DIR = f"{WORKING_DIR}/.git"
OBJECTS_DIR = f"{GIT_DIR}/objects"
HEAD_FILE = f"{GIT_DIR}/HEAD"
INDEX_FILE = f"{GIT_DIR}/index"

# -------------------------
def read_object(sha1):
    path = f"{OBJECTS_DIR}/{sha1[:2]}/{sha1[2:]}"
    with open(path, "rb") as f:
        return zlib.decompress(f.read()).split(b"\0",1)[1]

# -------------------------
def get_current_branch():
    with open(HEAD_FILE) as f:
        return f.read().strip().split(" ")[1].replace("refs/heads/","")

# -------------------------
def update_branch(commit):
    branch = get_current_branch()
    with open(f"{GIT_DIR}/refs/heads/{branch}","w") as f:
        f.write(commit)

# -------------------------
def get_tree(commit):
    data = read_object(commit).decode()
    for line in data.splitlines():
        if line.startswith("tree "):
            return line.split()[1]

# -------------------------
def clear_working_dir():
    for item in os.listdir(WORKING_DIR):
        if item==".git": continue
        p=os.path.join(WORKING_DIR,item)
        if os.path.isfile(p): os.remove(p)
        else: shutil.rmtree(p)

# -------------------------
def restore_tree(tree,base):
    data=read_object(tree)
    i=0
    while i<len(data):
        end=data.find(b"\0",i)
        mode,name=data[i:end].decode().split(" ")
        sha=data[end+1:end+21].hex()
        i=end+21
        full=os.path.join(base,name)

        if mode=="40000":
            os.makedirs(full,exist_ok=True)
            restore_tree(sha,full)
        else:
            content=read_object(sha)
            os.makedirs(os.path.dirname(full),exist_ok=True)
            with open(full,"wb") as f:
                f.write(content)

# -------------------------
def rebuild_index():
    def hash_file(p):
        with open(p,"rb") as f:
            d=f.read()
        store=f"blob {len(d)}\0".encode()+d
        return hashlib.sha1(store).hexdigest()

    with open(INDEX_FILE,"w") as idx:
        for r,dirs,files in os.walk(WORKING_DIR):
            if ".git" in dirs:
                dirs.remove(".git")
            for f in files:
                full=os.path.join(r,f)
                rel=os.path.relpath(full,WORKING_DIR)
                idx.write(f"{rel} {hash_file(full)}\n")

# -------------------------
def reset(commit, mode):
    # move branch pointer
    update_branch(commit)

    if mode in ("mixed","hard"):
        tree=get_tree(commit)
        clear_working_dir()
        restore_tree(tree,WORKING_DIR)
        rebuild_index()

    if mode=="soft":
        pass  # only HEAD moves

    print("Reset",mode,"to",commit)

# -------------------------
if __name__=="__main__":
    if len(sys.argv)<3:
        print("Usage: python reset.py --soft|--mixed|--hard <commit>")
    else:
        reset(sys.argv[2], sys.argv[1][2:])
