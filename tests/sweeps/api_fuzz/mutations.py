"""Bad-argument cases built from a working baseline call (a discovery row from catalogue.discover)."""
from __future__ import annotations

from dataclasses import dataclass

from tests.sweeps.api_fuzz.catalogue import call_source, setup_source
from tests.sweeps.api_fuzz.harness import Case

NUMBERS = frozenset({"number"})
BUFFERS = frozenset({"buffer"})
# bool parameters take any object by truthiness (PySide's convention): they get no wrong-type values.
# A QFont parameter takes a str or a sequence of family names (PySide builds the font): only
# non-iterables are wrong types for it.
NOT_STR = frozenset({"number", "buffer", "sequence", "mapping", "qobject", "deletable", "value", "font"})
OBJECTS = frozenset({"qobject", "deletable"})
SEQUENCES = frozenset({"sequence"})
ITERABLES = frozenset({"sequence", "mapping"})  # a dict iterates: not a wrong type for these
# A float for an int parameter is a wrong type: numbers may be refused as TypeError too.
NUMBER_REFUSALS = ("TypeError",)


@dataclass(frozen=True)
class BadValue:
    name: str
    source: str  # evaluated in the child, where np, X and N are in scope
    applies_to: frozenset  # argument kinds from child.arg_kind
    expect: str
    allow: tuple[str, ...] = ()


BAD_VALUES = [
    # Wrong types: always TypeError.
    BadValue("object", "object()", NOT_STR | {"str"}, "type"),
    # Positions and ranges also take date strings: an unparsable one is a bad value.
    BadValue("str", "'not a number'", NOT_STR - ITERABLES - {"font"}, "type", ("ValueError",)),
    BadValue("dict", "{'a': 1}", (NOT_STR - ITERABLES - {"font"}) | {"str"}, "type"),
    BadValue("list_for_scalar", "[1.0]", frozenset({"number", "str", "value"}), "type"),
    BadValue("list_of_numbers_for_font", "[1.0]", frozenset({"font"}), "type"),
    BadValue("none", "None", NOT_STR - OBJECTS, "type"),
    # None where an object is expected may mean "no object": accepted, or refused as a type or value.
    BadValue("none_object", "None", OBJECTS, "ok_or_value", ("TypeError",)),
    BadValue("none_str", "None", frozenset({"str"}), "ok_or_value", ("TypeError",)),
    # Numbers: accepted, or refused as a bad value.
    BadValue("nan", "float('nan')", NUMBERS, "ok_or_value", NUMBER_REFUSALS),
    BadValue("inf", "float('inf')", NUMBERS, "ok_or_value", NUMBER_REFUSALS),
    BadValue("minus_inf", "float('-inf')", NUMBERS, "ok_or_value", NUMBER_REFUSALS),
    BadValue("zero", "0", NUMBERS, "ok_or_value", NUMBER_REFUSALS),
    BadValue("minus_one", "-1", NUMBERS, "ok_or_value", NUMBER_REFUSALS),
    BadValue("huge_int", "2**63", NUMBERS, "ok_or_value", NUMBER_REFUSALS),
    BadValue("huge_float", "1e308", NUMBERS, "ok_or_value", NUMBER_REFUSALS),
    # Buffers: accepted, or refused; a dtype the library can't use is a wrong type.
    BadValue("empty", "np.array([], dtype=np.float64)", BUFFERS, "ok_or_value"),
    BadValue("zero_d", "np.array(1.0)", BUFFERS, "ok_or_value", ("TypeError",)),
    BadValue("three_d", "np.zeros((2, 2, 2))", BUFFERS, "ok_or_value", ("TypeError",)),
    BadValue("complex", "np.zeros(N, dtype=complex)", BUFFERS, "ok_or_value", ("TypeError",)),
    BadValue("bool_dtype", "np.zeros(N, dtype=bool)", BUFFERS, "ok_or_value", ("TypeError",)),
    BadValue("int8", "np.arange(N, dtype=np.int8)", BUFFERS, "ok_or_value", ("TypeError",)),
    BadValue("object_dtype", "np.array([1.0] * N, dtype=object)", BUFFERS, "ok_or_value", ("TypeError",)),
    BadValue("strided", "np.linspace(0.0, 1.0, 2 * N)[::2]", BUFFERS, "ok_or_value", ("TypeError",)),
    BadValue("big_endian", "np.linspace(0.0, 1.0, N).astype('>f8')", BUFFERS, "ok_or_value", ("TypeError",)),
    BadValue("read_only", "_read_only(np.linspace(0.0, 1.0, N))", BUFFERS, "ok_or_value", ("TypeError",)),
    BadValue("nan_buffer", "np.full(N, np.nan)", BUFFERS, "ok_or_value"),
    # Sequences.
    BadValue("empty_list", "[]", SEQUENCES, "ok_or_value"),
    BadValue("list_of_none", "[None]", SEQUENCES, "ok_or_value", ("TypeError",)),
]

# None that a method takes on purpose, by (method, argument position): its meaning, not a mistake.
NONE_ACCEPTED = {
    ("set_color_data", 0): "None clears the colouring",
    ("set_data_and_color", 1): "None colour: the data alone, no colour axis",
    ("set_data_colored", 1): "None colour: the data alone, no colour axis",
    ("set_callable", 0): "None removes the callback: the graph stops fetching",
    ("index", 0): "a plot the panel doesn't hold, None included, is at -1",
}

# Calls whose remaining arguments may match another overload or a default: they must not crash or swallow,
# and a refusal must be a type or value error (or the class's own "not supported").
_COUNT_ALLOW = ("TypeError", "NotImplementedError")

PREAMBLE = "import numpy as np\nimport shiboken6\n\n" \
           "def _read_only(a):\n    a.flags.writeable = False\n    return a\n\n"


def _base_args(row: dict) -> str:
    return f"args = list(candidate_args({row['kind']!r}, obj, {row['name']!r}, h)[{row['found']['index']}])\n"


def _case(row: dict, tag: str, call: str, expect: str, allow=()) -> Case:
    return Case(f"{row['kind']}:{row['cls']}.{row['name']}:{tag}", setup_source(row["kind"]),
                PREAMBLE + call, expect, tuple(allow))


def _invoke(row: dict) -> str:
    return f"obj.{row['name']}(*args)\n"


def cases_for(row: dict, receiver_deletable: bool) -> list[Case]:
    """The baseline call, then one case per (argument, bad value), wrong counts and deleted objects."""
    found = row["found"]
    base = _base_args(row)
    out = [_case(row, "baseline", call_source(row["kind"], row["name"], found["index"]), "ok")]
    kinds = found["arg_kinds"]
    for i, kind in enumerate(kinds):
        for bad in BAD_VALUES:
            if kind in bad.applies_to:
                expect, allow = bad.expect, bad.allow
                if bad.source == "None" and (row["name"], i) in NONE_ACCEPTED:
                    expect, allow = "ok_or_value", ("TypeError",)
                out.append(_case(row, f"{bad.name}@{i}", base + f"args[{i}] = {bad.source}\n" + _invoke(row),
                                 expect, allow))
        if kind == "deletable":
            out.append(_case(row, f"deleted@{i}", base + f"shiboken6.delete(args[{i}])\n" + _invoke(row), "deleted"))
    buffers = [i for i, k in enumerate(kinds) if k == "buffer"]
    if len(buffers) >= 2:
        i = buffers[0]
        out.append(_case(row, f"shorter@{i}", base + f"args[{i}] = args[{i}][:-1]\n" + _invoke(row), "value"))
    if kinds:
        out.append(_case(row, "too_few", base + "args = args[:-1]\n" + _invoke(row), "ok_or_value", _COUNT_ALLOW))
    out.append(_case(row, "too_many", base + "args = args + [object()]\n" + _invoke(row), "ok_or_value", _COUNT_ALLOW))
    if receiver_deletable:
        out.append(_case(row, "deleted_receiver", base + "shiboken6.delete(obj)\n" + _invoke(row), "deleted"))
    return out
