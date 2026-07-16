"""Public API for SciQLop's settings UI to drive the optional smart-search
feature. Safe to import unconditionally -- never imports fastembed at module
import time, only lazily inside is_available()/set_enabled()."""
import threading

from .smart_search_controller import SmartSearchController

AVAILABLE_MODELS = [
    "BAAI/bge-small-en-v1.5",
    "sentence-transformers/all-MiniLM-L6-v2",
]

_controller = None
_model_name = AVAILABLE_MODELS[0]
_enabled = False


def is_available() -> bool:
    try:
        import fastembed  # noqa: F401
    except ImportError:
        return False
    return True


def available_models() -> list:
    return list(AVAILABLE_MODELS)


def get_model() -> str:
    return _model_name


def set_model(name: str) -> None:
    global _model_name
    if name not in AVAILABLE_MODELS:
        raise ValueError(f"Unknown smart-search model: {name!r}. Available: {AVAILABLE_MODELS}")
    _model_name = name
    if _controller is not None and is_enabled():
        _rebuild_controller_method()


def is_enabled() -> bool:
    return _enabled


def controller() -> SmartSearchController:
    global _controller
    if _controller is None:
        _controller = SmartSearchController()
    return _controller


def set_enabled(enabled: bool, on_ready=None, on_error=None) -> None:
    """Asynchronous. Disabling is always synchronous and always succeeds.
    Enabling loads/downloads the model on a background thread; on_ready()/
    on_error(exc) are called from that thread when it finishes -- callers
    driving Qt UI must marshal back to the GUI thread themselves (e.g. via a
    Qt signal), the same cross-thread contract SmartSearchController uses."""
    global _enabled
    if not enabled:
        _enabled = False
        if _controller is not None:
            _controller.set_enabled(False)
        return

    if not is_available():
        if on_error is not None:
            on_error(RuntimeError("fastembed is not installed"))
        return

    def worker():
        global _enabled
        try:
            _rebuild_controller_method()
        except Exception as exc:
            if on_error is not None:
                on_error(exc)
            return
        _enabled = True
        controller().set_enabled(True)
        if on_ready is not None:
            on_ready()

    threading.Thread(target=worker, daemon=True).start()


def _rebuild_controller_method() -> None:
    from .smart_search_semantic import SemanticSearchMethod

    c = controller()
    c.clear_methods()
    c.register_method(SemanticSearchMethod(_model_name))
