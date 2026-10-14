"""Draw the explanatory diagrams of the README and of the user guide's signal processing section.

Each diagram is a standalone SVG with its own style and background, so it reads the
same on Read the Docs and on GitHub. The gap diagram places its samples, segments and
NaN row from SciQLopPlots.dsp itself, so it cannot drift from what the code does.

    QT_QPA_PLATFORM=offscreen PYTHONPATH=build-venv python docs/make_diagrams.py

Look at every picture before committing it (rsvg-convert renders them to PNG).
"""
import pathlib

import numpy as np

from SciQLopPlots import dsp

IMAGES = pathlib.Path(__file__).resolve().parent / "images"
WIDTH = 760

STYLE = """
.bg { fill: #ffffff; stroke: #d0d7de; }
text { font-family: "DejaVu Sans", Helvetica, Arial, sans-serif; font-size: 13px; fill: #1f2328; }
.ts { font-size: 11px; fill: #59636e; }
.tb { font-weight: 600; }
.mono { font-family: "DejaVu Sans Mono", Menlo, Consolas, monospace; font-size: 12px; }
.c { text-anchor: middle; }
.r { text-anchor: end; }
.axis { stroke: #8c959f; stroke-width: 1; }
.line { stroke: #59636e; stroke-width: 1.5; fill: none; }
.dash { stroke-dasharray: 4 3; }
.arrow { stroke: #59636e; stroke-width: 1.5; fill: none; marker-end: url(#arrow); }
.blue { fill: #dbeafe; stroke: #2563eb; }
.orange { fill: #fef3c7; stroke: #d97706; }
.green { fill: #d1fae5; stroke: #059669; }
.red { fill: #fee2e2; stroke: #dc2626; }
.violet { fill: #ede9fe; stroke: #7c3aed; }
.grey { fill: #f6f8fa; stroke: #8c959f; }
.dot-blue { fill: #2563eb; }
.dot-orange { fill: #d97706; }
.dot-grey { fill: #8c959f; }
.nan { fill: #ffffff; stroke: #dc2626; stroke-width: 2; }
.stroke-blue { stroke: #2563eb; stroke-width: 2; fill: none; }
.stroke-orange { stroke: #d97706; stroke-width: 2; fill: none; }
.stroke-green { stroke: #059669; stroke-width: 2; fill: none; }
.stroke-red { stroke: #dc2626; stroke-width: 2; fill: none; }
.text-blue { fill: #2563eb; font-weight: 600; }
.text-orange { fill: #b45309; font-weight: 600; }
.text-green { fill: #047857; font-weight: 600; }
.text-red { fill: #dc2626; font-weight: 600; }
.text-violet { fill: #6d28d9; font-weight: 600; }
"""


def svg(height, label, *body):
    return "\n".join([
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {WIDTH} {height}" '
        f'width="{WIDTH}" height="{height}" role="img" aria-label="{label}">',
        f"<style>{STYLE}</style>",
        '<defs><marker id="arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" '
        'markerHeight="7" orient="auto-start-reverse"><path d="M 0 0 L 10 5 L 0 10 z" '
        'fill="#59636e"/></marker></defs>',
        f'<rect class="bg" x="0.5" y="0.5" width="{WIDTH - 1}" height="{height - 1}" rx="8"/>',
        *body,
        "</svg>",
    ])


def text(x, y, s, cls=""):
    return f'<text class="{cls}" x="{x:.1f}" y="{y:.1f}">{s}</text>'


def line(x1, y1, x2, y2, cls="line"):
    return f'<path class="{cls}" d="M{x1:.1f} {y1:.1f} L{x2:.1f} {y2:.1f}"/>'


def rect(x, y, w, h, cls, rx=6):
    return f'<rect class="{cls}" x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" rx="{rx}"/>'


def dot(x, y, cls, r=4.5):
    return f'<circle class="{cls}" cx="{x:.1f}" cy="{y:.1f}" r="{r}"/>'


def polyline(points, cls):
    return f'<polyline class="{cls}" points="{" ".join(f"{x:.1f},{y:.1f}" for x, y in points)}"/>'


def bracket(x1, x2, y, label, cls="line", below=False):
    """A horizontal bracket from x1 to x2 with a centred label."""
    tick = 5 if below else -5
    ty = y + 16 if below else y - 8
    return "\n".join([
        f'<path class="{cls}" d="M{x1:.1f} {y + tick:.1f} V{y:.1f} H{x2:.1f} V{y + tick:.1f}"/>',
        text((x1 + x2) / 2, ty, label, "ts c"),
    ])


def gaps():
    x = np.r_[0:9, 15:24].astype(float)
    y = np.sin(x / 3)
    segments = dsp.split_segments(x, y)
    x_out, y_out = dsp.rolling_mean(x, y, 1)  # window 1: values unchanged, gap rows added
    px = lambda t: 70 + t * 27
    colours = ["blue", "orange"]
    segment_of = lambda t: next(i for i, (a, b) in enumerate(segments) if x[a] <= t <= x[b - 1])

    body = [text(20, 34, "1. Input", "tb")]
    body += [line(px(-0.5), 86, px(23.5), 86, "axis")]
    body += [dot(px(t), 86, f"dot-{colours[segment_of(t)]}") for t in x]
    body += [bracket(px(0), px(1), 70, "median Δt")]
    body += [bracket(px(8), px(15), 70, "Δt &gt; gap_factor × median Δt  →  a gap", "stroke-red")]

    body += [text(20, 140, "2. Split at gaps", "tb")]
    for i, (a, b) in enumerate(segments):
        x0, x1 = px(x[a]) - 12, px(x[b - 1]) + 12
        body += [rect(x0, 152, x1 - x0, 34, colours[i])]
        body += [text((x0 + x1) / 2, 174, f"segment {i + 1}", f"c text-{colours[i]}")]
    body += [text(WIDTH / 2, 208, "Each segment is processed on its own, in parallel: "
                  "no filter or window reaches across a gap.", "ts c")]

    body += [text(20, 246, "3. Output", "tb")]
    py = lambda v: 302 - v * 22
    for i, (a, b) in enumerate(segments):
        keep = (x_out >= x[a]) & (x_out <= x[b - 1])
        body += [polyline(zip(px(x_out[keep]), py(y_out[keep])), f"stroke-{colours[i]}")]
        body += [dot(px(t), py(v), f"dot-{colours[i]}", 3.5) for t, v in zip(x_out[keep], y_out[keep])]
    for t in x_out[np.isnan(y_out)]:
        body += [dot(px(t), 302, "nan", 6), text(px(t), 340, "one NaN row at the midpoint", "ts c")]
        body += [text(px(t), 354, "breaks the plotted line", "ts c")]
    return svg(368, "Gap handling: split at gaps, process each segment, join with a NaN row", *body)


def rolling_window():
    px = lambda i: 80 + i * 44
    nan_at, gap_after = 6, 9
    body = [text(20, 34, "rolling_mean(x, y, window=5)", "mono")]
    body += [line(px(-0.5), 70, px(gap_after + 0.5), 70, "axis"), line(px(10.5), 70, px(13.5), 70, "axis")]
    body += [text((px(gap_after) + px(11)) / 2, 74, "gap", "ts c text-red")]
    for i in [*range(gap_after + 1), *range(11, 14)]:
        body += [dot(px(i), 70, "nan", 5) if i == nan_at else dot(px(i), 70, "dot-blue")]
    body += [text(px(nan_at), 52, "NaN", "ts c text-red")]

    windows = [  # (centre, first, last, colour, label)
        (4, 2, 6, "blue", "centred on the sample: 2 before, 2 after"),
        (0, 0, 2, "orange", "shrinks at the start and end"),
        (8, 6, 9, "green", "stops at a gap"),
    ]
    for row, (centre, first, last, colour, label) in enumerate(windows):
        y = 104 + row * 34
        body += [rect(px(first) - 14, y, px(last) - px(first) + 28, 22, colour, 11)]
        body += [dot(px(centre), y + 11, f"dot-{'blue' if colour == 'blue' else 'grey'}", 3.5)]
        body += [text(px(13) + 10, y + 16, label, f"r text-{colour}")]
    body += [text(WIDTH / 2, 226, "NaN samples inside a window are skipped: the mean is "
                  "taken over the remaining samples.", "ts c")]
    return svg(244, "A centred rolling window that shrinks at edges and stops at gaps", *body)


def spectrogram_windows():
    n, size, hop = 26, 8, 4
    px = lambda i: 60 + i * 15
    t = np.arange(n)
    wave = 52 + 14 * np.sin(t / 1.6) * np.cos(t / 7)
    body = [text(20, 26, "spectrogram(x, y, window_size=8, overlap=4)", "mono")]
    body += [polyline(zip(px(t), wave), "line")]
    body += [dot(px(i), wave[i], "dot-grey", 2.5) for i in t]
    colours = ["blue", "orange", "blue", "orange", "blue"]
    starts = range(0, n - size + 1, hop)
    for k, s in enumerate(starts):
        y = 84 + (k % 2) * 28
        colour = colours[k % len(colours)]
        body += [rect(px(s) - 6, y, px(s + size - 1) - px(s) + 12, 20, colour, 4)]
        body += [line(px(s + size // 2), y - 2, px(s + size // 2), y + 22, f"stroke-{colour}")]
    body += [bracket(px(0) - 6, px(size - 1) + 6, 160, "window_size samples", below=True)]
    body += [bracket(px(hop) - 6, px(size - 1) + 6, 196, "overlap", below=True)]
    body += [text(px(0), 238, "Each window gives one spectrum, stamped at its centre "
                  "sample (the vertical tick).", "ts")]
    body += [text(px(0), 254, "hop = window_size − overlap.  The default, None, "
                  "means half a window.", "ts")]

    gx, gy, cell = 520, 70, 16
    n_windows, n_freq = len(starts), size // 2 + 1
    body += [line(px(n - 1) + 14, 110, gx - 10, 110, "arrow")]
    for k in range(n_windows):
        for f in range(n_freq):
            shade = ["#dbeafe", "#bfdbfe", "#93c5fd", "#60a5fa", "#3b82f6"][(k + 2 * f) % 5]
            body += [f'<rect x="{gx + k * cell}" y="{gy + (n_freq - 1 - f) * cell}" width="{cell - 1}" '
                     f'height="{cell - 1}" fill="{shade}"/>']
    body += [text(gx + n_windows * cell / 2, gy + n_freq * cell + 18, "one spectrum per window →", "ts c")]
    body += [text(gx + n_windows * cell + 8, gy + 10, "↑ window_size/2 + 1", "ts")]
    body += [text(gx + n_windows * cell + 8, gy + 24, "   frequencies", "ts")]
    return svg(270, "Spectrogram windows, overlap and centre times", *body)


def layout_and_reduce():
    body = [text(20, 30, "Arrays", "tb")]
    rows, cols, cw, ch = 5, 3, 46, 22
    x0, y0 = 150, 60
    body += [rect(60, y0, 60, rows * ch, "grey", 2)]
    body += [text(90, y0 - 10, "x", "mono c"), text(x0 + cols * cw / 2, y0 - 10, "y", "mono c")]
    for r in range(rows):
        body += [text(90, y0 + r * ch + 15, f"t{r}", "ts c")]
        for c in range(cols):
            body += [rect(x0 + c * cw, y0 + r * ch, cw - 2, ch - 2, ["blue", "orange", "green"][c], 2)]
    body += [text(x0 + cols * cw / 2, y0 + rows * ch + 20, "shape (n, k): one row per time,", "ts c")]
    body += [text(x0 + cols * cw / 2, y0 + rows * ch + 34, "one column per component", "ts c")]
    body += [text(90, y0 + rows * ch + 20, "shape (n,)", "ts c")]
    body += [text(90, y0 + rows * ch + 34, "float64 s", "ts c")]

    body += [line(x0 + cols * cw + 16, y0 + rows * ch / 2, 520, y0 + rows * ch / 2, "arrow")]
    body += [text(412, y0 + rows * ch / 2 - 10, "reduce(x, y, \"norm\")", "mono c")]
    body += [rect(530, y0, 60, rows * ch, "violet", 2)]
    body += [text(560, y0 - 10, "y_out", "mono c")]
    body += [text(560, y0 + rows * ch + 20, "shape (n,)", "ts c")]
    body += [text(608, y0 + 30, "one value per row:", "ts")]
    body += [text(608, y0 + 46, "sum, mean, min,", "ts")]
    body += [text(608, y0 + 62, "max, or norm", "ts")]
    body += [text(608, y0 + 78, "= √(Σ y²)", "ts")]
    return svg(220, "y is n rows by k columns; reduce combines the columns of each row", *body)


def reduce_axes():
    energies, angles, cw = 4, 3, 34
    x0, y0 = 60, 92
    colours = ["blue", "orange", "green", "violet"]
    body = [text(20, 30, "reduce_axes(x, y, shape=(4, 3), axes=(1,))", "mono")]
    body += [text(x0, y0 - 30, "one row of y: 12 columns = 4 energies × 3 angles", "ts")]
    for e in range(energies):
        body += [text(x0 + (e * angles + 1.5) * cw - 1.5, y0 - 8, f"energy {e}", f"ts c")]
        for a in range(angles):
            i = e * angles + a
            body += [rect(x0 + i * cw, y0, cw - 3, 30, colours[e], 3)]
            body += [text(x0 + i * cw + (cw - 3) / 2, y0 + 19, f"a{a}", "ts c")]
    out_x = x0 + 12 * cw + 58
    body += [line(x0 + 12 * cw + 6, y0 + 15, out_x - 8, y0 + 15, "arrow")]
    body += [text((x0 + 12 * cw + out_x) / 2, y0 + 46, "Σ angles", "ts c")]
    for e in range(energies):
        body += [rect(out_x + e * 52, y0, 49, 30, colours[e], 3)]
        body += [text(out_x + e * 52 + 24, y0 + 19, f"e{e}", "ts c")]
    body += [text(out_x, y0 + 50, "4 columns", "ts")]
    body += [text(20, 174, "Columns are read in C order: the last axis changes fastest. "
                  "Every row is reduced the same way.", "ts")]
    return svg(194, "reduce_axes sums a 4 by 3 row over its angle axis", *body)


def box(x, y, w, h, cls, title, *lines, title_cls="tb"):
    """A box with a bold title and smaller centred lines under it."""
    cx = x + w / 2
    top = y + h / 2 - 8 * len(lines) + 5
    return "\n".join([rect(x, y, w, h, cls), text(cx, top, title, f"c {title_cls}"),
                       *(text(cx, top + 18 + 16 * i, s, "ts c") for i, s in enumerate(lines))])


def arrow(x1, y1, x2, y2, label="", dx=0, dy=-6):
    body = [line(x1, y1, x2, y2, "arrow")]
    if label:
        body.append(text((x1 + x2) / 2 + dx, (y1 + y2) / 2 + dy, label, "ts c"))
    return "\n".join(body)


def architecture():
    body = [box(20, 20, 350, 46, "green", "Your Python code", "PySide6 apps, SciQLop, notebooks"),
            box(390, 20, 350, 46, "orange", "Your web page", "JavaScript"),
            arrow(195, 66, 195, 86), arrow(565, 66, 565, 86),
            box(20, 86, 350, 46, "grey", "SciQLopPlots package", "Python bindings (Shiboken6)"),
            box(390, 86, 350, 46, "grey", "sciqlopplots.mjs", "JavaScript module + WebAssembly build"),
            arrow(195, 132, 195, 156), arrow(565, 132, 565, 156)]
    body += [rect(20, 156, 720, 206, "blue"), text(36, 178, "C++ library", "tb text-blue")]
    body += [box(40, 192, 210, 154, "grey", "Panel", "SciQLopMultiPlotPanel",
                 "stacks plots, shares the time", "axis, aligns their margins,", "takes drag and drop"),
             arrow(250, 269, 284, 269, "holds", dy=-8),
             box(284, 192, 210, 154, "grey", "Plot", "SciQLopPlot", "SciQLopTimeSeriesPlot",
                 "SciQLopNDProjectionPlot", "axes, legend, colour scale"),
             arrow(494, 231, 528, 231), arrow(494, 307, 528, 307),
             box(528, 192, 192, 72, "grey", "Plottables", "lines, curves, colour maps,", "histograms, waterfalls"),
             box(528, 274, 192, 72, "grey", "Items", "spans, tracers, lines,", "text, shapes, pictures"),
             text(511, 222, "holds", "ts c")]
    body += [arrow(380, 362, 380, 382),
             box(20, 382, 720, 50, "violet", "NeoQCP", "drawing engine, a fork of QCustomPlot: axes, layers, "
                 "resampling on worker threads, GPU compositing"),
             box(20, 448, 720, 50, "grey", "Qt 6", "desktop (Linux, macOS, Windows) or browser (WebAssembly)")]
    return svg(514, "Architecture: Python or JavaScript on top of the C++ library, which draws with NeoQCP on Qt 6", *body)


def data_flow():
    lanes = [("Main thread", 46), ("Callback thread", 136), ("Resampling", 226)]
    body = []
    for name, y in lanes:
        body += [rect(20, y, 720, 74, "grey"), text(34, y + 42, name, "tb")]
    mid = lambda lane: lanes[lane][1] + 37
    w = 176
    steps = [(0, 160, "blue", "1. You pan or zoom", "the visible range changes"),
             (1, 340, "green", "2. Your function runs", "get_data(start, stop)"),
             (2, 520, "violet", "3. Points reduced", "min and max per pixel bin"),
             (0, 548, "blue", "4. Drawn", "replaces the last image")]
    for lane, x, cls, title, sub in steps:
        body.append(box(x, mid(lane) - 26, w, 52, cls, title, sub))
    body += [arrow(160 + w - 30, mid(0) + 26, 340 + 30, mid(1) - 26),
             arrow(340 + w - 30, mid(1) + 26, 520 + 30, mid(2) - 26),
             arrow(520 + w / 2 + 40, mid(2) - 26, 548 + w / 2 + 40, mid(0) + 26)]
    body += [text(380, 26, "Static data skips step 2. Until step 4, the plot keeps showing the last image.", "ts c")]
    return svg(316, "Data flow: a pan or zoom calls your function on its thread, the points are reduced on "
               "worker threads, and the main thread draws them", *body)


DIAGRAMS = {
    "architecture.svg": architecture,
    "data-flow.svg": data_flow,
    "dsp-gaps.svg": gaps,
    "dsp-rolling-window.svg": rolling_window,
    "dsp-spectrogram-windows.svg": spectrogram_windows,
    "dsp-layout.svg": layout_and_reduce,
    "dsp-reduce-axes.svg": reduce_axes,
}

if __name__ == "__main__":
    for name, draw in DIAGRAMS.items():
        (IMAGES / name).write_text(draw())
        print(IMAGES / name)
