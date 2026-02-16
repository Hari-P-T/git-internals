import os

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

PROJECT_ROOT = os.path.dirname(BASE_DIR)

ENV_PATH = os.path.join(PROJECT_ROOT, "constants", "constants")

if not os.path.exists(ENV_PATH):
    raise FileNotFoundError(f".env file not found at {ENV_PATH}")

def load_env():
    env = {}
    with open(ENV_PATH) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            key, value = line.split("=", 1)
            env[key.strip()] = value.strip()
    return env


_env = load_env()

WORKING_DIR = os.path.join(PROJECT_ROOT, _env["WORKING_DIR"])
GIT_DIR = os.path.join(WORKING_DIR, _env["GIT_DIR"])

OBJECTS_DIR = os.path.join(GIT_DIR, _env["OBJECTS_DIR"])
REFS_DIR = os.path.join(GIT_DIR, _env["REFS_DIR"])
HEADS_DIR = os.path.join(REFS_DIR, _env["HEADS_DIR"])
TAGS_DIR = os.path.join(REFS_DIR, _env["TAGS_DIR"])

INDEX_FILE = os.path.join(GIT_DIR, _env["INDEX_FILE"])
HEAD_FILE = os.path.join(GIT_DIR, _env["HEAD_FILE"])
STASH_FILE = os.path.join(REFS_DIR, _env["STASH_FILE"])
