"""Verifier self-tests: negative controls must be rejected, positive controls accepted.

    python tests/run.py
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
NEG = [  # (file, func, start, end)
    ("wrong_const.c", "f_7032", "0x7032", "0x704d"),
    ("wrong_width.c", "f_7032", "0x7032", "0x704d"),
    ("wrong_symbol.c", "f_7032", "0x7032", "0x704d"),
    ("wrong_call.c", "f_2250", "0x2250", "0x228e"),
    ("wrong_string.c", "f_ca6a", "0xca6a", "0xcac2"),
    ("wrong_libname.c", "f_ddb9", "0xddb9", "0xde21"),  # close() where the original calls malloc
    ("wrong_irq_vec.asm", "a_0", "0x0", "0x69", "--profile", "game-asm", "--object", "2"),
    ("wrong_irq_call.asm", "a_0", "0x0", "0x69", "--profile", "game-asm", "--object", "2"),
]


def main():
    sys.path.insert(0, str(ROOT / "tools"))
    import json
    fails = 0
    for f, fn, s, e, *extra in NEG:
        out = ROOT / "build" / "tests" / (f + ".json")
        out.parent.mkdir(parents=True, exist_ok=True)
        p = subprocess.run([sys.executable, str(ROOT / "tools" / "check.py"), str(ROOT / "tests" / "neg" / f), fn,
                            "--at", s, "--end", e, "--json", str(out), *extra], capture_output=True, text=True)
        res = json.loads(out.read_text())
        rejected = res["verdict"] != "EXACT"
        print(("ok  " if rejected else "FAIL") + f"  {f}: {p.stdout.splitlines()[0]}")
        fails += not rejected
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
