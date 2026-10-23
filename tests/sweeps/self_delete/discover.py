"""discover.py KIND START OUT: for each mutator from index START, find working args and the signals it fires."""
import json
import sys

import common as C


def build(kind):
    h = C.Hosts()
    obj = C.BUILDERS[kind](h)
    C.pump()
    return h, obj


def wire(obj, fired):
    keep = []
    for label, part in C.parts(obj):
        for sig in C.signals(part):
            slot = (lambda l, s: lambda *a: fired.append((l, s)))(label, sig)
            try:
                getattr(part, sig).connect(slot)
                keep.append(slot)
            except Exception:
                pass
    return keep


def probe(kind, setter):
    h, obj = build(kind)
    fired = []
    keep = wire(obj, fired)
    errors = []
    for idx, args in enumerate(C.candidates(kind, obj, setter, h)):
        try:
            getattr(obj, setter)(*args)
        except NotImplementedError as e:
            return {"kind": kind, "setter": setter, "idx": None, "unsupported": str(e)[:120], "emitted": []}
        except Exception as e:
            errors.append(f"{type(e).__name__}: {str(e)[:80]}")
            continue
        fired_in_call = list(fired)
        C.pump()
        emitted = sorted(set(fired))
        return {"kind": kind, "setter": setter, "idx": idx, "args": repr(args)[:120],
                "emitted": emitted, "emitted_sync": sorted(set(fired_in_call))}
    return {"kind": kind, "setter": setter, "idx": None, "errors": [e for e in errors if not e.startswith("TypeError")][:3] or errors[-2:], "emitted": []}


def main():
    kind, start, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    _, obj = build(kind)
    setters = C.mutators(obj)
    with open(out, "a") as f:
        for i, s in enumerate(setters[start:], start):
            f.write(json.dumps({"kind": kind, "i": i, "setter": s, "status": "start"}) + "\n")
            f.flush()
            r = probe(kind, s)
            r.update(i=i, status="done", n=len(setters))
            f.write(json.dumps(r) + "\n")
            f.flush()
    print("ALLDONE", len(setters))


if __name__ == "__main__":
    main()
