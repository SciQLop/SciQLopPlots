"""Run state: one JSON file per project, saved atomically after every step.

`in_flight` names the file a mutant changed, its backup and the original's
sha256. It is saved before the mutant is written, so a run killed by SIGKILL,
a crash or the shell's timeout leaves enough to put the original back: every
command starts with restore_leftovers().
"""
import hashlib
import json
import os
import shutil
from contextlib import contextmanager
from pathlib import Path


VERSION = 1


def read_source(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def write_source(path, text):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def new_state():
    return {"version": VERSION, "tests": {}, "mutants": {}, "files": {}, "uncovered": {}, "stale_equivalents": [], "in_flight": []}


def load(path):
    path = Path(path)
    if not path.exists():
        return new_state()
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("version") != VERSION:
        raise ValueError(f"{path}: state version {data.get('version')!r}, expected {VERSION}")
    return {**new_state(), **data}


def save(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(json.dumps({**data, "version": VERSION}, indent=1, sort_keys=True))
        f.flush()
        # the state is hours of mutant runs: it must survive a power loss, not only a crash
        os.fsync(f.fileno())
    os.replace(tmp, path)
    _fsync_dir(path.parent)


def _fsync_dir(directory):
    fd = os.open(directory, os.O_DIRECTORY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)


def restore_leftovers(state_path, data):
    restored = []
    for entry in data["in_flight"]:
        # copyfile, not copy2: the restored file must look newer than the object
        # built from the mutant, so the next ninja run rebuilds it.
        shutil.copyfile(entry["backup"], entry["path"])
        if sha(entry["path"]) != entry["sha"]:
            raise RuntimeError(f"restored {entry['path']} does not match its original sha256")
        restored.append(entry["path"])
    data["in_flight"] = []
    save(state_path, data)
    return restored


@contextmanager
def mutated(state_path, data, path, text, backup_dir):
    if data["in_flight"]:
        raise RuntimeError(f"leftover in flight, restore it first: {data['in_flight']}")
    path = Path(path)
    backup = Path(backup_dir) / (path.name + ".orig")
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, backup)
    data["in_flight"] = [{"path": str(path), "backup": str(backup), "sha": sha(path)}]
    save(state_path, data)
    try:
        write_source(path, text)
        yield
    finally:
        restore_leftovers(state_path, data)
