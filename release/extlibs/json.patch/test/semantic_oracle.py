"""RFC 6902/7396独立语义参照，不依赖XRT相等函数或被测库输出计算预期。"""
import argparse
import copy
from fractions import Fraction
import json
from pathlib import Path
import random
import subprocess


def equal(left, right):
    if isinstance(left, bool) or isinstance(right, bool):
        return type(left) is type(right) and left == right
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        return Fraction(left) == Fraction(right)
    if type(left) is not type(right):
        return False
    if isinstance(left, list):
        return len(left) == len(right) and all(equal(a, b) for a, b in zip(left, right))
    if isinstance(left, dict):
        return left.keys() == right.keys() and all(equal(value, right[key]) for key, value in left.items())
    return left == right


def merge(target, patch):
    if not isinstance(patch, dict):
        return copy.deepcopy(patch)
    result = copy.deepcopy(target) if isinstance(target, dict) else {}
    for key, value in patch.items():
        if value is None:
            result.pop(key, None)
        else:
            result[key] = merge(result.get(key), value)
    return result


def cases():
    values = [None, False, True, -2, -1, 0, 1, 2, -0.0, -1.0, 1.0, -1.5, 1.5, 0.1,
              2**53 - 1, 2**53, 2**53 + 1, float(2**53), float(2**53 + 2),
              -(2**63), -(2**63) + 1, 2**63 - 1, 2**63, 2**64 - 1, float(2**63), float(2**64),
              "", "1", "true", "你好", "é", "e\u0301", "\0", [], {}, [1], {"k": 1},
              {"k": True}, {"k": [1, False, None]}, {"first": 1, "last": [False, None]},
              {"last": [False, None], "first": 1.0}, float.fromhex("0x0.0000000000001p-1022"),
              float.fromhex("0x1.fffffffffffffp+1023"), -float.fromhex("0x1.fffffffffffffp+1023")]
    result = []
    for left in values:
        for right in values:
            okay = equal(left, right)
            # 对象模式在test之前先修改另一字段，检查失败事务不能改变原文档。
            for mode in range(3):
                document = left
                operations = [{"op": "test", "path": "", "value": right, "ignored": 7}]
                wanted = left if okay else None
                if mode == 1:
                    document = [left, False]
                    operations[0]["value"] = [right, False]
                    wanted = document if okay else None
                elif mode == 2:
                    document = {"字段\0/~": left, "keep": [False, None]}
                    operations = [{"op": "replace", "path": "/keep", "value": [True, None]},
                                  {"op": "test", "path": "/字段\u0000~1~0", "value": right}]
                    wanted = {"keep": [True, None], "字段\0/~": left} if okay else None
                result.append((["T", document, operations], "ok" if okay else "value", wanted))
    # 所有JSON类别的根替换/删除/对象转换组合，不依赖第三方包。
    for document in values:
        for patch in values:
            result.append((["M", document, patch], "ok", merge(document, patch)))
    rng = random.Random(0x7396)

    def value(depth):
        if depth and rng.randrange(3):
            if rng.randrange(2):
                return [value(depth - 1) for _ in range(rng.randrange(4))]
            keys = ["keep", "erase", "字段\0/~", "nested", "", "é", "e\u0301"]
            rng.shuffle(keys)
            return {key: value(depth - 1) for key in keys[:rng.randrange(6)]}
        return copy.deepcopy(rng.choice(values))

    for _ in range(384):
        document, patch = value(4), value(4)
        result.append((["M", document, patch], "ok", merge(document, patch)))
    # 对象深度是本包的显式上限；非对象补丁仍是整个根值替换。
    for depth in (1, 31, 63, 64, 65):
        patch = {}
        for _ in range(depth - 1):
            patch = {"nested": patch}
        document = {"keep": 7}
        result.append((["M", document, patch], "ok" if depth <= 64 else "value",
                       merge(document, patch) if depth <= 64 else None))
    return result


def verify(vectors, output):
    lines = output.splitlines()
    if len(lines) != len(vectors):
        raise AssertionError("Incomplete independent semantic output")
    for index, ((request, status, wanted), line) in enumerate(zip(vectors, lines)):
        fields = line.split("\t")
        if len(fields) != 4:
            raise AssertionError((index, "invalid result framing"))
        actual, document, patch = (json.loads(field) for field in fields[1:])
        if fields[0] != status or not equal(actual, wanted) or not equal(document, request[1]) or not equal(patch, request[2]):
            raise AssertionError((index, request, status, wanted, fields))


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--cli", type=Path)
    group.add_argument("--xl", type=Path)
    parser.add_argument("--source", type=Path)
    args = parser.parse_args()
    if args.xl and not args.source:
        parser.error("--xl requires --source")
    vectors = cases()
    payload = "".join(json.dumps(request, ensure_ascii=False, allow_nan=False, separators=(",", ":")) + "\n"
                      for request, _, _ in vectors)
    command = [str(args.cli.resolve())] if args.cli else [str(args.xl.resolve()), "run", str(args.source.resolve())]
    process = subprocess.run(command, input=payload, capture_output=True, text=True, encoding="utf8", timeout=120)
    if process.returncode or process.stderr:
        raise AssertionError((process.returncode, process.stderr))
    verify(vectors, process.stdout)
    # 状态、布尔/数字类别、数组/对象类别均须被独立判据区分。
    for changed in (process.stdout.replace("ok\t", "value\t", 1),
                    process.stdout.replace("\ttrue\t", "\t1\t", 1),
                    process.stdout.replace("\t[]\t", "\t{}\t", 1)):
        if changed == process.stdout:
            raise AssertionError("Negative-control target not covered")
        try:
            verify(vectors, changed)
        except AssertionError:
            continue
        raise AssertionError("Corrupt semantic result accepted")
    tests = sum(request[0] == "T" for request, _, _ in vectors)
    print(f"json.patch independent semantics: {tests} test + {len(vectors) - tests} merge, 3 negative controls PASS")


if __name__ == "__main__":
    main()
