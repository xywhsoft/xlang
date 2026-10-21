"""Use Python colorsys and the WCAG sRGB definition, not the package's C math."""
import colorsys
import json
import math
import pathlib
import random
import subprocess
import sys


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
        linear = [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in normalized]
        luminance = sum(a * b for a, b in zip(linear, (0.2126, 0.7152, 0.0722)))
        commands.append("r #%02X%02X%02X" % rgb)
        expected.append(("scalar", (h * 360, s, v, lightness, luminance)))
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
        elif kind == "name":
            actual = line
            ok = actual == target
        else:
            assert len(line) == 7 and line[0] == "#", (i, line)
            actual = tuple(int(line[n:n+2], 16) for n in (1, 3, 5))
            # Different valid floating implementations can straddle an exact half-byte boundary.
            tolerance = 0 if kind == "roundtrip" else 1
            ok = all(abs(x - y) <= tolerance for x, y in zip(actual, target))
        assert ok, (i, commands[i], actual, target)
    counts = {kind: sum(t == kind for t, _ in expected) for kind in ("scalar", "roundtrip", "conversion", "name")}
    pathlib.Path(output).write_text(json.dumps({"passed": len(expected), "counts": counts,
        "seed": 20260928, "reference": "Python colorsys; WCAG sRGB luminance; W3C CSS named-color fixture",
        "conversionChannelTolerance": 1, "roundtripTolerance": 0}, indent=2) + "\n", encoding="utf-8")
    print(f"color independent oracle: {len(expected)} checks PASS")


if __name__ == "__main__":
    main()
