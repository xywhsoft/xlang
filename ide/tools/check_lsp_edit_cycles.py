"""Validate repeated unsaved dependency edits against a real xlang-ls process."""

import argparse
import hashlib
import json
from pathlib import Path
import time

from check_language_snapshots import Client


ENTRY = 'import dep\nfunc main(...) as int\n return dep.value()\nend\n'
DEPENDENCY = 'export func value() as int\n return {}\nend\n'
VALID = '7'
INVALID = '"错😀"'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def change_literal(client, path, version, before, after):
    first = len(' return ')
    last = first + len(before.encode('utf-16-le')) // 2
    client.send('textDocument/didChange', {
        'textDocument': {'uri': path.as_uri(), 'version': version},
        'contentChanges': [{
            'range': {
                'start': {'line': 1, 'character': first},
                'end': {'line': 1, 'character': last},
            },
            'text': after,
        }],
    })


def accept_session(server, compiler, root, cycles, report):
    root.mkdir()
    entry, dependency = root / 'main.xl', root / 'dep.xl'
    entry.write_text(ENTRY, encoding='utf8', newline='\n')
    dependency.write_text(DEPENDENCY.format(VALID), encoding='utf8', newline='\n')
    protected = {path: digest(path) for path in (entry, dependency)}
    client = Client(server, compiler, root)
    try:
        client.initialize('native')
        client.open(entry, ENTRY)
        client.open(dependency, DEPENDENCY.format(VALID))
        prior = VALID
        for index in range(cycles):
            literal = INVALID if index % 2 == 0 else VALID
            version = index + 2
            change_literal(client, dependency, version, prior, literal)
            prior = literal

            started = time.monotonic()
            request = 2 + index * 2
            client.check(entry, 1, id=request)
            response, _ = client.reply(request)
            checked = response['result']
            diagnostics = checked['diagnostics']
            if literal == INVALID:
                assert not checked['ok'] and checked['count'] == 1, checked
                assert Path(diagnostics[0]['file']).resolve() == dependency, diagnostics
            else:
                assert checked['ok'] and not diagnostics, checked

            client.send('textDocument/completion', {
                'textDocument': {'uri': entry.as_uri()},
                'position': {'line': 2, 'character': 15},
                'version': 1,
            }, request + 1)
            response, _ = client.reply(request + 1)
            completion = response['result']
            assert completion['available'] is True and completion['version'] == 1, completion
            assert [(item['label'], item['detail']) for item in completion['items']] == [
                ('value', 'func() as int'),
            ], completion
            report['cycles'].append({
                'session': root.name, 'dependencyVersion': version,
                'diagnostics': len(diagnostics), 'completion': 'value',
                'seconds': time.monotonic() - started,
            })

        client.send('shutdown', {}, 10001)
        reply, _ = client.reply(10001)
        assert 'result' in reply and reply['result'] is None, reply
        client.send('exit')
        client.process.stdin.close()
        assert client.process.wait(timeout=10) == 0
        assert all(digest(path) == value for path, value in protected.items())
    finally:
        (root / 'stderr.txt').write_text(client.stop(), encoding='utf8')
        (root / 'transcript.json').write_text(json.dumps(client.transcript), encoding='utf8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('server', 'compiler', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    server, compiler = (path.resolve(strict=True) for path in (args.server, args.compiler))
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    protected = {path: digest(path) for path in (server, compiler, Path(__file__))}
    report = {'status': 'failed', 'cycles': [], 'protected': {str(p): v for p, v in protected.items()}}
    try:
        for session in range(3):
            accept_session(server, compiler, output / f'session-{session + 1}', 32, report)
        assert all(digest(path) == value for path, value in protected.items())
        report['status'] = 'passed'
    finally:
        (output / 'result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(f"PASS {len(report['cycles'])} Unicode edit cycles across 3 sessions")


if __name__ == '__main__':
    main()
