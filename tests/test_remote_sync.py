import os
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlparse


ROOT = Path(__file__).resolve().parents[1]
CPP_BIN = ROOT / "mini_git"
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


def parse_snapshot(payload):
    lines = payload.splitlines()
    if not lines or lines[0] != "MINIGIT_REMOTE_V1":
        raise ValueError("invalid snapshot header")

    snapshot = {
        "current_branch": "",
        "stash": "",
        "refs": [],
        "objects": [],
    }

    for line in lines[1:]:
        if line == "END":
            return snapshot
        if not line:
            continue

        kind, rest = line.split(" ", 1)
        if kind == "CURRENT_BRANCH":
            snapshot["current_branch"] = "" if rest == "-" else rest
        elif kind == "STASH":
            snapshot["stash"] = "" if rest == "-" else rest
        elif kind == "REF":
            name, value = rest.split(" ", 1)
            snapshot["refs"].append((name, "" if value == "-" else value))
        elif kind == "OBJECT":
            path, value = rest.split(" ", 1)
            snapshot["objects"].append((path, value))
        else:
            raise ValueError(f"unknown record type: {kind}")

    raise ValueError("snapshot missing END")


def serialize_snapshot(snapshot):
    lines = [
        "MINIGIT_REMOTE_V1",
        f"CURRENT_BRANCH {snapshot['current_branch'] or '-'}",
        f"STASH {snapshot['stash'] or '-'}",
    ]

    for name, value in sorted(snapshot["refs"]):
        lines.append(f"REF {name} {value or '-'}")

    for path, value in sorted(snapshot["objects"]):
        lines.append(f"OBJECT {path} {value}")

    lines.append("END")
    return "\n".join(lines) + "\n"


class RemoteSyncTests(unittest.TestCase):
    def make_project(self):
        temp_dir = tempfile.TemporaryDirectory()
        project_root = Path(temp_dir.name) / "project"
        (project_root / "constants").mkdir(parents=True, exist_ok=True)
        constants_source = ROOT / "constants" / "constants"
        if constants_source.exists():
            shutil.copy2(constants_source, project_root / "constants" / "constants")
        else:
            (project_root / "constants" / "constants").write_text(DEFAULT_CONSTANTS)
        return temp_dir, project_root

    def run_cpp(self, project_root, *args, input_text=None, extra_env=None):
        env = os.environ.copy()
        if extra_env:
            env.update(extra_env)

        result = subprocess.run(
            [str(CPP_BIN), *args],
            cwd=project_root,
            text=True,
            input=input_text,
            capture_output=True,
            env=env,
            check=True,
        )
        self.assertEqual(result.stderr, "")
        return result.stdout

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

    def read_git_config(self, project_root):
        return (self.git_dir(project_root) / "config").read_text()

    def list_object_files(self, project_root):
        base = self.git_dir(project_root) / "objects"
        if not base.exists():
            return []
        return sorted(
            path.relative_to(base).as_posix()
            for path in base.rglob("*")
            if path.is_file()
        )

    def start_mock_server(self):
        storage_dir = tempfile.TemporaryDirectory()
        storage_root = Path(storage_dir.name)

        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                parsed = urlparse(self.path)
                parts = [part for part in parsed.path.split("/") if part]
                if len(parts) != 4 or parts[:3] != ["api", "repos", parts[2]] or parts[3] != "push":
                    self.send_error(404, "Not Found")
                    return

                repo = unquote(parts[2])
                length = int(self.headers.get("Content-Length", "0"))
                payload = self.rfile.read(length).decode()
                snapshot = parse_snapshot(payload)
                repo_dir = storage_root / repo
                (repo_dir / "refs" / "heads").mkdir(parents=True, exist_ok=True)
                (repo_dir / "objects").mkdir(parents=True, exist_ok=True)

                (repo_dir / "CURRENT_BRANCH").write_text(snapshot["current_branch"])
                stash_path = repo_dir / "stash"
                if snapshot["stash"]:
                    stash_path.write_text(snapshot["stash"])
                elif stash_path.exists():
                    stash_path.unlink()

                for branch, commit in snapshot["refs"]:
                    (repo_dir / "refs" / "heads" / branch).write_text(commit)

                for relative_path, hex_content in snapshot["objects"]:
                    object_path = repo_dir / "objects" / relative_path
                    object_path.parent.mkdir(parents=True, exist_ok=True)
                    object_path.write_bytes(bytes.fromhex(hex_content))

                body = b"OK\n"
                self.send_response(200)
                self.send_header("Content-Type", "text/plain")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def do_GET(self):
                parsed = urlparse(self.path)
                parts = [part for part in parsed.path.split("/") if part]
                if len(parts) != 4 or parts[:3] != ["api", "repos", parts[2]] or parts[3] != "pull":
                    self.send_error(404, "Not Found")
                    return

                repo = unquote(parts[2])
                repo_dir = storage_root / repo
                if not repo_dir.exists():
                    self.send_error(404, "Repository not found")
                    return

                snapshot = {
                    "current_branch": (repo_dir / "CURRENT_BRANCH").read_text().strip() if (repo_dir / "CURRENT_BRANCH").exists() else "",
                    "stash": (repo_dir / "stash").read_text().strip() if (repo_dir / "stash").exists() else "",
                    "refs": [],
                    "objects": [],
                }

                refs_root = repo_dir / "refs" / "heads"
                if refs_root.exists():
                    for path in refs_root.iterdir():
                        if path.is_file():
                            snapshot["refs"].append((path.name, path.read_text().strip()))

                objects_root = repo_dir / "objects"
                if objects_root.exists():
                    for path in objects_root.rglob("*"):
                        if path.is_file():
                            snapshot["objects"].append((path.relative_to(objects_root).as_posix(), path.read_bytes().hex()))

                body = serialize_snapshot(snapshot).encode()
                self.send_response(200)
                self.send_header("Content-Type", "text/plain")
                self.send_header("Content-Length", str(len(body)))
                self.end_headers()
                self.wfile.write(body)

            def log_message(self, format, *args):
                return

        server = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()

        self.addCleanup(server.shutdown)
        self.addCleanup(server.server_close)
        self.addCleanup(thread.join, 1)
        self.addCleanup(storage_dir.cleanup)

        return f"http://127.0.0.1:{server.server_port}"

    def test_push_then_pull_restores_main_branch(self):
        sender_temp, sender_root = self.make_project()
        receiver_temp, receiver_root = self.make_project()
        self.addCleanup(sender_temp.cleanup)
        self.addCleanup(receiver_temp.cleanup)

        self.run_cpp(sender_root, "init")
        self.write_working_file(sender_root, "hello.txt", "from sender\n")
        self.run_cpp(sender_root, "add", "hello.txt")
        self.run_cpp(
            sender_root,
            "commit",
            input_text="sender commit\n",
            extra_env={"MINI_GIT_FIXED_TIME": "1700000100"},
        )

        server_url = self.start_mock_server()
        add_remote_output = self.run_cpp(sender_root, "remote", "add", "origin", server_url, "demo")
        self.assertEqual(add_remote_output, "Added remote origin\n")
        self.assertEqual(self.run_cpp(sender_root, "remote"), "origin\n")
        self.assertEqual(
            self.run_cpp(sender_root, "remote", "show", "origin"),
            f"name origin\nurl {server_url}\nrepo demo\n",
        )
        self.assertIn('[remote "origin"]', self.read_git_config(sender_root))

        self.run_cpp(receiver_root, "remote", "add", "origin", server_url, "demo")

        push_output = self.run_cpp(sender_root, "push", "origin")
        self.assertIn("Pushed", push_output)

        pull_output = self.run_cpp(receiver_root, "pull", "origin")
        self.assertIn("Pulled", pull_output)
        self.assertEqual(self.read_working_file(receiver_root, "hello.txt"), "from sender\n")
        self.assertEqual(self.read_ref(receiver_root), self.read_ref(sender_root))
        self.assertEqual(self.list_object_files(receiver_root), self.list_object_files(sender_root))
        self.assertEqual((self.git_dir(receiver_root) / "HEAD").read_text(), "ref: refs/heads/main\n")

    def test_pull_uses_remote_current_branch_and_restores_feature_state(self):
        sender_temp, sender_root = self.make_project()
        receiver_temp, receiver_root = self.make_project()
        self.addCleanup(sender_temp.cleanup)
        self.addCleanup(receiver_temp.cleanup)

        self.run_cpp(sender_root, "init")
        self.write_working_file(sender_root, "hello.txt", "main\n")
        self.run_cpp(sender_root, "add", "hello.txt")
        self.run_cpp(
            sender_root,
            "commit",
            input_text="main commit\n",
            extra_env={"MINI_GIT_FIXED_TIME": "1700000110"},
        )
        self.run_cpp(sender_root, "branch", "feature")
        self.run_cpp(sender_root, "checkout", "feature")
        self.write_working_file(sender_root, "hello.txt", "feature\n")
        self.run_cpp(sender_root, "add", "hello.txt")
        self.run_cpp(
            sender_root,
            "commit",
            input_text="feature commit\n",
            extra_env={"MINI_GIT_FIXED_TIME": "1700000111"},
        )

        server_url = self.start_mock_server()
        self.run_cpp(sender_root, "remote", "add", "origin", server_url, "demo")
        self.run_cpp(receiver_root, "remote", "add", "origin", server_url, "demo")
        self.run_cpp(sender_root, "push", "origin")
        self.run_cpp(receiver_root, "pull", "origin")

        self.assertEqual((self.git_dir(receiver_root) / "HEAD").read_text(), "ref: refs/heads/feature\n")
        self.assertEqual(self.read_working_file(receiver_root, "hello.txt"), "feature\n")
        self.assertEqual(self.read_ref(receiver_root, "feature"), self.read_ref(sender_root, "feature"))


if __name__ == "__main__":
    unittest.main()
