import weakref

# Registry: {class: {property_name: property_spec_dict}}
_property_registry = {}


def register_property(cls, property_name, signal_name, getter_name, setter_name, property_type, signal_args=None):
    """Register an observable property for a class.

    Args:
        cls: The class to register the property on.
        property_name: Name for the .on namespace (e.g. "range").
        signal_name: Qt signal attribute name (e.g. "range_changed"), or None for write-only.
        getter_name: Method name to get current value (e.g. "range"), or None.
        setter_name: Method name to set value (e.g. "set_range"), or None for read-only.
        property_type: One of "data", "range", "string" - determines threading strategy.
        signal_args: Tuple of types to select a specific signal overload, or None for default.
    """
    if cls not in _property_registry:
        _property_registry[cls] = {}
    _property_registry[cls][property_name] = {
        "signal_name": signal_name,
        "getter_name": getter_name,
        "setter_name": setter_name,
        "property_type": property_type,
        "signal_args": signal_args,
    }


def _lookup_property_spec(obj, property_name):
    """Look up a property spec, walking the MRO."""
    for cls in type(obj).__mro__:
        if cls in _property_registry and property_name in _property_registry[cls]:
            return _property_registry[cls][property_name]
    return None


class ObservableProperty:
    """A reference to a specific observable property on a specific QObject instance."""
    __slots__ = ("_qobject_ref", "property_name", "property_type",
                 "_signal_name", "_getter_name", "_setter_name", "_signal_args")

    def __init__(self, qobject, property_name, spec):
        self._qobject_ref = weakref.ref(qobject) if hasattr(qobject, '__weakref__') else lambda: qobject
        self.property_name = property_name
        self.property_type = spec["property_type"]
        self._signal_name = spec["signal_name"]
        self._getter_name = spec["getter_name"]
        self._setter_name = spec["setter_name"]
        self._signal_args = spec.get("signal_args")

    @property
    def qobject(self):
        obj = self._qobject_ref()
        if obj is None:
            raise RuntimeError(f"Source QObject for '{self.property_name}' has been destroyed")
        return obj

    @property
    def signal(self):
        if self._signal_name is None:
            return None
        sig = getattr(self.qobject, self._signal_name)
        if self._signal_args is not None:
            sig = sig[self._signal_args]
        return sig

    @property
    def value(self):
        if self._getter_name is None:
            raise AttributeError(f"Property '{self.property_name}' is not readable")
        return getattr(self.qobject, self._getter_name)()

    @value.setter
    def value(self, val):
        if self._setter_name is None:
            raise AttributeError(f"Property '{self.property_name}' is not writable")
        if isinstance(val, (list, tuple)):
            getattr(self.qobject, self._setter_name)(*val)
        else:
            getattr(self.qobject, self._setter_name)(val)

    def __rshift__(self, other):
        # Defer import to avoid circular dependency
        try:
            from .pipeline import build_pipeline_step
        except ImportError:
            from pipeline import build_pipeline_step
        return build_pipeline_step(self, other)


class OnNamespace:
    """The .on namespace that provides access to ObservableProperty instances."""
    __slots__ = ("_qobject",)

    def __init__(self, qobject):
        self._qobject = qobject

    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        spec = _lookup_property_spec(self._qobject, name)
        if spec is None:
            raise AttributeError(
                f"{type(self._qobject).__name__} has no observable property '{name}'"
            )
        return ObservableProperty(self._qobject, name, spec)


class OnDescriptor:
    """Descriptor that provides the .on namespace on instances."""

    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        return OnNamespace(obj)
