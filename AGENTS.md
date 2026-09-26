# Working rules (humans and Codex workers)

Goal: source that, built with the historical Watcom toolchain, reproduces `assets/KE.EXE` exactly.
Read README.md and docs/evidence.md first.
Run Python tools as `.\kpy.cmd tools/X.py ...` (PyPy; `python` is a Store alias that hangs in sandboxes).
Do not pip install anything (no network); capstone is vendored in build/pylib.

- `assets/` holds the immutable originals. `manifest.json` is the only status file. The verifier decides
  what is true; nobody edits expected bytes or weakens a check to fit a candidate. If a check looks wrong,
  prove it with independent evidence and report it instead of working around it.
- Run historical tools only through `tools/dosrun.py` (pinned, hash-checked installs under C:/tools).
  Never use a modern compiler as an authority. Open Watcom 2.0 (`C:/tmp/watcom`) may be used only as an
  inspection aid (wdis/dmpobj), never to produce reconstruction output.
- Workers write only inside `build/workers/NAME/` plus any file the task explicitly assigns to them.
  Do not edit `manifest.json`, `src/`, `asm/`, or existing tools unless assigned; describe needed changes.
  The supervisor promotes verified results.
- No raw-byte fallbacks, byte-emitting `db`/inline-asm tricks in C, absolute-address casts to force
  bytes, or patched outputs. Hand-written assembly stays assembly only where the original was assembly.
- Keep original facts, hypotheses and tool limitations distinct (PROVEN / STRONG / HYPOTHESIS).
- Iterate as long as you make measurable progress; there are no attempt quotas. Stop and escalate when
  progress stalls, the evidence points at a global toolchain/verifier issue, or you need context outside
  your task.
- Put bulky output in files under your worker dir. Final answer: <=20 dense lines — task, exact range,
  hypotheses tested, result, best candidate path, exact remaining mismatch, files changed, verifier
  command + result, safe to promote?, new global clue/blocker.
