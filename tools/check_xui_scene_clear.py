"""Verify removed widgets leave background pixels, while new widgets still draw."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--runtime', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    runtime = (args.runtime or root / 'release' / ('win_x64' if os.name == 'nt' else 'linux_x64')).resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    sha = lambda path: hashlib.sha256(path.read_bytes()).hexdigest()
    fixture = root / 'test/fixtures/xui_learning/scene_replacement.xl'
    helper = root / 'lib/bindings/xui/examples/_window.xl'
    cli = runtime / ('xl.exe' if os.name == 'nt' else 'xl')
    raw = args.out.resolve() / 'frame.rgba'
    env = dict(os.environ, XLANG_PACKAGE_PATH=str(runtime / 'extlibs'),
               XUI_EXAMPLE_FRAMES='8', XUI_EXAMPLE_DUMP=str(raw))
    env.pop('XUI_EXAMPLE_CYCLE', None)
    command = [str(cli), 'run', str(fixture)]
    inputs = {str(path): sha(path) for path in (fixture, helper, cli, runtime / 'extlibs/xui/src/managed.xl')}
    start = time.monotonic()
    result = subprocess.run(command, cwd=runtime, env=env, capture_output=True, timeout=180)
    log = result.stdout + result.stderr
    (args.out / 'run.log').write_bytes(log)
    dimensions = re.search(rb'^RGBA (\d+) (\d+)$', log, re.M)
    old_pixel = None
    new_pixels = 0
    if result.returncode == 0 and dimensions and raw.is_file():
        width, height = map(int, dimensions.groups())
        pixels = raw.read_bytes()
        assert len(pixels) == width * height * 4
        background = bytes((244, 246, 250, 255))
        offset = (140 * width + 200) * 4
        old_pixel = list(pixels[offset:offset + 4])
        new_pixels = sum(pixels[(y * width + x) * 4:(y * width + x + 1) * 4] != background
                         for y in range(24, 68) for x in range(24, 144))
    passed = (result.returncode == 0 and old_pixel == [244, 246, 250, 255]
              and new_pixels > 100 and all(sha(Path(path)) == digest for path, digest in inputs.items()))
    report = dict(passed=passed, command=command, exitCode=result.returncode,
                  seconds=round(time.monotonic() - start, 3), inputs=inputs,
                  removedScenePixel=old_pixel, newWidgetPixelCount=new_pixels,
                  logSha256=hashlib.sha256(log).hexdigest(), networkTests=False)
    (args.out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Scene replacement pixel regression:', passed, old_pixel, new_pixels)
    if not passed:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
