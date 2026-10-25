"""Compare the C JSON Patch implementation with python-json-patch 1.35."""

import json
import random
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
DEPS = ROOT / "build" / "test" / "json_patch_oracle_deps"
if DEPS.is_dir():
    sys.path.insert(0, str(DEPS))

try:
    import jsonpatch
except ImportError as error:
    raise SystemExit(
        "Install test-only dependency: python -m pip install --target "
        "demo6/build/test/json_patch_oracle_deps jsonpatch==1.35"
    ) from error
if jsonpatch.__version__ != "1.35":
    raise SystemExit("json.patch oracle requires python-json-patch 1.35")


def value(rng, depth):
    if depth and rng.randrange(3):
        if rng.randrange(2):
            return [value(rng, depth - 1) for _ in range(rng.randrange(4))]
        keys = ["plain", "a/b", "tilde~", "你好", "0"]
        rng.shuffle(keys)
        return {key: value(rng, depth - 1) for key in keys[: rng.randrange(5)]}
    return rng.choice([None, True, False, rng.randrange(-100, 101), "", "你好", "a/b~c"])


def main(executable):
    rng = random.Random(0x6902)
    cases = []
    for _ in range(160):
        source = value(rng, 3)
        target = value(rng, 3)
        patch = jsonpatch.make_patch(source, target).patch
        expected = jsonpatch.apply_patch(source, patch)
        cases.append((source, patch, expected))

    explicit = [
        ({"a": [1, 2, 3]}, [{"op": "move", "from": "/a/0", "path": "/a/2"}]),
        ({"a/b": {"~": 4}}, [{"op": "copy", "from": "/a~1b/~0", "path": "/copy"}]),
        ({"a": [1, 2]}, [{"op": "test", "path": "/a/1", "value": 2}]),
        ({"a": [1, 2]}, [{"op": "add", "path": "/a/-", "value": {"b": 3}}]),
    ]
    for source, patch in explicit:
        cases.append((source, patch, jsonpatch.apply_patch(source, patch)))

    encoded = "".join(json.dumps(case, ensure_ascii=False, separators=(",", ":")) + "\n" for case in cases)
    result = subprocess.run([executable], input=encoded, text=True, encoding="utf-8", capture_output=True)
    if result.returncode:
        print(result.stderr, file=sys.stderr, end="")
        try:
            failed = int(result.stderr.split("case ", 1)[1].split(":", 1)[0]) - 1
            print(json.dumps(cases[failed], ensure_ascii=False), file=sys.stderr)
        except (IndexError, ValueError):
            pass
        return result.returncode
    print(result.stdout, end="")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: oracle.py <oracle_cases.exe>")
    raise SystemExit(main(sys.argv[1]))
