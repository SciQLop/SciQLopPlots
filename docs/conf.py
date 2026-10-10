import re
from pathlib import Path

project = "SciQLopPlots"
author = "Alexis Jeandet"
copyright = "SciQLop contributors"
version = release = re.search(
    r"__version__\s*=\s*['\"]([^'\"]+)",
    (Path(__file__).parent.parent / "SciQLopPlots" / "__init__.py").read_text(),
).group(1)

extensions = ["myst_parser"]
source_suffix = {".md": "markdown"}
myst_heading_anchors = 3

# docs/ also holds internal notes (plans, backlogs, reviews): publish only the pages below.
_published = {"index.md", "user-guide.md"}
exclude_patterns = ["_build"] + [
    str(p.relative_to(Path(__file__).parent))
    for p in Path(__file__).parent.rglob("*.md")
    if p.name not in _published
]

html_theme = "sphinx_rtd_theme"
html_title = f"SciQLopPlots {release}"
