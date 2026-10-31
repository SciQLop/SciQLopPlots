"""Every keyword argument is snake_case, like `metadata=` on `plot()`."""
import inspect
import re

from shibokensupport.signature import get_signature

import SciQLopPlots.SciQLopPlotsBindings as bindings


def _camel_case_keywords():
    found = set()
    for class_name, cls in vars(bindings).items():
        if not isinstance(cls, type):
            continue
        for name, member in vars(cls).items():
            signatures = get_signature(inspect.unwrap(member))  # through deprecation wrappers
            for signature in signatures if isinstance(signatures, list) else [signatures] if signatures else []:
                for parameter in signature.parameters:
                    if re.fullmatch(r"[a-z]+[A-Z]\w*", parameter):
                        found.add(f"{class_name}.{name}({parameter}=)")
    return sorted(found)


def test_no_camel_case_keyword():
    assert _camel_case_keywords() == []


def test_remote_graphs_take_metadata(plot):
    graph = plot.add_remote_line_graph(["B"], metadata={"product": "b"})
    assert graph.metadata() == {"product": "b"}

