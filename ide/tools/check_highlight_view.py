"""Check actual language-server documentHighlight pixels in the IDE editor."""
import argparse
import json
from pathlib import Path
import subprocess
from PIL import Image, ImageChops, ImageDraw


def check(fixture, compiler, root):
    root.mkdir(parents=True, exist_ok=False)
    command = [str(fixture.resolve()), str((root/'documents').resolve()), str(compiler.resolve())]
    run = subprocess.run(command, cwd=fixture.resolve().parent, capture_output=True, timeout=180)
    (root/'stdout.log').write_bytes(run.stdout)
    (root/'stderr.log').write_bytes(run.stderr)
    if run.returncode:
        raise RuntimeError(f'Native highlight fixture exited {run.returncode}: {run.stderr.decode(errors="replace")}')
    rows = [json.loads(s) for s in run.stdout.decode('utf-8').splitlines() if s.startswith('{') and '"theme"' in s]
    assert len(rows) == 2 and {r['theme'] for r in rows} == {0, 1}
    checks = []
    for row in rows:
        name = 'dark' if row['theme'] else 'light'
        marked = Image.open(root/'documents'/f'highlight-{name}.png').convert('RGB')
        clear = Image.open(root/'documents'/f'clear-{name}.png').convert('RGB')
        delta = ImageChops.difference(marked, clear)
        mask = Image.new('L', marked.size, 0)
        draw = ImageDraw.Draw(mask)
        for x, y, w, h in row['rects']:
            rect = (x-1, y-1, x+w+1, y+h+1)
            draw.rectangle(rect, fill=255)
            assert delta.crop(rect).getbbox(), name+' missing actual semantic highlight pixels'
        outside = ImageChops.multiply(delta, Image.merge('RGB', [ImageChops.invert(mask)]*3))
        x, y, w, h = row['editor']
        assert not outside.crop((x, y, x+w, y+h)).getbbox(), name+' unrelated editor pixels changed'
        checks.append(dict(theme=name, ranges=len(row['rects']), bounds=delta.getbbox(), editor=row['editor']))
    result = dict(status='passed', command=command, observations=rows, pixels=checks,
                  scope='Actual native server and unsaved Unicode documentHighlight requests; two themes and editor pixel containment; independent find/diagnostic markers retained. Toolbar cancellation state is outside the editor comparison.')
    (root/'result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(f'IDE document highlights passed: {len(rows)} themes; {root}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    check(args.fixture, args.compiler, args.output)
