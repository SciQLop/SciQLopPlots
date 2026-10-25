"""The per-run report: survivors to triage, uncovered lines, kill-matrix summary."""
import itertools
import json
from collections import Counter
from pathlib import Path

STATUSES = ("pending", "killed", "timeout", "survived", "build_error", "equivalent", "interrupted")
TRIAGE = "real bug / weak test / dead code / equivalent"
FAIL_FAST = ("These runs stop at the first kill: killed_by names the fastest killing test only "
             "(for SciQLopPlots, the fastest test file), so the two lists below are only hints.")


def ranges(numbers):
    runs = itertools.groupby(enumerate(sorted(numbers)), key=lambda pair: pair[1] - pair[0])
    spans = [[n for _, n in run] for _, run in runs]
    return ", ".join(str(s[0]) if len(s) == 1 else f"{s[0]}-{s[-1]}" for s in spans)


def cell(text):
    # why replaced, not escaped: a backslash does not escape inside a Markdown code span
    return "`" + text.strip().replace("`", "'").replace("|", "\\|") + "`"


def kill_matrix(data):
    matrix = {}
    for mid, m in sorted(data["mutants"].items()):
        for killer in m.get("killed_by", []):
            matrix.setdefault(killer, []).append(mid)
    return matrix


def _belongs(killer, test):
    class_level_killer = "::" not in killer and test.startswith(killer + "::")
    return killer == test or killer.startswith(test + "::") or class_level_killer


def redundancy(data):
    matrix = kill_matrix(data)
    ran = {t for m in data["mutants"].values() if m["status"] in ("killed", "survived", "timeout")
           for t in m["tests"][: m.get("ran", len(m["tests"]))]}
    killing = {t for t in ran if any(_belongs(k, t) for k in matrix)}
    sole = {m["killed_by"][0] for m in data["mutants"].values() if len(m.get("killed_by", [])) == 1}
    return sorted(ran - killing), sorted(k for k in matrix if k not in sole)


def _survivor_rows(data):
    survivors = sorted((m["path"], m["line"], mid, m) for mid, m in data["mutants"].items() if m["status"] == "survived")
    return [f"| {mid} | {m['path']}:{m['line']} | {m['op']} | {cell(m['before'])} | {cell(m['after'])} "
            f"| {m.get('ran', len(m['tests']))}/{m.get('covering', len(m['tests']))} |  |"
            for _, _, mid, m in survivors]


def _bullets(items):
    return [f"- {item}" for item in items] or ["- none"]


def render(name, data, fail_fast=False):
    counts = Counter(m["status"] for m in data["mutants"].values())
    decided = counts["killed"] + counts["timeout"] + counts["survived"]
    score = f"{100 * (counts['killed'] + counts['timeout']) / decided:.1f}%" if decided else "n/a"
    never, no_unique = redundancy(data)
    uncovered = [f"{path}: {ranges(lines)}" for path, lines in sorted(data["uncovered"].items()) if lines]
    failing = [f"{n}: {e.get('note') or e.get('status')}" for n, e in sorted(data["tests"].items()) if e.get("status") != "passed"]
    unmeasured = list(data.get("not_measured", []))
    stale = [f"{e['path']} {e['op']} {cell(e['before'])}: {e['reason']}" for e in data.get("stale_equivalents", [])]
    return "\n".join([
        f"# Mutation report: {name}", "",
        "| status | count |", "|---|---|", *(f"| {s} | {counts[s]} |" for s in STATUSES), "",
        f"Mutation score (killed + timeout over decided): {score}", "",
        "## Survivors", "",
        f"One row, one follow-up. Fill the triage column: {TRIAGE}. "
        "'tests run' is ran/covering: every covering test ran against each survivor.", "",
        "| id | file:line | operator | before | after | tests run | triage |",
        "|---|---|---|---|---|---|---|", *_survivor_rows(data), "",
        "## Uncovered target lines", "", "A missing test or dead code.", "", *_bullets(uncovered), "",
        "## Tests without a passing baseline", "", "Failed, or not run yet: left out of every mutant run.", "", *_bullets(failing), "",
        "## Targets without coverage", "", "No coverage data for these files, nothing was planned for them.", "",
        *_bullets(unmeasured), "",
        "## Stale equivalents", "", "Entries in the config that match no planned mutant: re-check or delete them.", "",
        *_bullets(stale), "",
        "## Kill matrix", "", "The full matrix is the -kill-matrix.json file next to this report.", "",
        *([FAIL_FAST, ""] if fail_fast else []),
        "Ran against mutants and killed none (removal candidates if they cover nothing unique):", "",
        *_bullets(never), "",
        "Killed only mutants that other tests killed too:", "", *_bullets(no_unique), "",
    ])


def write(project, data, out=None):
    out = Path(out) if out else project.run_dir / "report.md"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(render(project.name, data, bool(project.raw.get("stop_on_first_kill"))), encoding="utf-8")
    out.with_name(out.stem + "-kill-matrix.json").write_text(json.dumps(kill_matrix(data), indent=1), encoding="utf-8")
    return out
