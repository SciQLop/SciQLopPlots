"""Which SciQLopPlots methods a piece of code calls, measured with sys.setprofile."""
import os
import sys

import SciQLopPlots

_PACKAGE_DIR = os.path.dirname(SciQLopPlots.__file__)

# Public methods the stories don't call, and why.
NOT_IN_STORIES = {}


def _declaring(cls, name):
    """The SciQLopPlots class in cls's MRO whose own dict defines name, or None."""
    for base in cls.__mro__:
        if name in vars(base) and getattr(SciQLopPlots, base.__name__, None) is base:
            return base.__name__
    return None


def collect_called(fn) -> set[tuple[str, str]]:
    """Run fn and return the (declaring class, method) pairs of the SciQLopPlots methods it called:
    bound C methods, and the Python wrappers the package installs on its classes."""
    called = set()

    def profile(frame, event, arg):
        if event == "c_call":
            owner = getattr(arg, "__self__", None)
            if owner is not None and not isinstance(owner, type(sys)):
                cls = owner if isinstance(owner, type) else type(owner)
                declaring = _declaring(cls, arg.__name__)
                if declaring:
                    called.add((declaring, arg.__name__))
        elif event == "call" and frame.f_code.co_filename.startswith(_PACKAGE_DIR):
            receiver = frame.f_locals.get("self")
            if receiver is not None:
                declaring = _declaring(type(receiver), frame.f_code.co_name)
                if declaring:
                    called.add((declaring, frame.f_code.co_name))

    sys.setprofile(profile)
    try:
        fn()
    finally:
        sys.setprofile(None)
    return called
