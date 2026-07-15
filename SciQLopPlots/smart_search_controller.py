"""Combines however many SearchMethod instances are registered into a single
per-leaf score overlay via weighted max. Adding a new method later never
requires touching this combination logic -- see docs/superpowers/specs/
2026-07-15-pluggable-smart-search-design.md."""
import threading
from typing import Sequence

from .smart_search_method import NodeSnapshot, SearchMethod


class SmartSearchController:
    def __init__(self):
        self._methods = []
        self._enabled = False
        self._source_model = None
        self._corpus_model = None
        self._targets = []

    def register_method(self, method: SearchMethod, weight: float = 1.0) -> None:
        self._methods.append((method, weight))

    def clear_methods(self) -> None:
        self._methods.clear()

    def attach(self, source_model, corpus_model, targets: Sequence) -> None:
        """source_model: the ProductsModel emitting rowsInserted/rowsRemoved/
        modelReset. corpus_model: any ProductsFlatFilterModel instance, used
        only for its corpus_snapshot() -- not necessarily a visible view.
        targets: filter model instances (tree and/or flat) that receive the
        combined score overlay via set_external_scores()."""
        self._source_model = source_model
        self._corpus_model = corpus_model
        self._targets = list(targets)
        for target in self._targets:
            target.set_smart_search_enabled(self._enabled)
        source_model.rowsInserted.connect(self._reindex)
        source_model.rowsRemoved.connect(self._reindex)
        source_model.modelReset.connect(self._reindex)
        self._reindex()

    def set_enabled(self, enabled: bool) -> None:
        self._enabled = enabled
        for target in self._targets:
            target.set_smart_search_enabled(enabled)
        if enabled:
            self._reindex()

    def index(self, nodes: Sequence[NodeSnapshot]) -> None:
        for method, _weight in self._methods:
            method.index(nodes)

    def _reindex(self, *args) -> None:
        if not self._enabled or self._corpus_model is None:
            return
        snapshot = self._corpus_model.corpus_snapshot()
        nodes = [NodeSnapshot(path_key, raw_text) for path_key, raw_text in snapshot.items()]
        threading.Thread(target=self.index, args=(nodes,), daemon=True).start()

    def on_query_changed(self, text: str) -> None:
        if not self._enabled:
            return
        threading.Thread(target=self._query_worker, args=(text,), daemon=True).start()

    def _query_worker(self, text: str) -> None:
        combined = self.combined_scores(text)
        for target in self._targets:
            target.set_external_scores(combined)

    def combined_scores(self, text: str) -> dict:
        combined = {}
        for method, weight in self._methods:
            for path_key, score in method.query(text).items():
                weighted = score * weight
                if weighted > combined.get(path_key, 0.0):
                    combined[path_key] = weighted
        return combined
