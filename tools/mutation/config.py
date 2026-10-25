"""A phase config (TOML): what to mutate, and how to build, test and cover each project.

Everything project-specific is data in the TOML; this module only expands it.
Strings are templates: {root} {repo} {build_dir} {run_dir} {scratch} {test}
{test_class} {test_function} {env_<NAME>}. A list item that is exactly
"{tests}" becomes one item per test.
"""
import ast
import dataclasses
import glob
import os
import re
import subprocess
import tomllib
from dataclasses import dataclass
from pathlib import Path

import proc
from state import read_source


@dataclass(frozen=True)
class Target:
    path: str
    scope: str = "all"
    functions: tuple = ()
    deploy: str = ""
    objects: tuple = ()


@dataclass(frozen=True)
class Equivalent:
    path: str
    op: str
    before: str
    reason: str


def expand(value, variables):
    if isinstance(value, str):
        return value.format_map(variables)
    if isinstance(value, dict):
        return {key: expand(item, variables) for key, item in value.items()}
    return [arg for item in value for arg in (variables["tests"] if item == "{tests}" else [expand(item, variables)])]


@dataclass
class Project:
    name: str
    root: Path
    repo: Path
    build_dir: Path
    raw: dict
    targets: tuple = ()
    equivalents: tuple = ()

    @property
    def run_dir(self):
        return self.build_dir / "mutation"

    @property
    def state_path(self):
        return self.run_dir / "state.json"

    @property
    def scratch(self):
        return self.run_dir / "scratch"

    @property
    def group_file(self):
        return self.run_dir / "child.pgid"

    def variables(self, tests=()):
        tests = list(tests)
        single = tests[0] if len(tests) == 1 else ""
        test_class, _, test_function = single.partition("::")
        base = {
            **{f"env_{key}": value for key, value in os.environ.items()},
            "root": str(self.root), "repo": str(self.repo), "build_dir": str(self.build_dir),
            "run_dir": str(self.run_dir), "scratch": str(self.scratch),
            "test": single, "test_class": test_class, "test_function": test_function,
        }
        prefix = self.raw.get("tests", {}).get("prefix", "").format_map(base)
        return {**base, "tests": [prefix + t for t in tests]}

    def expand(self, value, tests=()):
        return expand(value, self.variables(tests))

    def env(self, tests=(), mutation=False):
        junk = {"GCOV_PREFIX": str(self.run_dir / "gcda-junk")} if mutation else {}
        return {**os.environ, **self.expand(self.raw.get("env", {}), tests),
                **self.expand(self.raw.get("test_env", {}), tests), **junk}

    def command(self, key, tests=()):
        return self.expand(self.raw[key], tests)

    def cwd(self):
        return self.expand(self.raw.get("test_cwd", "{root}"))

    def target(self, path):
        return next(t for t in self.targets if t.path == path)

    def deploy_path(self, target):
        return Path(target.deploy) if target.deploy else self.repo / target.path

    def run(self, cmd, *, cwd=None, env=None, timeout):
        self.run_dir.mkdir(parents=True, exist_ok=True)
        return proc.run(cmd, cwd=cwd, env=env, timeout=timeout, group_file=self.group_file)

    def build(self):
        return self.run(self.command("build_cmd"), cwd=str(self.root), env=self.env(),
                        timeout=self.raw.get("build_timeout", 900))


def load(config_path, name):
    config_path = Path(config_path).resolve()
    data = tomllib.loads(config_path.read_text(encoding="utf-8"))
    projects = {p["name"]: p for p in data["project"]}
    if name not in projects:
        raise SystemExit(f"no project {name!r} in {config_path}; have {sorted(projects)}")
    raw = projects[name]
    root = (config_path.parent / data.get("root", ".")).resolve()
    project = Project(name, root, (root / raw["repo"]).resolve(), (root / raw["build_dir"]).resolve(), raw)
    targets = tuple(Target(t["path"], t.get("scope", "all"), tuple(t.get("functions", ())),
                           project.expand(t.get("deploy", "")), tuple(t.get("objects", ())))
                    for t in raw["target"])
    equivalents = tuple(Equivalent(e["path"], e["op"], e["before"], e["reason"]) for e in raw.get("equivalent", ()))
    return dataclasses.replace(project, targets=targets, equivalents=equivalents)


def changed_lines(repo, base, path):
    diff = subprocess.run(["git", "-C", str(repo), "diff", "-U0", f"{base}...HEAD", "--", path],
                          capture_output=True, text=True, check=True).stdout
    hunks = re.finditer(r"^@@ -\S+ \+(\d+)(?:,(\d+))? @@", diff, re.M)
    return {n for h in hunks for n in range(int(h[1]), int(h[1]) + int(h[2] or 1))}


def function_lines(text, names):
    return {n for node in ast.walk(ast.parse(text))
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names
            for n in range(node.lineno, node.end_lineno + 1)}


def target_lines(project, target):
    text = read_source(project.repo / target.path)
    lines = set(range(1, len(text.splitlines()) + 1)) if target.scope == "all" else changed_lines(project.repo, project.raw["base"], target.path)
    return lines & function_lines(text, set(target.functions)) if target.functions else lines


def discover_classes(project):
    spec = project.raw["tests"]
    return re.findall(spec["pattern"], read_source(project.root / spec["regex_file"]))


def _functions(project, spec, cls):
    env = {**project.env([cls]), **project.expand(spec.get("functions_env", {}), [cls])}
    result = project.run(project.expand(spec["functions_cmd"], [cls]), cwd=project.cwd(), env=env, timeout=120)
    names = re.findall(spec["functions_pattern"], result.output, re.M)
    if result.code != 0 or not names:
        raise SystemExit(f"listing the test functions of {cls} failed (exit {result.code}):\n{result.output[-1000:]}")
    return names


def discover_tests(project):
    spec = project.raw["tests"]
    if "glob" in spec:
        found = {Path(p).relative_to(project.root).as_posix() for pattern in spec["glob"] for p in glob.glob(str(project.root / pattern))}
        return sorted(found)
    classes = discover_classes(project)
    if "functions_cmd" not in spec:
        return classes
    return [f"{cls}::{fn}" for cls in classes for fn in _functions(project, spec, cls)]
