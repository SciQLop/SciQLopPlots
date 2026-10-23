"""control.py KIND SETTER IDX: delete the object right after the setter returns (no slot)."""
import sys
import shiboken6
import common as C

kind, setter, idx = sys.argv[1], sys.argv[2], int(sys.argv[3])
h = C.Hosts()
obj = C.BUILDERS[kind](h)
C.pump()
getattr(obj, setter)(*C.candidates(kind, obj, setter, h)[idx])
shiboken6.delete(obj)
C.pump(5)
print("DONE", flush=True)
