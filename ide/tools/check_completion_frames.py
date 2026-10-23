"""Measure editor input through real application-frame render submission."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def distribution(values):
    ordered = sorted(values)
    return {"samples": len(ordered), "p50": statistics.median(ordered),
            "p95": ordered[math.ceil(.95 * len(ordered)) - 1], "maximum": max(ordered)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--compiler", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--samples", type=int, default=30, help="samples per mode and round")
    parser.add_argument("--rounds", type=int, default=2, help="alternate auto/forced order each round")
    parser.add_argument("--bytes", type=int, nargs="+", default=[8192])
    parser.add_argument("--max-render-p95-ms", type=float, help="optional explicit performance gate")
    args = parser.parse_args()
    if not 1 <= args.samples <= 120 or not 1 <= args.rounds <= 8 or any(
            n < 128 or n > 7 * 1024 * 1024 for n in args.bytes):
        parser.error("samples 1..120, rounds 1..8, bytes 128..7340032 are required")
    if len(set(args.bytes)) != len(args.bytes):
        parser.error("duplicate byte sizes are not allowed")
    if args.max_render_p95_ms is not None and (
            not math.isfinite(args.max_render_p95_ms) or args.max_render_p95_ms <= 0):
        parser.error("performance gate must be a finite positive number")
    fixture = args.fixture.resolve()
    compiler = args.compiler.resolve()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    assets = [fixture, fixture.parent / "xlang-ls.exe", fixture.parent / "xge.dll"]
    protected = {str(p): digest(p) for p in assets}
    evidence = {
        "status": "running", "unit": "ms", "scope":
        "Real xideAppFrame and on-demand game loop with production window/VSYNC/high-DPI flags. "
        "Character and Ctrl+Space inputs enter real editor APIs, with pending events dispatched between "
        "inputs as in the arrival-ordered platform bridge; hotkey intent is checked before polling. "
        "No inner wait/sleep/poll loop. "
        "Every result checks native backend, current document identity/version, exact whole-token range, "
        "complete source and candidate. Measurement ends after application surface render/present "
        "submission, before compositor/physical display. Private desktop, hidden window, warm native "
        "server, one local candidate, ordinary background views retained. Document preparation and "
        "first-sample screenshot readback are outside measured spans. Each preparation alternates a local "
        "declaration name; typed R must produce the matching fresh candidate. The declaration-name edit "
        "is outside the measured span. GUI CPU includes fixture checks "
        "and excludes child servers. This baseline does not prove changed-function semantic incrementality, "
        "cold startup, full-engine memory, all document sizes or hardware input-to-photon latency. "
        "Host is shared and not exclusive.",
        "fixture": str(fixture), "compiler": str(compiler),
        "compilerExists": compiler.is_file(), "protected": protected,
        "host": {"platform": platform.platform(), "machine": platform.machine(),
                 "processor": platform.processor(), "logicalCpus": os.cpu_count()},
        "runs": [], "summary": [], "maxRenderP95Ms": args.max_render_p95_ms,
        "targetPassed": None,
    }
    report = root / "result.json"

    def save():
        report.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    save()
    try:
        for size in args.bytes:
            for round_index in range(args.rounds):
                name = f"bytes-{size}-round-{round_index}"
                output = root / name
                command = [str(fixture), str(output), str(compiler), str(args.samples),
                           str(size), str(round_index % 2)]
                process = subprocess.run(command, cwd=fixture.parent, capture_output=True, timeout=660)
                log = root / (name + ".log")
                log.write_bytes(process.stdout + process.stderr)
                stdout = process.stdout.decode("utf-8", errors="replace")
                samples = [json.loads(line) for line in stdout.splitlines() if line.startswith("{")]
                run = {"command": command, "exit": process.returncode, "log": str(log),
                       "logSha256": digest(log), "requestedBytes": size,
                       "round": round_index, "order": ["auto", "forced"] if round_index % 2 == 0
                       else ["forced", "auto"], "samples": samples,
                       "images": {str(p): digest(p) for p in output.glob("mode-*.png")}}
                evidence["runs"].append(run)
                save()
                if process.returncode or "completion_frame_test passed:" not in stdout:
                    raise RuntimeError(f"frame fixture failed; inspect {log}")
                for mode in (0, 1):
                    rows = [s for s in samples if s["mode"] == mode]
                    if sorted(s["sample"] for s in rows) != list(range(args.samples)):
                        raise RuntimeError("missing/duplicate mode samples")
                    if len({s["bytes"] for s in rows}) != 1 or not size - 64 <= rows[0]["bytes"] <= size:
                        raise RuntimeError("unexpected actual document size")
                    for row in rows:
                        if row["candidate"] not in ("zzRecent", "zzResult") or row["documentVersion"] < 1:
                            raise RuntimeError("invalid fresh candidate/version evidence")
                        for metric in ("inputMs", "renderSubmittedMs", "maxFrameMs", "totalFrameMs",
                                       "maxCallbackGapMs", "guiCpuMs", "frames", "dpiScale"):
                            if not math.isfinite(row[metric]) or row[metric] < 0:
                                raise RuntimeError("invalid performance sample")
                for index, sample in enumerate(samples):
                    expected = "zzRecent" if index % 2 == 0 else "zzResult"
                    if sample["candidate"] != expected or sample["dpiScale"] <= 0:
                        raise RuntimeError("candidate/scale corpus mismatch")
                    if index and sample["documentVersion"] <= samples[index - 1]["documentVersion"]:
                        raise RuntimeError("non-increasing document version")
                if len(samples) != 2 * args.samples or len(run["images"]) != 2:
                    raise RuntimeError("incomplete frame samples or screenshots")
                print(name, "verified", len(samples), "frame samples", flush=True)
        for size in args.bytes:
            for mode in (0, 1):
                rows = [s for r in evidence["runs"] if r["requestedBytes"] == size
                        for s in r["samples"] if s["mode"] == mode]
                evidence["summary"].append({"requestedBytes": size, "bytes": rows[0]["bytes"],
                    "mode": "auto" if mode == 0 else "forced", "metrics": {
                        metric: distribution([s[metric] for s in rows]) for metric in
                        ("inputMs", "renderSubmittedMs", "maxFrameMs", "totalFrameMs",
                         "maxCallbackGapMs", "guiCpuMs", "frames")}})
        if any(digest(p) != value for p, value in protected.items()):
            raise RuntimeError("fixture/server/SDK changed during measurement")
        evidence["status"] = "measured"
        if args.max_render_p95_ms is not None:
            evidence["targetPassed"] = all(s["metrics"]["renderSubmittedMs"]["p95"] <=
                                           args.max_render_p95_ms for s in evidence["summary"])
            if not evidence["targetPassed"]:
                evidence["status"] = "performance-gate-failed"
        save()
        print(json.dumps({"status": evidence["status"], "summary": evidence["summary"],
                          "evidence": str(report)}, ensure_ascii=False), flush=True)
        return 1 if evidence["targetPassed"] is False else 0
    except (OSError, RuntimeError, ValueError, TypeError, KeyError, subprocess.TimeoutExpired) as error:
        evidence["status"] = "failed"
        evidence["error"] = str(error)
        save()
        print(str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
