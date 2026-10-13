# SciQLopPlots

SciQLopPlots draws scientific plots in Qt applications.
It stays fast with big data: millions of points pan and zoom smoothly, because a background thread downsamples what you see.
It is built for [SciQLop](https://github.com/SciQLop/SciQLop) but works in any PySide6 program.
It also runs in the browser, from JavaScript: see [JavaScript (WebAssembly)](javascript.md).

```bash
pip install SciQLopPlots
```

![A time series panel](images/panel.png)

```{toctree}
:maxdepth: 2

user-guide
api
javascript
```

Source code, issues and releases: [github.com/SciQLop/SciQLopPlots](https://github.com/SciQLop/SciQLopPlots).
