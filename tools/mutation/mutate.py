"""Mutation testing for SciQLopPlots and NeoQCP (docs/plans/2026-10-22-mutation-testing-design.md).

    python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project neoqcp <command>

Commands, in order:
    cover [--reset] [--budget S]   per-test coverage of the targets, and the baseline
    plan                           mutants on the covered target lines
    run [--budget S]               apply, build, test, restore; resumable
    report [--out FILE]            survivors to triage, uncovered lines, kill matrix
    restore                        kill and put back what a killed run left, then rebuild
    try --path P --line N --op OP --tests T [T ...]   one mutant of the current source

Exit codes: 0 done; 10 work left, run the same command again; 3 interrupted,
sources restored, run again; 4 another command holds this build dir's lock.
"""
import argparse
import fcntl
import os
import sys
from contextlib import contextmanager

import config
import covermap
import proc
import report
import runner
import state as st

MORE, INTERRUPTED, LOCKED = 10, 3, 4
DEFAULT_BUDGET = 520  # under `timeout -k 20 570`, which stays under the 600 s shell limit


@contextmanager
def build_dir_lock(project):
    project.run_dir.mkdir(parents=True, exist_ok=True)
    with open(project.run_dir / "lock", "w") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print(f"another mutation run holds {project.build_dir}", file=sys.stderr)
            sys.exit(LOCKED)
        yield


def _left(count, what):
    print(f"{what}: {count} left" + (", run the same command again" if count else ", done"))
    return MORE if count else 0


def _plan(project, args):
    data = runner.plan(project)
    for target in project.targets:
        mine = [m for m in data["mutants"].values() if m["path"] == target.path]
        print(f"{target.path}: {len(mine)} mutants, {len(data['uncovered'].get(target.path, []))} uncovered code lines")
    failing = sorted(f"{n} ({e.get('note') or e['status']})" for n, e in data["tests"].items() if e["status"] != "passed")
    print(f"baseline failures, left out of every mutant run: {failing or 'none'}")
    for e in data["stale_equivalents"]:
        print(f"warning: equivalent entry matches no mutant: {e['path']} {e['op']} {e['before'].strip()!r}")
    return 0


def _report(project, args):
    print(report.write(project, st.load(project.state_path), args.out))
    return 0


def _restore(project, args):
    data = st.load(project.state_path)
    print("restored:", covermap.recover(project, data) or "nothing")
    covermap.build_or_die(project)
    return 0


def _try(project, args):
    result = runner.try_mutant(project, args.path, args.line, args.op, args.tests)
    print(f"{result['status']}: {', '.join(result.get('killed_by', [])) or 'no test failed'}")
    return 0


COMMANDS = {
    "cover": lambda project, args: _left(covermap.cover(project, args.budget, reset=args.reset), "cover"),
    "plan": _plan,
    "run": lambda project, args: _left(runner.run(project, args.budget), "run"),
    "report": _report,
    "restore": _restore,
    "try": _try,
}


def _parser():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    parser.add_argument("--project", required=True)
    commands = parser.add_subparsers(dest="command", required=True)
    cover = commands.add_parser("cover")
    cover.add_argument("--reset", action="store_true")
    cover.add_argument("--budget", type=float, default=DEFAULT_BUDGET)
    commands.add_parser("plan")
    run = commands.add_parser("run")
    run.add_argument("--budget", type=float, default=DEFAULT_BUDGET)
    report_cmd = commands.add_parser("report")
    report_cmd.add_argument("--out")
    commands.add_parser("restore")
    trial = commands.add_parser("try")
    trial.add_argument("--path", required=True)
    trial.add_argument("--line", type=int, required=True)
    trial.add_argument("--op", required=True)
    trial.add_argument("--tests", nargs="+", required=True)
    return parser


def _require_env(project):
    missing = [name for name in project.raw.get("require_env", []) if name not in os.environ]
    if missing:
        sys.exit(f"{project.name} needs {', '.join(missing)}: run the command under xvfb-run -a")


def main(argv=None):
    args = _parser().parse_args(argv)
    project = config.load(args.config, args.project)
    _require_env(project)
    proc.install_signal_handlers()
    with build_dir_lock(project):
        try:
            return COMMANDS[args.command](project, args)
        except proc.Interrupted as why:
            print(f"interrupted ({why}); sources restored, run the same command again", file=sys.stderr)
            return INTERRUPTED


if __name__ == "__main__":
    sys.exit(main())
