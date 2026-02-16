import os

from config import (
    WORKING_DIR,
    GIT_DIR,
    OBJECTS_DIR,
    REFS_DIR,
    HEADS_DIR,
    TAGS_DIR,
    INDEX_FILE,
    HEAD_FILE
)

def git_init():

    # Create working directory
    os.makedirs(WORKING_DIR, exist_ok=True)

    # Create .git structure
    os.makedirs(OBJECTS_DIR, exist_ok=True)
    os.makedirs(HEADS_DIR, exist_ok=True)
    os.makedirs(TAGS_DIR, exist_ok=True)

    # Create HEAD file
    if not os.path.exists(HEAD_FILE):
        with open(HEAD_FILE, "w") as f:
            f.write("ref: refs/heads/main\n")

    # Create main branch reference
    main_branch = os.path.join(HEADS_DIR, "main")
    if not os.path.exists(main_branch):
        open(main_branch, "w").close()

    # Create index file
    if not os.path.exists(INDEX_FILE):
        open(INDEX_FILE, "w").close()

    # Create config file
    config_file = os.path.join(GIT_DIR, "config")
    if not os.path.exists(config_file):
        with open(config_file, "w") as f:
            f.write("""[core]
    repositoryformatversion = 0
    filemode = true
    bare = false
""")

    # Create description file
    description_file = os.path.join(GIT_DIR, "description")
    if not os.path.exists(description_file):
        with open(description_file, "w") as f:
            f.write(
                "Unnamed repository; edit this file 'description' to name the repository.\n"
            )

    print("Initialized empty Mini Git repository.")


if __name__ == "__main__":
    git_init()
