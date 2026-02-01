import os
import hashlib
import zlib

WORKING_DIR = "../dir"
GIT_DIR = f"{WORKING_DIR}/.git"
OBJECTS_DIR = f"{GIT_DIR}/objects"
INDEX_FILE = f"{GIT_DIR}/index"

# -------------------------
# Create blob
# -------------------------
def create_blob(file_path):
    with open(file_path, "rb") as f:
        content = f.read()

    header = f"blob {len(content)}\0".encode()
    store_data = header + content

    sha1 = hashlib.sha1(store_data).hexdigest()

    obj_dir = f"{OBJECTS_DIR}/{sha1[:2]}"
    obj_file = f"{obj_dir}/{sha1[2:]}"
    os.makedirs(obj_dir, exist_ok=True)

    with open(obj_file, "wb") as f:
        f.write(zlib.compress(store_data))

    return sha1

# -------------------------
# Add everything
# -------------------------
def mini_git_add():
    for root, dirs, files in os.walk(WORKING_DIR):

        # Skip .git directory
        if ".git" in dirs:
            dirs.remove(".git")

        for name in files:
            full_path = os.path.join(root, name)

            # store relative path
            rel_path = os.path.relpath(full_path, WORKING_DIR)

            blob = create_blob(full_path)

            with open(INDEX_FILE, "a") as f:
                f.write(f"{rel_path} {blob}\n")

            print("Staged:", rel_path)

# -------------------------
if __name__ == "__main__":
    mini_git_add()
