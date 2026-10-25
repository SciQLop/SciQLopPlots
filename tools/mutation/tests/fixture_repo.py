"""Throwaway git repos for the tool's tests."""
import subprocess

GIT = ["-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false", "-c", "core.hooksPath=/dev/null"]


def git(repo, *args):
    subprocess.run(["git", "-C", str(repo), *GIT, *args], check=True, capture_output=True)


def init_repo(path):
    """`path` becomes a git repo on branch main, with everything in it committed."""
    git(path, "init", "-q", "-b", "main")
    git(path, "add", ".")
    git(path, "commit", "-qm", "base")
    return path


def make_repo(path, files):
    path.mkdir(parents=True)
    for name, text in files.items():
        (path / name).write_text(text)
    return init_repo(path)
