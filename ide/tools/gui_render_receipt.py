"""Validate the final bounded-render receipt after asynchronous work settles."""
import re
import json
import os
from pathlib import Path


def rendered_at_least(text, minimum):
    # --frames is a minimum: the workbench also waits for catalog/file-sync jobs.
    rows = re.findall(r'^xlang IDE: rendered ([0-9]+) frames; windows=([0-9]+) panes=([0-9]+); C and xlang registered\r?$', text, re.M)
    return len(rows) == 1 and int(rows[0][0]) >= minimum and all(int(value) > 0 for value in rows[0][1:])


def runtime_font(runtime):
    fonts = sorted(p for p in (Path(runtime) / 'data/fonts').iterdir() if p.suffix.lower() in ('.ttf', '.otf', '.ttc') and p.is_file())
    if not fonts:
        raise ValueError('The tested runtime has no bundled font')
    return next((p for p in fonts if p.name == 'Consolas-with-Yahei_Nerd_Font.ttf'), fonts[0]).name


def preferences_with_recent_only(path, before, allowed_paths):
    """Opening a file persists history; every other preference stays identical."""
    value = json.loads(Path(path).read_text(encoding='utf-8'))
    recent = value.pop('recent', [])
    expected = {k: v for k, v in before.items() if k != 'recent'}
    assert value == expected, 'A non-history preference changed'
    def key(p):
        return os.path.normcase(str(Path(p).resolve()).removeprefix('\\\\?\\'))
    allowed = {key(p) for p in allowed_paths}
    seen = set()
    for row in recent:
        assert set(row) == {'path', 'accessedAt'} and type(row['accessedAt']) is int and row['accessedAt'] > 0
        canonical = key(row['path'])
        assert canonical in allowed and canonical not in seen, 'History escaped the fixture inputs'
        seen.add(canonical)
    assert recent, 'The actual opened file was not recorded'
