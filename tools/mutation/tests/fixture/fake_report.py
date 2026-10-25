"""Stands in for gcovr: test 'a' ran lines 1-2 of a.cpp, test 'b' ran line 3; 1-4 are code."""
import json
import sys

scratch = sys.argv[1]
ran = open(f"{scratch}/ran").read().strip()
covered = {"a": [1, 2], "b": [3]}.get(ran, [])
lines = [{"line_number": n, "count": int(n in covered)} for n in (1, 2, 3, 4)]
lines.append({"line_number": 5, "count": 0, "gcovr/noncode": True})
json.dump({"files": [{"file": "a.cpp", "lines": lines}]}, open(f"{scratch}/gcovr.json", "w"))
