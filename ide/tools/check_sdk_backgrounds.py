"""Verify native BACKGROUND indicator pixels without changing user files."""
import argparse
import json
from pathlib import Path
import subprocess
from PIL import Image, ImageChops, ImageDraw


def check(fixture, root):
    root.mkdir(parents=True, exist_ok=False)
    command = [str(fixture.resolve()), str((root / 'documents').resolve())]
    run = subprocess.run(command, cwd=fixture.resolve().parent, capture_output=True, timeout=180)
    (root / 'stdout.log').write_bytes(run.stdout)
    (root / 'stderr.log').write_bytes(run.stderr)
    if run.returncode:
        raise RuntimeError(f'Native fixture exited {run.returncode}: {run.stderr.decode(errors="replace")}')
    rows = [json.loads(s) for s in run.stdout.decode('utf-8').splitlines() if s.startswith('{')]
    assert {(r['scene'], r['theme']) for r in rows} == {(s, t) for s in range(4) for t in range(2)}
    checks = []
    for row in rows:
        prefix = f"scene{row['scene']}-theme{row['theme']}-"
        images = {s: Image.open(root / 'documents' / (prefix+s+'.png')).convert('RGB')
                  for s in ('base', 'marked', 'duplicate', 'cleared')}
        delta = ImageChops.difference(images['base'], images['marked'])
        assert delta.getbbox(), prefix+' no visible background'
        assert not ImageChops.difference(images['marked'], images['duplicate']).getbbox(), prefix+' duplicate overdraw'
        assert not ImageChops.difference(images['base'], images['cleared']).getbbox(), prefix+' incomplete clear'
        mask = Image.new('L', images['base'].size, 0)
        draw = ImageDraw.Draw(mask)
        for x, y, w, h in row['rects']:
            rect = (int(x)-1, int(y)-1, int(x+w)+1, int(y+h)+1)
            draw.rectangle(rect, fill=255)
            assert delta.crop(rect).getbbox(), prefix+' missing glyph background'
        outside = ImageChops.multiply(delta, Image.merge('RGB', [ImageChops.invert(mask)]*3))
        assert not outside.getbbox(), prefix+' pixels changed outside target glyphs'
        checks.append(dict(scene=row['scene'], theme=row['theme'], bounds=delta.getbbox(), glyphs=len(row['rects'])))
    result = dict(status='passed', command=command, observations=rows, pixels=checks,
                  scope='Native forced redraw: Unicode/tab, folding, horizontal scroll, wrapped vertical scroll, two themes; duplicate union, clear, model/find/diagnostic preservation. No language request scheduling.')
    (root / 'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(f'SDK background pixels passed: {len(checks)} scenes; {root}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    check(args.fixture, args.output)
