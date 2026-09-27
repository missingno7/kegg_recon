"""Stress the timer oracle and game startup under concurrent host CPU load.

Run after building and exporting the oracle image::

    python port/tools/timer_load_check.py
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build", default=str(ROOT / "build" / "port"))
    ap.add_argument("--oracle-runs", type=int, default=50)
    ap.add_argument("--startup-runs", type=int, default=30)
    ap.add_argument("--startup-ms", type=int, default=5000)
    ap.add_argument("--burners", type=int, default=4)
    args = ap.parse_args()
    if min(args.oracle_runs, args.startup_runs, args.startup_ms, args.burners) < 1:
        ap.error("run counts, startup duration, and burner count must be positive")

    build = Path(args.build).resolve()
    oracle = build / "oracle" / "ke_oracle.exe"
    if not oracle.is_file():
        print(f"missing oracle executable: {oracle}", file=sys.stderr)
        return 2

    burn_code = "x=1\nwhile True:\n x=(x*1664525+1013904223)&0xffffffff\n"
    burners: list[subprocess.Popen[bytes]] = []
    failures = 0
    try:
        for _ in range(args.burners):
            burners.append(subprocess.Popen([sys.executable, "-c", burn_code],
                                            stdin=subprocess.DEVNULL,
                                            stdout=subprocess.DEVNULL,
                                            stderr=subprocess.DEVNULL))
        print(f"CPU load active: {len(burners)} burner processes", flush=True)

        for run in range(1, args.oracle_runs + 1):
            result = subprocess.run([str(oracle), str(build / "oracle")],
                                    capture_output=True, text=True, errors="replace")
            summary = result.stdout.strip().splitlines()[-1:] or ["no oracle summary"]
            if result.returncode:
                failures += 1
                print(f"oracle {run}/{args.oracle_runs}: FAIL ({result.returncode})", flush=True)
                print(result.stdout)
                print(result.stderr)
                break
            print(f"oracle {run}/{args.oracle_runs}: {summary[0]}", flush=True)

        if failures:
            return 1

        smoke = ROOT / "port" / "tools" / "smoke.py"
        for run in range(1, args.startup_runs + 1):
            result = subprocess.run([sys.executable, str(smoke), "--build", str(build),
                                     "--timer-diag", "--ms", str(args.startup_ms)],
                                    capture_output=True, text=True, errors="replace")
            diagnostic = next((line for line in result.stdout.splitlines()
                               if line.startswith("timer diagnostic:")), "diagnostic missing")
            if result.returncode:
                failures += 1
                print(f"startup {run}/{args.startup_runs}: FAIL ({result.returncode})", flush=True)
                print(result.stdout)
                print(result.stderr)
                break
            print(f"startup {run}/{args.startup_runs}: {diagnostic}", flush=True)
    finally:
        for burner in burners:
            burner.terminate()
        for burner in burners:
            burner.wait()

    if failures:
        return 1
    print(f"LOAD CHECK OK: {args.oracle_runs}/{args.oracle_runs} oracle runs, "
          f"{args.startup_runs}/{args.startup_runs} timer-enabled starts", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
