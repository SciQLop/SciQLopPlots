# SciQLopPlots

SciQLopPlots draws scientific plots in Qt applications.
It stays fast with big data: millions of points pan and zoom smoothly, because a background thread downsamples what you see.
It is built for [SciQLop](https://github.com/SciQLop/SciQLop) but works in any PySide6 program.

```bash
pip install SciQLopPlots
```

Wheels exist for Python 3.11 to 3.14 on Linux, macOS and Windows.

![A time series panel](images/panel.png)

It also runs in the browser, from JavaScript. **[Try the gallery](https://sciqlop.github.io/SciQLopPlots/)**, or read [JavaScript (WebAssembly)](javascript.md) to use it in your own pages.

```{toctree}
:maxdepth: 2

user-guide
api
javascript
```

Source code, issues and releases: [github.com/SciQLop/SciQLopPlots](https://github.com/SciQLop/SciQLopPlots).
