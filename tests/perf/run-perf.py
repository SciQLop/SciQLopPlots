#!/usr/bin/env python3
"""
Perf profiling wrapper for SciQLopPlots multi-plot benchmarks.

The benchmark script uses raise(SIGSTOP) after warmup so perf only
captures the hot loop. This script launches the benchmark, waits for
it to stop, then attaches perf and sends SIGCONT.

    # Record a specific scenario:
    python3 run-perf.py synced_pan

    # Record all scenarios:
    python3 run-perf.py --all

    # Just perf stat (no recording):
    python3 run-perf.py --stat synced_pan

    # Custom iterations:
    python3 run-perf.py synced_pan --iters 50

    # Generate report from existing perf.data:
    python3 run-perf.py --report synced_pan

Output goes to results/<scenario>.perf.data (and .stat.txt for stat).
"""

import argparse
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
RESULTS_DIR = SCRIPT_DIR / "results"

BENCHMARKS = {
    "multiplot": {
        "script": SCRIPT_DIR / "multiplot-perf.py",
        "scenarios": ["synced_pan", "y_zoom", "full_replot", "panel_setup", "callable_pan"],
    },
    "colormap": {
        "script": SCRIPT_DIR / "colormap-perf.py",
        "scenarios": ["synced_pan", "y_rescale", "full_replot", "panel_setup", "callable_pan"],
    },
}

# Flat list of all scenarios (prefixed with benchmark name for disambiguation)
ALL_SCENARIOS = []
SCENARIO_LOOKUP = {}  # scenario_name -> (script, scenario_name)
for bname, binfo in BENCHMARKS.items():
    for s in binfo["scenarios"]:
        prefixed = f"{bname}/{s}"
        ALL_SCENARIOS.append(prefixed)
        SCENARIO_LOOKUP[prefixed] = (binfo["script"], s)
        # Also allow unprefixed if unambiguous
        if s not in SCENARIO_LOOKUP:
            SCENARIO_LOOKUP[s] = (binfo["script"], s)
        else:
            # Ambiguous — require prefix
            SCENARIO_LOOKUP[s] = None

# Legacy flat list for --all
SCENARIOS = list(ALL_SCENARIOS)


def wait_for_stop(pid, timeout=120):
    """Wait until process enters stopped state (T) via /proc."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            with open(f"/proc/{pid}/status") as f:
                for line in f:
                    if line.startswith("State:"):
                        if "T (stopped)" in line or "t (tracing stop)" in line:
                            return True
                        break
        except FileNotFoundError:
            return False
        time.sleep(0.05)
    return False


def resolve_scenario(name):
    """Resolve scenario name to (script_path, scenario_name)."""
    if name in SCENARIO_LOOKUP:
        result = SCENARIO_LOOKUP[name]
        if result is None:
            # Ambiguous — list options
            options = [k for k in ALL_SCENARIOS if k.endswith(f"/{name}")]
            sys.exit(f"Ambiguous scenario '{name}'. Use: {', '.join(options)}")
        return result
    sys.exit(f"Unknown scenario: {name}\nAvailable: {', '.join(ALL_SCENARIOS)}")


def build_bench_cmd(script, scenario, iters):
    cmd = [sys.executable, str(script), scenario]
    if iters:
        cmd.append(str(iters))
    return cmd


def run_stat(scenario, iters):
    """Run perf stat — attach after warmup barrier."""
    script, scene_name = resolve_scenario(scenario)
    label = scenario.replace("/", "_")
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    out_file = RESULTS_DIR / f"{label}.stat.txt"

    cmd = build_bench_cmd(script, scene_name, iters)

    print(f"\n{'='*60}")
    print(f"perf stat: {scenario}" + (f" ({iters} iters)" if iters else ""))
    print(f"{'='*60}")

    bench = subprocess.Popen(cmd, stderr=subprocess.PIPE, text=True)
    if not wait_for_stop(bench.pid):
        bench.wait()
        print(bench.stderr.read(), file=sys.stderr)
        sys.exit("Benchmark did not reach barrier")

    perf_cmd = [
        "perf", "stat",
        "-e", "cycles,instructions,cache-references,cache-misses,"
              "branches,branch-misses,L1-dcache-load-misses,"
              "LLC-load-misses,task-clock",
        "-p", str(bench.pid),
    ]
    perf = subprocess.Popen(perf_cmd, stderr=subprocess.PIPE, text=True)

    time.sleep(0.1)
    os.kill(bench.pid, signal.SIGCONT)

    bench.wait()
    bench_output = bench.stderr.read()
    print(bench_output, file=sys.stderr)

    perf.wait()
    perf_output = perf.stderr.read()
    print(perf_output)

    with open(out_file, "w") as f:
        f.write(f"# Scenario: {scenario}\n")
        if iters:
            f.write(f"# Iterations: {iters}\n")
        f.write(bench_output)
        f.write(perf_output)
    print(f"  → saved to {out_file}")


def run_record(scenario, iters, call_graph, frequency):
    """Run perf record — attach after warmup barrier."""
    script, scene_name = resolve_scenario(scenario)
    label = scenario.replace("/", "_")
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    data_file = RESULTS_DIR / f"{label}.perf.data"

    cmd = build_bench_cmd(script, scene_name, iters)

    print(f"\n{'='*60}")
    print(f"perf record: {scenario}" + (f" ({iters} iters)" if iters else ""))
    print(f"  call-graph: {call_graph or 'default'}, freq: {frequency} Hz")
    print(f"{'='*60}")

    bench = subprocess.Popen(cmd, stderr=subprocess.PIPE, text=True)
    if not wait_for_stop(bench.pid):
        bench.wait()
        print(bench.stderr.read(), file=sys.stderr)
        sys.exit("Benchmark did not reach barrier")

    perf_cmd = [
        "perf", "record",
        "-o", str(data_file),
        "-F", str(frequency),
        "-p", str(bench.pid),
    ]
    if call_graph:
        perf_cmd += ["--call-graph", call_graph]
    perf = subprocess.Popen(perf_cmd, stderr=subprocess.PIPE, text=True)

    time.sleep(0.1)
    os.kill(bench.pid, signal.SIGCONT)

    bench.wait()
    bench_output = bench.stderr.read()
    print(bench_output, file=sys.stderr)

    perf.wait()
    print(perf.stderr.read(), file=sys.stderr)
    print(f"  → saved to {data_file}")
    return data_file


def run_report(scenario):
    """Generate text report from perf.data."""
    label = scenario.replace("/", "_")
    data_file = RESULTS_DIR / f"{label}.perf.data"
    if not data_file.exists():
        print(f"No perf data found: {data_file}")
        print(f"Run: python3 {__file__} {scenario}")
        return

    report_file = RESULTS_DIR / f"{label}.report.txt"

    result = subprocess.run(
        ["perf", "report", "-i", str(data_file),
         "--stdio", "--no-children", "--percent-limit", "0.5"],
        capture_output=True, text=True,
    )
    print(result.stdout[:5000])

    with open(report_file, "w") as f:
        f.write(result.stdout)
    print(f"\n  → full report saved to {report_file}")

    callee_file = RESULTS_DIR / f"{label}.callers.txt"
    result2 = subprocess.run(
        ["perf", "report", "-i", str(data_file),
         "--stdio", "--children", "--percent-limit", "1.0"],
        capture_output=True, text=True,
    )
    with open(callee_file, "w") as f:
        f.write(result2.stdout)
    print(f"  → caller/callee report saved to {callee_file}")


def main():
    parser = argparse.ArgumentParser(
        description="Perf profiling wrapper for SciQLopPlots multi-plot benchmarks",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("scenario", nargs="?", help="Scenario to profile")
    parser.add_argument("--all", action="store_true", help="Run all scenarios")
    parser.add_argument("--stat", action="store_true",
                        help="Use perf stat instead of perf record")
    parser.add_argument("--report", action="store_true",
                        help="Generate report from existing perf.data")
    parser.add_argument("--iters", type=int, default=None,
                        help="Override iteration count")
    parser.add_argument("--call-graph", default="dwarf",
                        choices=["dwarf", "fp", "lbr"],
                        help="Call-graph recording method (default: dwarf)")
    parser.add_argument("--frequency", type=int, default=4999,
                        help="Sampling frequency in Hz (default: 4999)")
    args = parser.parse_args()

    if not args.scenario and not args.all:
        parser.print_help()
        print(f"\nAvailable scenarios: {', '.join(ALL_SCENARIOS)}")
        sys.exit(1)

    targets = ALL_SCENARIOS if args.all else [args.scenario]

    if args.report:
        for t in targets:
            run_report(t)
        return

    for t in targets:
        if args.stat:
            run_stat(t, args.iters)
        else:
            run_record(t, args.iters, args.call_graph, args.frequency)
            run_report(t)


if __name__ == "__main__":
    main()
