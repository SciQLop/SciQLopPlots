"""Every public method, the receivers that can call it, and a working baseline call for each."""
from __future__ import annotations

import enum
import inspect
import json
import os
import tempfile
from dataclasses import dataclass

from tests.sweeps.api_fuzz.harness import Case, classify, run_batch

API_DIR = os.path.dirname(os.path.abspath(__file__))


@dataclass(frozen=True)
class Method:
    cls: str
    name: str
    signatures: tuple[str, ...]
    python_wrapper: bool


# Classes the sweep doesn't build, and why.
EXEMPT = {
    "date": "datetime re-export, not SciQLopPlots API",
    "datetime": "datetime re-export, not SciQLopPlots API",
    "timezone": "datetime re-export, not SciQLopPlots API",
    "QIntList": "binding container type for QList<int>",
    "MultiPlotsVerticalSpanPtr": "QPointer wrapper type the bindings return, not built by users",
    "SciQLopPlotInterfacePtr": "QPointer wrapper type the bindings return, not built by users",
    "SciQLopRangeItemInterface": "abstract base with no bound concrete subclass",
    "PlotsModelNode": "inspector tree node, only reached through the model's indexes",
    "SciQLopHistogramRemote": "no Python factory makes one",
}


def _signatures(fn) -> tuple[str, ...]:
    from shibokensupport.signature import get_signature
    try:
        sig = get_signature(fn)
    except Exception:
        sig = None
    if sig is None:
        try:
            sig = inspect.signature(fn)
        except (TypeError, ValueError):
            return ()
    return tuple(str(s) for s in (sig if isinstance(sig, list) else [sig]))


def _is_python(fn) -> bool:
    return inspect.isfunction(inspect.unwrap(fn))


def public_classes() -> dict[str, type]:
    import SciQLopPlots as S
    return {n: c for n, c in vars(S).items()
            if isinstance(c, type) and not n.startswith("_") and not issubclass(c, enum.Enum)}


def public_methods() -> list[Method]:
    from PySide6.QtCore import Signal
    out = []
    for cname, cls in sorted(public_classes().items()):
        if cname in EXEMPT or cls.__name__ != cname:  # exempt, or a kept-for-compatibility alias name
            continue
        for name, member in sorted(vars(cls).items()):
            if name.startswith("_") or isinstance(member, (Signal, type)) or not callable(member):
                continue
            if isinstance(member, (staticmethod, classmethod)):
                member = member.__func__
            out.append(Method(cname, name, _signatures(member), _is_python(member)))
    return out


def setup_source(kind: str) -> str:
    return (f"import sys; sys.path.insert(0, {API_DIR!r})\n"
            f"from child import *\n"
            f"h = Hosts()\n"
            f"obj = build({kind!r}, h)\n")


def call_source(kind: str, name: str, index: int) -> str:
    return f"args = candidate_args({kind!r}, obj, {name!r}, h)[{index}]\nobj.{name}(*args)\n"


def _run_writing(cases_and_paths, **kw):
    cases = [c for c, _ in cases_and_paths]
    verdicts = [classify(c, o) for c, o in zip(cases, run_batch(cases, **kw))]
    results = []
    for (case, path), verdict in zip(cases_and_paths, verdicts):
        data = None
        if os.path.exists(path):
            with open(path) as f:
                data = json.load(f)
        results.append((case, verdict, data))
    return results


def kinds() -> list[str]:
    """Every receiver kind child.py can build."""
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "kinds.json")
        call = f"import json, child\njson.dump(list(child.KINDS), open({path!r}, 'w'))\n"
        setup = f"import sys; sys.path.insert(0, {API_DIR!r})\n"
        [(case, verdict, data)] = _run_writing([(Case("kinds", setup, call, "ok"), path)])
        assert verdict.status == "pass", verdict
        return data


def receiver_mros(kinds: list[str]) -> dict[str, list[str] | None]:
    """The class names each receiver kind is an instance of (None if it failed to build)."""
    with tempfile.TemporaryDirectory() as tmp:
        jobs = []
        for kind in kinds:
            path = os.path.join(tmp, f"{kind}.json")
            call = f"import json\njson.dump(mro(obj), open({path!r}, 'w'))\n"
            jobs.append((Case(f"mro:{kind}", setup_source(kind), call, "ok"), path))
        return {case.id[4:]: data if verdict.status == "pass" else None
                for case, verdict, data in _run_writing(jobs)}


def receivers(mros: dict[str, list[str] | None]) -> dict[str, list[str]]:
    """Class name -> the receiver kinds that are instances of it."""
    out: dict[str, list[str]] = {}
    for kind, names in mros.items():
        for name in names or []:
            out.setdefault(name, []).append(kind)
    return out


def applicable(methods: list[Method], mro: list[str]) -> list[Method]:
    owned = set(mro)
    return [m for m in methods if m.cls in owned]


# Methods with no baseline call, and why. Their bad-argument cases are not generated.
NO_BASELINE = {
    ("PlotsModel", m): "needs a PlotsModelNode or an index into the inspector tree (see EXEMPT)"
    for m in ("addNode", "addTopLevelNode", "add_node", "canDropMimeData", "dropMimeData", "moveChildByObject",
              "object", "removeChildByObject", "set_selected")
}

# Calls that break the harness rather than the library (a modal loop, a thread move it can't undo), and Qt's
# event-handler overrides: Qt calls those with real events, users don't.
SKIP = {"moveToThread", "exec", "exec_", "eventFilter", "paintEvent", "dropEvent", "dragEnterEvent",
        "dragLeaveEvent", "dragMoveEvent", "keyPressEvent", "mousePressEvent"}


def discover(kind: str, methods: list[Method], **kw) -> list[dict]:
    """For each method callable on `kind`: the first candidate argument tuple that works, or why none does."""
    with tempfile.TemporaryDirectory() as tmp:
        jobs = []
        for m in methods:
            if m.name in SKIP:
                continue
            path = os.path.join(tmp, f"{m.cls}.{m.name}.json")
            call = f"find_baseline({kind!r}, {m.name!r}, h, obj, {path!r})\n"
            jobs.append((Case(f"discover:{kind}:{m.cls}.{m.name}", setup_source(kind), call, "ok"), path))
        out = []
        for (case, verdict, data), m in zip(_run_writing(jobs, **kw), [m for m in methods if m.name not in SKIP]):
            found = data["found"] if data and verdict.status == "pass" else None
            refused = data.get("refused") if data and verdict.status == "pass" else None
            out.append({"kind": kind, "cls": m.cls, "name": m.name, "found": found, "refused": refused,
                        "status": verdict.status, "detail": verdict.detail[-2000:]})
        return out


def discover_all(methods: list[Method], mros: dict[str, list[str] | None], jobs: int = 4) -> list[dict]:
    """discover() for every receiver kind, `jobs` children at a time."""
    from concurrent.futures import ThreadPoolExecutor
    work = [(kind, applicable(methods, mro)) for kind, mro in mros.items() if mro]
    with ThreadPoolExecutor(jobs) as pool:
        return [row for rows in pool.map(lambda kw: discover(*kw), work) for row in rows]
