"""Tests for the public smart_search API surface -- degradation paths in
particular, since these must never let a failure break baseline search."""
import builtins
import threading
import time

import pytest
import SciQLopPlots.smart_search as smart_search


@pytest.fixture(autouse=True)
def reset_smart_search_state():
    smart_search._controller = None
    smart_search._enabled = False
    smart_search._model_name = smart_search.AVAILABLE_MODELS[0]
    yield
    smart_search._controller = None
    smart_search._enabled = False
    smart_search._model_name = smart_search.AVAILABLE_MODELS[0]


def _simulate_fastembed_missing(monkeypatch):
    real_import = builtins.__import__

    def fake_import(name, *args, **kwargs):
        if name == "fastembed":
            raise ImportError("simulated: fastembed not installed")
        return real_import(name, *args, **kwargs)

    monkeypatch.setattr(builtins, "__import__", fake_import)


def test_is_available_reflects_fastembed_import(monkeypatch):
    _simulate_fastembed_missing(monkeypatch)
    assert smart_search.is_available() is False


def test_set_enabled_true_without_fastembed_reports_error_and_stays_disabled(monkeypatch):
    _simulate_fastembed_missing(monkeypatch)
    errors = []
    done = threading.Event()

    def on_error(exc):
        errors.append(exc)
        done.set()

    smart_search.set_enabled(True, on_error=on_error)
    assert done.wait(timeout=5)
    assert len(errors) == 1
    assert not smart_search.is_enabled()


def test_set_model_rejects_unknown_name():
    with pytest.raises(ValueError):
        smart_search.set_model("not-a-real-model")


def test_set_enabled_false_is_always_safe_even_when_never_enabled():
    smart_search.set_enabled(False)
    assert not smart_search.is_enabled()


def test_available_models_is_non_empty_and_get_model_defaults_to_first():
    models = smart_search.available_models()
    assert len(models) > 0
    assert smart_search.get_model() == models[0]
