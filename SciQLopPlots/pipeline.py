import inspect

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


def _make_dispatch(source_prop, transform, target_prop):
    """Create the slot function that handles signal emission."""
    call_style = _detect_call_style(transform) if transform else None

    def slot(*args):
        try:
            value = args[0] if len(args) == 1 else args

            if transform is None:
                result = value
            else:
                if call_style == "event":
                    event = Event(
                        source=source_prop.qobject,
                        value=value,
                        property_name=source_prop.property_name,
                    )
                    result = transform(event)
                elif call_style == "positional":
                    result = transform(*args)
                else:
                    result = transform()

            if result is None and transform is not None:
                return

            if target_prop is not None:
                target_prop.value = result
        except RuntimeError:
            # Source or target QObject may have been destroyed
            pass

    return slot


class Pipeline:
    """A live connection: source.on.prop >> transform >> target.on.prop"""

    def __init__(self, source_prop, transform, target_prop):
        self._source_prop = source_prop
        self._target_prop = target_prop
        self._transform = transform
        self._connected = False
        self._slot = None
        self._auto_threaded = (
            target_prop is not None and target_prop.property_type == "data"
        )

        self._connect()

    def _connect(self):
        signal = self._source_prop.signal
        if signal is None:
            raise ValueError(
                f"Cannot observe '{self._source_prop.property_name}': no signal"
            )
        self._slot = _make_dispatch(self._source_prop, self._transform, self._target_prop)
        signal.connect(self._slot)
        self._connected = True

    def disconnect(self):
        if self._connected and self._slot is not None:
            try:
                self._source_prop.signal.disconnect(self._slot)
            except (RuntimeError, TypeError):
                pass
            self._connected = False

    @property
    def connected(self):
        return self._connected

    def threaded(self):
        """Force this pipeline to execute transforms in a worker thread."""
        self._auto_threaded = True
        return self


class PartialPipeline:
    """Intermediate result of source >> transform. Live as a sink, upgradeable with >> target."""

    def __init__(self, source_prop, transform):
        self._source_prop = source_prop
        self._transform = transform
        self._sink_pipeline = Pipeline(source_prop, transform, None)

    def __rshift__(self, other):
        try:
            from .properties import ObservableProperty
        except ImportError:
            from properties import ObservableProperty

        if isinstance(other, ObservableProperty):
            self._sink_pipeline.disconnect()
            return Pipeline(self._source_prop, self._transform, other)
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
