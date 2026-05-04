"""Python facade for the SciQLopPlots runtime tracer.

Records Chrome trace JSON / Perfetto-compatible events from both C++ and Python
into the same trace file. Toggle at runtime; ship in production.

Quick start
-----------
>>> from SciQLopPlots import tracing
>>> tracing.enable("/tmp/trace.json")
>>> with tracing.zone("fetch", cat="data", product="amda/mms_fgm"):
...     ...  # work
>>> tracing.disable()  # flushes and closes the file

Or set the env var SCIQLOP_TRACE=/tmp/trace.json before launching.
"""
from __future__ import annotations

import functools
from typing import Any, Optional

from . import SciQLopPlotsBindings as _b


def enable(path: str) -> None:
    _b.tracing_enable(path)


def disable() -> None:
    _b.tracing_disable()


def flush() -> None:
    _b.tracing_flush()


def is_enabled() -> bool:
    return _b.tracing_is_enabled()


def set_thread_name(name: str) -> None:
    _b.tracing_set_thread_name(name)


def counter(name: str, value: float, cat: str = "") -> None:
    _b.tracing_counter(name, float(value), cat)


def async_begin(name: str, cat: str = "") -> int:
    return _b.tracing_async_begin(name, cat)


def async_end(handle: int) -> None:
    _b.tracing_async_end(handle)


def _add_arg(handle: int, key: str, value: Any) -> None:
    if isinstance(value, bool):
        _b.tracing_sync_zone_add_bool(handle, key, value)
    elif isinstance(value, int):
        _b.tracing_sync_zone_add_int(handle, key, value)
    elif isinstance(value, float):
        _b.tracing_sync_zone_add_double(handle, key, value)
    else:
        _b.tracing_sync_zone_add_str(handle, key, str(value))


class zone:
    """Context manager for a synchronous trace zone.

    Example:
        with tracing.zone("fetch", cat="data", n_points=4096): ...
    """

    __slots__ = ("_name", "_cat", "_args", "_handle")

    def __init__(self, name: str, cat: str = "", **args: Any) -> None:
        self._name = name
        self._cat = cat
        self._args = args
        self._handle = 0

    def __enter__(self) -> "zone":
        self._handle = _b.tracing_sync_zone_begin(self._name, self._cat)
        if self._handle and self._args:
            for k, v in self._args.items():
                _add_arg(self._handle, k, v)
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        if self._handle:
            _b.tracing_sync_zone_end(self._handle)
            self._handle = 0


def traced(name: Optional[str] = None, cat: str = ""):
    """Decorator that wraps a function call in a synchronous zone."""

    def decorator(fn):
        zname = name or fn.__qualname__

        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            with zone(zname, cat=cat):
                return fn(*args, **kwargs)

        return wrapper

    return decorator


class session:
    """Context manager that enables tracing for the duration of a `with` block.

    Example (e.g. from the SciQLop Jupyter console wrapping a slow operation):

        from SciQLopPlots import tracing
        with tracing.session("/tmp/slow_pan.json"):
            # ...reproduce the slow path...
            ...
        # File is flushed and closed at exit. Open in https://ui.perfetto.dev/
    """

    __slots__ = ("_path",)

    def __init__(self, path: str) -> None:
        self._path = path

    def __enter__(self) -> "session":
        enable(self._path)
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        disable()
