"""按 toml-test 1.0 清单检查语法、预期值及写回值。"""

from datetime import date, datetime, time
import json
import math
from pathlib import Path
import subprocess
import sys
import tomllib


def canonical(value):
    if isinstance(value, dict):
        return ("table", tuple(sorted((key, canonical(item))
                                      for key, item in value.items())))
    if isinstance(value, list):
        return ("array", tuple(canonical(item) for item in value))
    if isinstance(value, bool):
        return ("bool", value)
    if isinstance(value, int):
        return ("int", value)
    if isinstance(value, float):
        if math.isnan(value):
            return ("float", "nan")
        return ("float", value.hex())
    if hasattr(value, "isoformat"):
        return (type(value).__name__, value.isoformat())
    return (type(value).__name__, value)


def expected_value(value):
    if isinstance(value, list):
        return [expected_value(item) for item in value]
    if isinstance(value, dict):
        if set(value) == {"type", "value"}:
            kind, text = value["type"], value["value"]
            readers = {
                "bool": lambda s: s == "true",
                "integer": int,
                "float": float,
                "string": str,
                "datetime": lambda s: datetime.fromisoformat(s.replace("Z", "+00:00")),
                "datetime-local": datetime.fromisoformat,
                "date-local": date.fromisoformat,
                "time-local": time.fromisoformat,
            }
            if kind not in readers or (kind == "bool" and text not in ("true", "false")):
                raise ValueError(f"unknown expected TOML value: {kind}={text}")
            return readers[kind](text)
        return {key: expected_value(item) for key, item in value.items()}
    raise ValueError(f"unknown expected TOML node: {value!r}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: check_conformance.py PROBE TOML_TEST_DIR")
    probe, repo = Path(sys.argv[1]), Path(sys.argv[2])
    root = repo / "tests"
    cases = (root / "files-toml-1.0.0").read_text(encoding="utf-8").splitlines()
    valid = invalid = 0
    syntax_failures = []
    reference_failures = []
    value_failures = []
    for relative in cases:
        if not relative.endswith(".toml"):
            continue
        category = relative.split("/", 1)[0]
        if category not in ("valid", "invalid"):
            continue
        if category == "valid":
            valid += 1
        else:
            invalid += 1
        try:
            path = root / relative
            args = [str(probe)]
            if category == "valid":
                args.append("--roundtrip")
            result = subprocess.run(args + [str(path)], capture_output=True,
                                    timeout=10, check=False)
            matched = result.returncode == (0 if category == "valid" else 1)
            if not matched:
                syntax_failures.append((relative, result.returncode,
                                        result.stderr.decode("utf-8", errors="replace").strip()))
            elif category == "valid":
                try:
                    expected = canonical(expected_value(json.loads(
                        path.with_suffix(".json").read_text(encoding="utf-8"))))
                    original = canonical(tomllib.loads(path.read_text(encoding="utf-8")))
                    written = canonical(tomllib.loads(result.stdout.decode("utf-8")))
                    if original != expected:
                        reference_failures.append((relative, "tomllib differs from expected JSON"))
                    if written != expected:
                        value_failures.append((relative, "written value differs from expected JSON"))
                except (ValueError, UnicodeError, OSError) as error:
                    value_failures.append((relative, str(error)))
        except subprocess.TimeoutExpired:
            syntax_failures.append((relative, "timeout", ""))
    print(f"TOML 1.0 syntax: {valid} valid, {invalid} invalid, "
          f"{len(syntax_failures)} mismatches; "
          f"reference: {len(reference_failures)} mismatches; "
          f"semantic roundtrip: {len(value_failures)} mismatches")
    for relative, status, detail in syntax_failures[:20]:
        print(f"{relative}: {status} {detail}", file=sys.stderr)
    for relative, detail in value_failures[:20]:
        print(f"{relative}: {detail}", file=sys.stderr)
    for relative, detail in reference_failures[:20]:
        print(f"{relative}: {detail}", file=sys.stderr)
    if syntax_failures or reference_failures or value_failures:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
