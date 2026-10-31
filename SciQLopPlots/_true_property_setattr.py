"""Assigns from a module that uses true_property: PySide picks Q_PROPERTYs or methods by the module
of the running Python frame, and the library's own guards must not turn a caller's property into
a plain attribute."""
from __feature__ import true_property  # noqa: F401


def assign(parent, name, value):
    parent.__setattr__(name, value)
