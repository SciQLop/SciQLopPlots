"""Demo gallery — lists all available demos and launches the selected one."""
import subprocess
import sys
import os

DEMOS = [
    ("Line Graph", "basics/line_graph.py", "Basic 3-component line plot with lazy data loading"),
    ("Colormap", "basics/colormap.py", "Uniform and non-uniform colormap examples"),
    ("Vertical Spans", "basics/vertical_spans.py", "Interactive spans with reactive tooltips"),
    ("Parametric Curve", "basics/parametric_curve.py", "Butterfly curve with decorative items"),
    ("Pipeline", "advanced/pipeline.py", "Reactive pipeline: span→tooltip, data→FFT"),
    ("Stacked Plots", "advanced/stacked_plots.py", "Multi-plot panel with time sync and multi-span"),
    ("Drag & Drop", "advanced/drag_and_drop.py", "Custom drag-drop callback for plot creation"),
    ("Products", "advanced/products.py", "Hierarchical product tree browser"),
    ("Projections", "advanced/projections.py", "3D parametric curves (requires speasy)"),
    ("Stress Test", "stress/stress_model.py", "Random create/destroy panels for stability"),
    ("Profiling", "stress/profiling.py", "Real-time data + FFT spectrogram"),
    ("MMS Data", "real_data/micro_sciqlop.py", "MMS spacecraft data (requires speasy)"),
]


def main():
    base = os.path.dirname(os.path.abspath(__file__))

    print("SciQLopPlots Demo Gallery")
    print("=" * 40)
    for i, (name, _, desc) in enumerate(DEMOS, 1):
        print(f"  {i:2d}. {name:20s} — {desc}")
    print(f"  {'q':>3s}. Quit")
    print()

    while True:
        choice = input("Select demo number (or 'q' to quit): ").strip()
        if choice.lower() == "q":
            break
        try:
            idx = int(choice) - 1
            if 0 <= idx < len(DEMOS):
                name, path, _ = DEMOS[idx]
                full_path = os.path.join(base, path)
                print(f"\nLaunching: {name}")
                subprocess.run([sys.executable, full_path])
            else:
                print(f"Invalid choice. Enter 1-{len(DEMOS)}.")
        except ValueError:
            print("Enter a number or 'q'.")


if __name__ == "__main__":
    main()
