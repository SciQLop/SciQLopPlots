import sys
import os

# Allow importing SciQLopPlots submodules directly for unit tests
# that don't need the full C++ bindings
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'SciQLopPlots'))
