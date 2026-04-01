import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
CPP_BIN = ROOT / "mini_git"
PYTHON = sys.executable
PYTHON_REFERENCE = ROOT / "src" / "cli.py"
DEFAULT_CONSTANTS = """WORKING_DIR=dir
GIT_DIR=.git
OBJECTS_DIR=objects
REFS_DIR=refs
HEADS_DIR=heads
TAGS_DIR=tags
INDEX_FILE=index
HEAD_FILE=HEAD
STASH_FILE=stash
"""


def blob_hash(data: bytes) -> str:
    store = f"blob {len(data)}\0".encode() + data
    return hashlib.sha1(store).hexdigest()


class MiniGitCppPortTests(unittest.TestCase):
    def make_project(self):
        temp_dir = tempfile.TemporaryDirectory()
        project_root = Path(temp_dir.name) / "project"
        (project_root / "constants").mkdir(parents=True, exist_ok=True)
        constants_source = ROOT / "constants" / "constants"
        if constants_source.exists():
            shutil.copy2(constants_source, project_root / "constants" / "constants")
        else:
            (project_root / "constants" / "constants").write_text(DEFAULT_CONSTANTS)

        if PYTHON_REFERENCE.exists():
            shutil.copytree(ROOT / "src", project_root / "src")

        return temp_dir, project_root

    def run_cli(self, executable, project_root, *args, input_text=None, extra_env=None):
        env = os.environ.copy()
        if extra_env:
            env.update(extra_env)

        command = executable
        if executable == "python":
            command = [PYTHON, str(project_root / "src" / "cli.py")]

        result = subprocess.run(
            [*command, *args],
            cwd=project_root,
            text=True,
            input=input_text,
            capture_output=True,
            env=env,
            check=True,
        )
        self.assertEqual(result.stderr, "")
        return result.stdout

    def run_cpp(self, project_root, *args, input_text=None, extra_env=None):
        return self.run_cli([str(CPP_BIN)], project_root, *args, input_text=input_text, extra_env=extra_env)

    def run_python(self, project_root, *args, input_text=None, extra_env=None):
        return self.run_cli("python", project_root, *args, input_text=input_text, extra_env=extra_env)

    def git_dir(self, project_root):
        return project_root / "dir" / ".git"

    def write_working_file(self, project_root, relative_path, content):
        path = project_root / "dir" / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)

    def read_working_file(self, project_root, relative_path):
        return (project_root / "dir" / relative_path).read_text()

    def read_ref(self, project_root, branch="main"):
        return (self.git_dir(project_root) / "refs" / "heads" / branch).read_text().strip()

    def read_index(self, project_root):
        return (self.git_dir(project_root) / "index").read_text()

    def assert_same_repo_state(self, left_root, right_root):
        for relative in [
            Path("dir/.git/index"),
            Path("dir/.git/HEAD"),
            Path("dir/.git/refs/heads/main"),
        ]:
            self.assertEqual((left_root / relative).read_text(), (right_root / relative).read_text())

        left_objects = sorted(
            path.relative_to(left_root / "dir" / ".git" / "objects").as_posix()
            for path in (left_root / "dir" / ".git" / "objects").rglob("*")
            if path.is_file()
        )
        right_objects = sorted(
            path.relative_to(right_root / "dir" / ".git" / "objects").as_posix()
            for path in (right_root / "dir" / ".git" / "objects").rglob("*")
            if path.is_file()
        )
        self.assertEqual(left_objects, right_objects)

        for relative in left_objects:
            self.assertEqual(
                (left_root / "dir" / ".git" / "objects" / relative).read_bytes(),
                (right_root / "dir" / ".git" / "objects" / relative).read_bytes(),
            )

    def test_basic_end_to_end_commands(self):
        temp_dir, project_root = self.make_project()
        self.addCleanup(temp_dir.cleanup)

        self.assertEqual(self.run_cpp(project_root, "init"), "Initialized empty Mini Git repository.\n")

        self.write_working_file(project_root, "note.txt", "hello from c++\n")
        add_output = self.run_cpp(project_root, "add", "note.txt")
        self.assertEqual(add_output, "Staged: note.txt\n")

        commit_output = self.run_cpp(
            project_root,
            "commit",
            input_text="first commit\n",
            extra_env={"MINI_GIT_FIXED_TIME": "1700000000"},
        )
        self.assertTrue(commit_output.startswith("Commit message: Committed: "))
        commit_hash = self.read_ref(project_root)
        self.assertIn(commit_hash, commit_output)

        status_output = self.run_cpp(project_root, "status")
        self.assertEqual(status_output, "=== Staged ===\n\n=== Modified ===\n\n=== Untracked ===\n")

        log_output = self.run_cpp(project_root, "log")
        self.assertEqual(log_output, f"commit {commit_hash}\n     first commit\n\n")

        branch_output = self.run_cpp(project_root, "branch", "feature")
        self.assertEqual(branch_output, "Created branch feature\n")

        checkout_output = self.run_cpp(project_root, "checkout", "feature")
        self.assertEqual(checkout_output, "Switched to feature\n")

        self.write_working_file(project_root, "note.txt", "feature branch\n")
        self.run_cpp(project_root, "add", "note.txt")
        self.run_cpp(
            project_root,
            "commit",
            input_text="feature commit\n",
            extra_env={"MINI_GIT_FIXED_TIME": "1700000001"},
        )

        main_checkout = self.run_cpp(project_root, "checkout", "main")
        self.assertEqual(main_checkout, "Switched to main\n")
        self.assertEqual(self.read_working_file(project_root, "note.txt"), "hello from c++\n")

    def test_reset_modes(self):
        temp_dir, project_root = self.make_project()
        self.addCleanup(temp_dir.cleanup)

        self.run_cpp(project_root, "init")
        self.write_working_file(project_root, "file.txt", "v1\n")
        self.run_cpp(project_root, "add", "file.txt")
        self.run_cpp(project_root, "commit", input_text="v1\n", extra_env={"MINI_GIT_FIXED_TIME": "1700000010"})
        commit1 = self.read_ref(project_root)

        self.write_working_file(project_root, "file.txt", "v2\n")
        self.run_cpp(project_root, "add", "file.txt")
        self.run_cpp(project_root, "commit", input_text="v2\n", extra_env={"MINI_GIT_FIXED_TIME": "1700000011"})
        commit2 = self.read_ref(project_root)

        soft_output = self.run_cpp(project_root, "reset", "--soft", commit1)
        self.assertEqual(soft_output, f"Reset soft to {commit1}\n")
        self.assertEqual(self.read_ref(project_root), commit1)
        self.assertEqual(self.read_working_file(project_root, "file.txt"), "v2\n")
        self.assertIn(blob_hash(b"v2\n"), self.read_index(project_root))

        mixed_output = self.run_cpp(project_root, "reset", "--mixed", commit1)
        self.assertEqual(mixed_output, f"Reset mixed to {commit1}\n")
        self.assertEqual(self.read_working_file(project_root, "file.txt"), "v1\n")
        self.assertIn(blob_hash(b"v1\n"), self.read_index(project_root))

        self.write_working_file(project_root, "file.txt", "v3\n")
        self.run_cpp(project_root, "add", "file.txt")
        self.run_cpp(project_root, "commit", input_text="v3\n", extra_env={"MINI_GIT_FIXED_TIME": "1700000012"})
        commit3 = self.read_ref(project_root)
        self.assertNotEqual(commit2, commit3)

        hard_output = self.run_cpp(project_root, "reset", "--hard", commit1)
        self.assertEqual(hard_output, f"Reset hard to {commit1}\n")
        self.assertEqual(self.read_ref(project_root), commit1)
        self.assertEqual(self.read_working_file(project_root, "file.txt"), "v1\n")
        self.assertIn(blob_hash(b"v1\n"), self.read_index(project_root))

    def test_merge_rebase_and_stash_behavior(self):
        temp_dir, project_root = self.make_project()
        self.addCleanup(temp_dir.cleanup)

        self.run_cpp(project_root, "init")
        self.write_working_file(project_root, "base.txt", "base\n")
        self.run_cpp(project_root, "add", "base.txt")
        self.run_cpp(project_root, "commit", input_text="base\n", extra_env={"MINI_GIT_FIXED_TIME": "1700000020"})
        main_commit = self.read_ref(project_root)

        self.run_cpp(project_root, "branch", "feature")
        self.run_cpp(project_root, "checkout", "feature")
        self.write_working_file(project_root, "feature.txt", "feature\n")
        self.run_cpp(project_root, "add", "feature.txt")
        self.run_cpp(project_root, "commit", input_text="feature\n", extra_env={"MINI_GIT_FIXED_TIME": "1700000021"})
        feature_commit = self.read_ref(project_root, "feature")

        self.run_cpp(project_root, "checkout", "main")
        merge_output = self.run_cpp(project_root, "merge", "feature")
        self.assertEqual(merge_output, "Merged feature into main\n")
        merge_commit = self.read_ref(project_root)
        self.assertNotEqual(merge_commit, main_commit)
        merge_body = self.read_object_body(project_root, merge_commit)
        self.assertIn(f"parent {main_commit}\n".encode(), merge_body)
        self.assertIn(f"parent {feature_commit}\n".encode(), merge_body)

        self.run_cpp(project_root, "checkout", "feature")
        rebase_output = self.run_cpp(project_root, "rebase", "main")
        self.assertEqual(rebase_output, "Rebased feature onto main\n")
        rebased_commit = self.read_ref(project_root, "feature")
        self.assertNotEqual(rebased_commit, feature_commit)
        rebased_body = self.read_object_body(project_root, rebased_commit).decode()
        self.assertTrue(rebased_body.endswith("feature\n"))
        self.assertIn(merge_commit, self.first_parent_chain(project_root, rebased_commit))

        self.write_working_file(project_root, "base.txt", "stashed\n")
        self.run_cpp(project_root, "add", "base.txt")
        stash_push_output = self.run_cpp(
            project_root,
            "stash",
            "push",
            extra_env={"MINI_GIT_FIXED_TIME": "1700000022"},
        )
        self.assertTrue(stash_push_output.startswith("Saved working directory to stash: "))
        self.assertEqual(self.read_working_file(project_root, "base.txt"), "base\n")

        stash_list_output = self.run_cpp(project_root, "stash", "list")
        self.assertTrue(stash_list_output.startswith("stash@{0}: "))

        stash_apply_output = self.run_cpp(project_root, "stash", "apply")
        self.assertTrue(stash_apply_output.startswith("Applied stash: "))
        self.assertEqual(self.read_working_file(project_root, "base.txt"), "stashed\n")

    def read_object_body(self, project_root, sha1):
        object_path = self.git_dir(project_root) / "objects" / sha1[:2] / sha1[2:]
        compressed = object_path.read_bytes()
        import zlib

        full = zlib.decompress(compressed)
        return full.split(b"\0", 1)[1]

    def first_parent_chain(self, project_root, sha1):
        chain = []
        current = sha1
        while current:
            body = self.read_object_body(project_root, current).decode()
            parent = ""
            for line in body.splitlines():
                if line.startswith("parent "):
                    parent = line.split()[1]
                    break
            chain.append(current)
            current = parent
        return chain

    def test_python_cpp_parity_for_core_flow(self):
        if not PYTHON_REFERENCE.exists():
            self.skipTest("Python reference implementation is not present in the current worktree")

        temp_python, python_root = self.make_project()
        temp_cpp, cpp_root = self.make_project()
        self.addCleanup(temp_python.cleanup)
        self.addCleanup(temp_cpp.cleanup)

        env = {"MINI_GIT_FIXED_TIME": "1700000030"}
        self.run_python(python_root, "init", extra_env=env)
        self.run_cpp(cpp_root, "init", extra_env=env)

        self.write_working_file(python_root, "hello.txt", "hello\n")
        self.write_working_file(cpp_root, "hello.txt", "hello\n")

        self.assertEqual(
            self.run_python(python_root, "add", "hello.txt", extra_env=env),
            self.run_cpp(cpp_root, "add", "hello.txt", extra_env=env),
        )

        self.assertEqual(
            self.run_python(python_root, "commit", input_text="hello\n", extra_env=env),
            self.run_cpp(cpp_root, "commit", input_text="hello\n", extra_env=env),
        )

        self.assertEqual(
            self.run_python(python_root, "status", extra_env=env),
            self.run_cpp(cpp_root, "status", extra_env=env),
        )
        self.assertEqual(
            self.run_python(python_root, "log", extra_env=env),
            self.run_cpp(cpp_root, "log", extra_env=env),
        )
        self.assertEqual(
            self.run_python(python_root, "branch", "feature", extra_env=env),
            self.run_cpp(cpp_root, "branch", "feature", extra_env=env),
        )
        self.assertEqual(
            self.run_python(python_root, "checkout", "feature", extra_env=env),
            self.run_cpp(cpp_root, "checkout", "feature", extra_env=env),
        )

        self.assert_same_repo_state(python_root, cpp_root)

    def test_python_cpp_parity_for_stash_flow(self):
        if not PYTHON_REFERENCE.exists():
            self.skipTest("Python reference implementation is not present in the current worktree")

        temp_python, python_root = self.make_project()
        temp_cpp, cpp_root = self.make_project()
        self.addCleanup(temp_python.cleanup)
        self.addCleanup(temp_cpp.cleanup)

        env = {"MINI_GIT_FIXED_TIME": "1700000040"}
        self.run_python(python_root, "init", extra_env=env)
        self.run_cpp(cpp_root, "init", extra_env=env)

        self.write_working_file(python_root, "stash.txt", "base\n")
        self.write_working_file(cpp_root, "stash.txt", "base\n")
        self.run_python(python_root, "add", "stash.txt", extra_env=env)
        self.run_cpp(cpp_root, "add", "stash.txt", extra_env=env)
        self.run_python(python_root, "commit", input_text="base\n", extra_env=env)
        self.run_cpp(cpp_root, "commit", input_text="base\n", extra_env=env)

        self.write_working_file(python_root, "stash.txt", "work in progress\n")
        self.write_working_file(cpp_root, "stash.txt", "work in progress\n")
        self.run_python(python_root, "add", "stash.txt", extra_env=env)
        self.run_cpp(cpp_root, "add", "stash.txt", extra_env=env)

        self.assertEqual(
            self.run_python(python_root, "stash", "push", extra_env=env),
            self.run_cpp(cpp_root, "stash", "push", extra_env=env),
        )
        self.assertEqual(
            self.run_python(python_root, "stash", "list", extra_env=env),
            self.run_cpp(cpp_root, "stash", "list", extra_env=env),
        )
        self.assertEqual(
            self.run_python(python_root, "stash", "apply", extra_env=env),
            self.run_cpp(cpp_root, "stash", "apply", extra_env=env),
        )

        self.assert_same_repo_state(python_root, cpp_root)


if __name__ == "__main__":
    unittest.main()
