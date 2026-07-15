"""Pluggable search-method interface for SciQLopPlots' smart search layer.
Deliberately uses plain (path_key, raw_text) tuples rather than a live
ProductsModelNode reference, so index()/query() never need to touch a
Shiboken-wrapped C++ object (and therefore never need the GIL) on the hot
per-query path -- only when the corpus itself is re-snapshotted."""
from typing import NamedTuple, Protocol, Sequence


class NodeSnapshot(NamedTuple):
    path_key: str
    raw_text: str


class SearchMethod(Protocol):
    def index(self, nodes: Sequence[NodeSnapshot]) -> None:
        ...

    def query(self, text: str) -> dict:
        ...
