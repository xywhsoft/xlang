"""Real native completion during dependency body errors, without an executor."""
import argparse
import hashlib
import json
from pathlib import Path
from check_lsp_automatic_diagnostics import AutomaticClient


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    report = dict(status='failed', serverSha256=sha(server), cases=[])
    body = 'import dep\nfunc main(...) as int\n return dep.value()\nend\n'
    good = 'export func value() as int\n return 7\nend\n'
    bad = 'export func value() as int\n return "bad"\nend\n'
    header = 'export func value(MissingType arg) as int\n return 7\nend\n'
    entry, dep = root/'main.xl', root/'dep.xl'
    entry.write_text(body, encoding='utf8')
    dep.write_text(bad, encoding='utf8')
    client = AutomaticClient(server, root/'no-executor/xl.exe', root)

    def completion(expected):
        result = client.call('textDocument/completion', dict(
            textDocument=dict(uri=entry.as_uri()),
            position=dict(line=2, character=15), version=1))
        assert result['available'] == expected, result
        assert result['checkedCompilation'] is False, result
        if expected:
            assert [x['label'] for x in result['items']] == ['value'], result
            assert result['items'][0]['detail'] == 'func() as int', result
        else:
            assert not result['items'] and result['error'], result
        return result

    def checked_dependency():
        stats = client.quiet()
        diagnostics = client.latest.get(dep.as_uri(), {}).get('diagnostics', [])
        assert diagnostics, (stats, client.events)
        return diagnostics

    def done(name, **data):
        report['cases'].append(dict(name=name, passed=True, **data))
        print('PASS', name, flush=True)

    try:
        client.send('initialize', dict(
            capabilities=dict(textDocument=dict(publishDiagnostics=dict(versionSupport=True))),
            initializationOptions=dict(checkOnSave=False)), 1)
        client.reply(1)
        client.send('initialized', {})
        client.open(entry, body)
        diagnostics = checked_dependency()
        value = completion(True)
        done('disk-body-error-keeps-current-completion-and-diagnostics',
             result=value, diagnostics=diagnostics)
        cached = completion(True)
        assert cached['analysis']['cacheHits'] > value['analysis']['cacheHits'], cached
        done('exact-error-input-query-can-hit-cache', result=cached)

        client.open(dep, header, 7)
        checked_dependency()
        done('unsaved-invalid-header-invalidates-prior-completion', result=completion(False))
        client.change(dep, bad, 8)
        checked_dependency()
        done('unsaved-body-repair-restores-semantic-completion', result=completion(True))
        signature = client.call('textDocument/signatureHelp', dict(
            textDocument=dict(uri=entry.as_uri()), position=dict(line=2, character=18), version=1))
        assert signature and signature['signatures'], signature
        done('signature-help-survives-dependency-body-error', result=signature)

        client.change(dep, good, 9)
        client.quiet()
        assert not client.latest[dep.as_uri()]['diagnostics'], client.events
        done('full-repair-clears-only-real-error', result=completion(True))
        client.change(dep, bad, 10)
        diagnostics = checked_dependency()
        assert client.latest[dep.as_uri()]['version'] == 10
        done('recurring-body-error-keeps-candidates-at-latest-overlay',
             result=completion(True), diagnostics=diagnostics)

        client.send('textDocument/didClose', dict(textDocument=dict(uri=dep.as_uri())))
        client.quiet()
        done('close-overlay-revalidates-real-disk-body-error', result=completion(True))
        assert dep.read_text(encoding='utf8') == bad
        assert entry.read_text(encoding='utf8') == body
        assert sha(server) == report['serverSha256']
        report['status'] = 'passed'
    finally:
        report['stderr'] = client.stop()
        report['transcript'] = client.transcript
        (root/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')


if __name__ == '__main__':
    main()
