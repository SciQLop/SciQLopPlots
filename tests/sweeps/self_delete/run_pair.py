"""run_pair.py KIND SETTER IDX PART SIGNAL TARGET: a slot on PART.SIGNAL deletes TARGET while SETTER runs."""
import sys

import shiboken6

import common as C


def resolve_part(obj, label):
    if label == "self":
        return obj
    return obj.components()[int(label[len("component["):-1])]


def main():
    kind, setter, idx, label, signal, target_name = sys.argv[1:7]
    h = C.Hosts()
    obj = C.BUILDERS[kind](h)
    C.pump()
    target = obj if target_name == "self" else h.owner
    args = C.candidates(kind, obj, setter, h)[int(idx)]
    fired = []

    def slot(*_):
        if not fired and shiboken6.isValid(target):
            fired.append(1)
            shiboken6.delete(target)

    getattr(resolve_part(obj, label), signal).connect(slot)
    try:
        getattr(obj, setter)(*args)
    except Exception as e:  # a just-deleted object may make the rest of the call raise
        print("RAISED", type(e).__name__, str(e)[:100], flush=True)
    C.pump(5)
    print("FIRED" if fired else "NOFIRE", flush=True)
    print("DONE", flush=True)


if __name__ == "__main__":
    main()
