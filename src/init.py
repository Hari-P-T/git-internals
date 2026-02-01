import os

def git_init():
    base = "../dir/.git"

    os.makedirs(f"{base}/objects", exist_ok=True)
    os.makedirs(f"{base}/refs/heads", exist_ok=True)
    os.makedirs(f"{base}/refs/tags", exist_ok=True)

    # HEAD points to main branch
    with open(f"{base}/HEAD", "w") as f:
        f.write("ref: refs/heads/main\n")

    # Config file
    with open(f"{base}/config", "w") as f:
        f.write("""[core]
    repositoryformatversion = 0
    filemode = true
    bare = false
""")

    # Description file
    with open(f"{base}/description", "w") as f:
        f.write("Unnamed repository; edit this file 'description' to name the repository.\n")

    print("Initialized empty repository")

if __name__ == "__main__":
    git_init()
