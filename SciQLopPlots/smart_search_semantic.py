"""Generic, pretrained local text-embedding search method -- never trained on
or aware of this project's product catalog. Requires the optional
SciQLopPlots[smart_search] extra (fastembed) to be installed."""
from typing import Sequence

from .smart_search_method import NodeSnapshot

DEFAULT_MODEL = "BAAI/bge-small-en-v1.5"


class SemanticSearchMethod:
    def __init__(self, model_name: str = DEFAULT_MODEL):
        from fastembed import TextEmbedding

        self._model_name = model_name
        self._model = TextEmbedding(model_name=model_name)
        self._path_keys = []
        self._embeddings = None

    @property
    def model_name(self) -> str:
        return self._model_name

    def index(self, nodes: Sequence[NodeSnapshot]) -> None:
        # Compute before touching either attribute, then assign both back to
        # back: a concurrent query() can still interleave between the two
        # stores, but only across two adjacent bytecodes instead of across
        # the full (possibly slow) embedding computation.
        import numpy as np

        path_keys = [n.path_key for n in nodes]
        texts = [n.raw_text for n in nodes]
        embeddings = np.array(list(self._model.embed(texts))) if texts else None
        self._path_keys = path_keys
        self._embeddings = embeddings

    def query(self, text: str) -> dict:
        import numpy as np

        path_keys, embeddings = self._path_keys, self._embeddings
        if embeddings is None or not path_keys:
            return {}
        query_vec = next(self._model.embed([text]))
        norms = np.linalg.norm(embeddings, axis=1) * np.linalg.norm(query_vec)
        norms[norms == 0] = 1.0
        cosine = (embeddings @ query_vec) / norms
        return {
            path_key: float(max(0.0, sim)) * 100.0
            for path_key, sim in zip(path_keys, cosine)
        }
