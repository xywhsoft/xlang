"""Use Python colorsys and the WCAG sRGB definition, not the package's C math."""
import colorsys
import json
import math
import pathlib
import random
import subprocess
import sys


def luminance(rgb):
    normalized = [channel / 255 for channel in rgb]
    linear = [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in normalized]
    return sum(a * b for a, b in zip(linear, (0.2126, 0.7152, 0.0722)))


def main():
    executable, output = sys.argv[1:]
    rng = random.Random(20260928)
    commands, expected = [], []
    colors = [(i, i, i) for i in range(256)]
    colors += [(r, g, b) for r in (0, 1, 127, 128, 254, 255)
               for g in (0, 1, 127, 128, 254, 255) for b in (0, 1, 127, 128, 254, 255)]
    colors += [tuple(rng.randrange(256) for _ in range(3)) for _ in range(4096)]
    for rgb in colors:
        normalized = tuple(x / 255 for x in rgb)
        h, s, v = colorsys.rgb_to_hsv(*normalized)
        lightness = colorsys.rgb_to_hls(*normalized)[1]
        brightness = luminance(rgb)
        commands.append("r #%02X%02X%02X" % rgb)
        expected.append(("scalar", (h * 360, s, v, lightness, brightness)))
        commands.append(f"v {h * 360:.17g} {s:.17g} {v:.17g}")
        expected.append(("roundtrip", rgb))
    parameters = [(h, s, v) for h in (-720, -120, 0, 60, 120, 240, 360, 720)
                  for s in (-2, 0, 0.5, 1, 2) for v in (-2, 0, 0.5, 1, 2)]
    parameters += [(rng.uniform(-1e6, 1e6), rng.uniform(-0.2, 1.2), rng.uniform(-0.2, 1.2)) for _ in range(4096)]
    for h, s, v in parameters:
        hue = math.fmod(h, 360)
        if hue < 0:
            hue += 360
        saturation, value = max(0, min(1, s)), max(0, min(1, v))
        for operation in ("v", "l"):
            rgb = (colorsys.hsv_to_rgb(hue / 360, saturation, value) if operation == "v"
                   else colorsys.hls_to_rgb(hue / 360, value, saturation))
            commands.append(f"{operation} {h:.17g} {s:.17g} {v:.17g}")
            expected.append(("conversion", tuple(math.floor(x * 255 + 0.5) for x in rgb)))
    # 测试自行计算亮度与插值，不从待测 C 实现生成期望结果。
    fixed = [
        ("#000", "#fff", (0, 0, 0), (255, 255, 255)),
        ("#0000", "#ffff", (0, 0, 0), (255, 255, 255)),
        ("#12345678", "#abcdef00", (18, 52, 86), (171, 205, 239)),
        ("#808080", "#7f7f7f", (128, 128, 128), (127, 127, 127)),
        ("#ff0000", "#00ff00", (255, 0, 0), (0, 255, 0)),
    ]
    pairs = list(fixed)
    for _ in range(4096):
        a = tuple(rng.randrange(256) for _ in range(3))
        b = tuple(rng.randrange(256) for _ in range(3))
        pairs.append(("#%02X%02X%02X" % a, "#%02X%02X%02X" % b, a, b))
    for index, (first, second, rgb_a, rgb_b) in enumerate(pairs):
        a, b = sorted((luminance(rgb_a), luminance(rgb_b)))
        commands.append(f"c {first} {second}")
        expected.append(("contrast", (b + 0.05) / (a + 0.05)))
        factors = (-1, 0, 0.125, 0.5, 0.875, 1, 2) if index < len(fixed) else (rng.uniform(-0.5, 1.5),)
        for factor in factors:
            commands.append(f"m {first} {second} {factor:.17g}")
            t = max(0, min(1, factor))
            expected.append(("mix", tuple(math.floor(x + (y - x) * t + 0.5)
                                          for x, y in zip(rgb_a, rgb_b))))
    # 固定 W3C 数据夹具，不从待测 C 表反向生成期望值，也不在日常测试时联网。
    fixture = pathlib.Path(__file__).resolve().parents[1] / "release/extlibs/color/test/css_named_colors.json"
    named = json.loads(fixture.read_text(encoding="utf-8"))["colors"]
    assert len(named) == 148
    canonical = {}
    for name in sorted(named):
        canonical.setdefault(named[name], name)
    for name, value in named.items():
        for variant in (name, name.upper(), name.title()):
            commands.append("n " + variant)
            expected.append(("name", value))
        commands.append("h " + value)
        expected.append(("name", canonical[value]))
    result = subprocess.run([executable], input="\n".join(commands) + "\n", capture_output=True,
                            text=True, encoding="ascii", timeout=90)
    if result.returncode:
        raise RuntimeError(f"oracle driver exited {result.returncode}: {result.stderr}")
    lines = result.stdout.splitlines()
    assert len(lines) == len(expected), (len(lines), len(expected))
    for i, (line, (kind, target)) in enumerate(zip(lines, expected)):
        if kind == "scalar":
            actual = tuple(map(float, line.split()))
            ok = len(actual) == 5 and all(math.isclose(x, y, rel_tol=1e-12, abs_tol=1e-12) for x, y in zip(actual, target))
        elif kind == "contrast":
            actual = float(line)
            ok = math.isclose(actual, target, rel_tol=1e-12, abs_tol=1e-12)
        elif kind == "name":
            actual = line
            ok = actual == target
        else:
            assert len(line) == 7 and line[0] == "#", (i, line)
            actual = tuple(int(line[n:n+2], 16) for n in (1, 3, 5))
            # Different valid floating implementations can straddle an exact half-byte boundary.
            tolerance = 1 if kind == "conversion" else 0
            ok = all(abs(x - y) <= tolerance for x, y in zip(actual, target))
        assert ok, (i, commands[i], actual, target)
    counts = {kind: sum(t == kind for t, _ in expected) for kind in
              ("scalar", "roundtrip", "conversion", "contrast", "mix", "name")}
    pathlib.Path(output).write_text(json.dumps({"passed": len(expected), "counts": counts,
        "seed": 20260928, "reference": "Python colorsys; WCAG sRGB luminance and contrast; independent RGB mix; W3C CSS named-color fixture",
        "conversionChannelTolerance": 1, "roundtripTolerance": 0, "mixChannelTolerance": 0}, indent=2) + "\n", encoding="utf-8")
    print(f"color independent oracle: {len(expected)} checks PASS")


if __name__ == "__main__":
    main()
