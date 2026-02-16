import os
import hashlib
import zlib

from config import (
    WORKING_DIR,
    GIT_DIR,
    OBJECTS_DIR,
    INDEX_FILE,
    HEAD_FILE,
    HEADS_DIR,
    TAGS_DIR,
    STASH_FILE
)

# -------------------------
def hash_file(path):
    with open(path,"rb") as f:
        data = f.read()
    store = f"blob {len(data)}\0".encode()+data
    return hashlib.sha1(store).hexdigest()

# -------------------------
def read_object(sha1):
    path = os.path.join(OBJECTS_DIR, sha1[:2], sha1[2:])
    with open(path,"rb") as f:
        return zlib.decompress(f.read()).split(b"\0",1)[1]

# -------------------------
def get_head_commit():
    with open(HEAD_FILE) as f:
        ref=f.read().strip().split(" ")[1]
    with open(f"{GIT_DIR}/{ref}") as f:
        return f.read().strip()

# -------------------------
def get_tree(commit):
    data=read_object(commit).decode()
    for line in data.splitlines():
        if line.startswith("tree "):
            return line.split()[1]

# -------------------------
def read_tree(tree,base):
    result={}
    data=read_object(tree)
    i=0
    while i<len(data):
        end=data.find(b"\0",i)
        mode,name=data[i:end].decode().split(" ")
        sha=data[end+1:end+21].hex()
        i=end+21
        path=os.path.join(base,name)
        if mode=="40000":
            result.update(read_tree(sha,path))
        else:
            result[path]=sha
    return result

# -------------------------
def read_index():
    index={}
    if not os.path.exists(INDEX_FILE):
        return index
    with open(INDEX_FILE) as f:
        for line in f:
            p,h=line.strip().split(" ")
            index[p]=h
    return index

# -------------------------
def get_working_files():
    files={}
    for root,dirs,fs in os.walk(WORKING_DIR):
        if ".git" in dirs:
            dirs.remove(".git")
        for f in fs:
            full=os.path.join(root,f)
            rel=os.path.relpath(full,WORKING_DIR)
            files[rel]=hash_file(full)
    return files

# -------------------------
def git_status():
    index=read_index()
    working=get_working_files()

    head_tree={}
    try:
        head=get_head_commit()
        tree=get_tree(head)
        head_tree=read_tree(tree,"")
    except:
        pass

    print("=== Staged ===")
    for p,h in index.items():
        if p not in head_tree or head_tree[p]!=h:
            print(p)

    print("\n=== Modified ===")
    for p,h in working.items():
        if p in index and index[p]!=h:
            print(p)

    print("\n=== Untracked ===")
    for p in working:
        if p not in index:
            print(p)

# -------------------------
if __name__=="__main__":
    git_status()
