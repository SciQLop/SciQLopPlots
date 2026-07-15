"""Pure-Python tests for SmartSearchController's registration and
score-combination logic -- no Qt, no fastembed, no product model involved."""
from SciQLopPlots.smart_search_controller import SmartSearchController
from SciQLopPlots.smart_search_method import NodeSnapshot


class FakeMethod:
    def __init__(self, scores):
        self._scores = scores
        self.indexed_nodes = None

    def index(self, nodes):
        self.indexed_nodes = list(nodes)

    def query(self, text):
        return dict(self._scores)


def test_single_method_scores_pass_through():
    controller = SmartSearchController()
    controller.register_method(FakeMethod({"a": 80.0, "b": 20.0}))
    assert controller.combined_scores("whatever") == {"a": 80.0, "b": 20.0}


def test_two_methods_combine_via_max():
    controller = SmartSearchController()
    controller.register_method(FakeMethod({"a": 30.0, "b": 90.0}))
    controller.register_method(FakeMethod({"a": 70.0, "c": 10.0}))
    assert controller.combined_scores("whatever") == {"a": 70.0, "b": 90.0, "c": 10.0}


def test_weighted_combination():
    controller = SmartSearchController()
    controller.register_method(FakeMethod({"a": 100.0}), weight=0.5)
    assert controller.combined_scores("whatever") == {"a": 50.0}


def test_no_methods_registered_returns_empty():
    controller = SmartSearchController()
    assert controller.combined_scores("whatever") == {}


def test_clear_methods_removes_all():
    controller = SmartSearchController()
    controller.register_method(FakeMethod({"a": 100.0}))
    controller.clear_methods()
    assert controller.combined_scores("whatever") == {}


def test_index_forwards_nodes_to_every_method():
    controller = SmartSearchController()
    m1, m2 = FakeMethod({}), FakeMethod({})
    controller.register_method(m1)
    controller.register_method(m2)
    nodes = [NodeSnapshot("a", "text a"), NodeSnapshot("b", "text b")]
    controller.index(nodes)
    assert m1.indexed_nodes == nodes
    assert m2.indexed_nodes == nodes
