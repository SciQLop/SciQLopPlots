_RANGE_PROPERTIES = frozenset({"range"})
_DATA_PROPERTIES = frozenset({"data"})


class Event:
    """Event object passed to pipeline transform functions.

    Attributes:
        source: The QObject that emitted the signal.
        value: The raw value from the signal.
        property_name: Name of the property that changed ("range", "data", etc.).
    """
    __slots__ = ("source", "value", "property_name")

    def __init__(self, source, value, property_name):
        self.source = source
        self.value = value
        self.property_name = property_name

    @property
    def range(self):
        """Returns value if this is a range event, None otherwise."""
        if self.property_name in _RANGE_PROPERTIES:
            return self.value
        return None

    @property
    def data(self):
        """Returns value if this is a data event, None otherwise."""
        if self.property_name in _DATA_PROPERTIES:
            return self.value
        return None
