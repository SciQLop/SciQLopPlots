import inspect
import sys
import threading
from concurrent.futures import ThreadPoolExecutor

try:
    from .event import Event
except ImportError:
    from event import Event


def _detect_call_style(func):
    """Detect whether a callable expects an Event or positional args.

    Returns "event" if the function takes a single parameter (new style),
    "positional" if it takes multiple parameters (old style),
    "none" if it takes no parameters.
    """
    try:
        sig = inspect.signature(func)
        params = [
            p for p in sig.parameters.values()
            if p.default is inspect.Parameter.empty
            and p.kind in (
                inspect.Parameter.POSITIONAL_ONLY,
                inspect.Parameter.POSITIONAL_OR_KEYWORD,
            )
        ]
        if len(params) == 0:
            return "none"
        elif len(params) == 1:
            return "event"
        else:
            return "positional"
    except (ValueError, TypeError):
        return "event"


def _make_step(source_prop, transform, target_prop, alive):
    """value -> transform -> target, for one emission. Nothing reaches a pipeline no longer alive."""
    call_style = _detect_call_style(transform) if transform else None

    def step(value, args):
        try:
            if transform is None:
                result = value
            elif call_style == "event":
                result = transform(Event(source=source_prop.qobject, value=value,
                                         property_name=source_prop.property_name))
            elif call_style == "positional":
                result = transform(*args)
            else:
                result = transform()

            if result is None and transform is not None:
                return

            if target_prop is not None and alive():
                target_prop.value = result
        except RuntimeError:
            # Source or target QObject may have been destroyed
            pass

    return step


def _make_dispatch(source_prop, transform, target_prop, run=None, alive=lambda: True):
    """The slot: reads the value where the signal fires, then runs the step there or via run."""
    step = _make_step(source_prop, transform, target_prop, alive)

    def slot(*args):
        try:
            # If signal fired with no args (e.g. data_changed()), fetch from getter
            value = source_prop.value if not args else args[0] if len(args) == 1 else args
        except RuntimeError:
            return
        if run is None:
            step(value, args)
        else:
            run(lambda: step(value, args))

    return slot


class _LatestOnlyWorker:
    """One thread, jobs in order; a job superseded before it starts is skipped, like data fetches."""

    def __init__(self):
        self._executor = ThreadPoolExecutor(max_workers=1, thread_name_prefix="sciqlop-pipeline")
        self._lock = threading.Lock()
        self._latest = 0

    def submit(self, job):
        with self._lock:
            self._latest += 1
            ticket = self._latest
        try:
            self._executor.submit(self._run, ticket, job)
        except RuntimeError:  # stopped meanwhile
            pass

    def _run(self, ticket, job):
        with self._lock:
            if ticket != self._latest:
                return
        try:
            job()
        except Exception:
            sys.excepthook(*sys.exc_info())

    def stop(self):
        with self._lock:
            self._latest += 1  # what is still queued is skipped
        self._executor.shutdown(wait=False)


class Pipeline:
    """A live connection: source.on.prop >> transform >> target.on.prop"""

    def __init__(self, source_prop, transform, target_prop):
        self._source_prop = source_prop
        self._target_prop = target_prop
        self._transform = transform
        self._connected = False
        self._slot = None
        self._worker = None

        self._connect()

    def _connect(self):
        signal = self._source_prop.signal
        if signal is None:
            raise ValueError(
                f"Cannot observe '{self._source_prop.property_name}': no signal"
            )
        run = self._worker.submit if self._worker else None
        # A flag, not self: the connection must not keep a dropped Pipeline alive.
        live = self._live = [True]
        self._slot = _make_dispatch(self._source_prop, self._transform, self._target_prop, run,
                                    alive=lambda: live[0])
        signal.connect(self._slot)
        self._connected = True

    def disconnect(self):
        if self._connected and self._slot is not None:
            try:
                self._source_prop.signal.disconnect(self._slot)
            except (RuntimeError, TypeError):
                pass
            self._connected = False
            self._live[0] = False
        if self._worker is not None:
            self._worker.stop()
            self._worker = None

    @property
    def connected(self):
        return self._connected

    def threaded(self):
        """Run the transform and the target's setter on a worker thread of this pipeline.

        The value is read where the signal fires. While the worker is busy, only the
        latest value waits: older ones are skipped.
        """
        if self._worker is None:
            self.disconnect()
            self._worker = _LatestOnlyWorker()
            self._connect()
        return self


class PartialPipeline:
    """Intermediate result of source >> transform. Live as a sink, upgradeable with >> target."""

    def __init__(self, source_prop, transform):
        self._source_prop = source_prop
        self._transform = transform
        self._sink_pipeline = Pipeline(source_prop, transform, None)
        self._threaded = False

    def disconnect(self):
        self._sink_pipeline.disconnect()

    def threaded(self):
        """As Pipeline.threaded(), kept when the sink is extended with >> target."""
        self._threaded = True
        self._sink_pipeline.threaded()
        return self

    def __rshift__(self, other):
        try:
            from .properties import ObservableProperty
        except ImportError:
            from properties import ObservableProperty

        if isinstance(other, ObservableProperty):
            self._sink_pipeline.disconnect()
            pipeline = Pipeline(self._source_prop, self._transform, other)
            return pipeline.threaded() if self._threaded else pipeline
        raise TypeError(
            f"Cannot pipe to {type(other).__name__}. Expected ObservableProperty."
        )


def build_pipeline_step(source_prop, other):
    """Called by ObservableProperty.__rshift__. Routes to Pipeline or PartialPipeline."""
    try:
        from .properties import ObservableProperty
    except ImportError:
        from properties import ObservableProperty

    if isinstance(other, ObservableProperty):
        return Pipeline(source_prop, None, other)
    elif callable(other):
        return PartialPipeline(source_prop, other)
    else:
        raise TypeError(
            f"Cannot pipe to {type(other).__name__}. Expected callable or ObservableProperty."
        )
