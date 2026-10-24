"""Verify ordered call candidate indexes and original-scan fallbacks through real stdio."""
import argparse
import hashlib
import json
from pathlib import Path
from check_lsp_module_semantics import source, initialize
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(server): sha(server), str(Path(__file__)): sha(__file__)}
    cases, clients = [], []
    report = dict(status='failed', scope=__doc__, cases=cases, protected=protected)
    try:
        for opened in [False, True]:
            folder = output/('opened' if opened else 'disk')
            folder.mkdir()
            entry, left, right = [folder/n for n in ['main.xl', 'left.xl', 'right.xl']]
            main = ('import left\nimport right\nfunc main(...) as int\n'
                    ' return left.compute(3)\nend\n')
            entry.write_bytes(main.encode())
            protected[str(entry)] = sha(entry)
            client = AutomaticClient(server, folder/'absent/xl.exe', folder)
            clients.append(client)
            initialize(client, entry, [entry, left, right], folder)
            if opened:
                client.open(entry, main, 1)
            prefix = source(40, 0, False).split('export func compute(')[0]

            def body(value, marker='completionL'):
                return (prefix + 'export func compute(int value) as int\r\n'
                        f' int edited = {value}\r\n auto completionLocal = right.compute(3)\r\n'
                        f' return {marker}\r\nend\r\n')

            def done(name, result):
                cases.append(dict(name=('opened-' if opened else 'disk-')+name,
                                  passed=True, analysis=result['analysis']))

            expect_index = True

            def complete(path, text, version, marker='completionL', available=True):
                client.request += 1
                at = text.rindex(marker)+len(marker)
                client.send('textDocument/completion', dict(textDocument=dict(uri=path.as_uri()),
                    version=version, position=position(text, at)), client.request)
                reply, _ = client.reply(client.request)
                assert 'error' not in reply, reply
                result = reply['result']
                assert result['version'] == version and result['available'] == available, result
                work = result['analysis']['lastBuildSemantics']
                assert all(isinstance(work[k], int) and work[k] >= 0
                           for k in ['checked', 'reused', 'contextHits', 'bytes']), work
                calls = result['analysis']['lastBuildCallCandidates']
                assert all(isinstance(calls[k], int) and calls[k] >= 0 for k in
                           ['indexes', 'rootVisits', 'indexedQueries', 'linearQueries', 'candidateVisits', 'bytes']), calls
                if expect_index:
                    assert calls['indexes'] == 1 and calls['indexedQueries'] > 0 and not calls['linearQueries'], calls
                    assert calls['candidateVisits'] <= calls['indexedQueries'] * 2 and calls['bytes'] > 0, calls
                else:
                    assert not calls['indexes'] and not calls['indexedQueries'] and calls['linearQueries'] > 0, calls
                if available:
                    assert [i['label'] for i in result['items']] == ['completionLocal'], result
                    item = result['items'][0]
                    assert item['data']['declaration']['uri'] == path.as_uri(), item
                    assert item['textEdit'] == dict(newText='completionLocal', range=dict(
                        start=position(text, at-len(marker)), end=position(text, at))), item
                else:
                    assert not result['items'], result
                return result

            text, version = body(0), 7
            client.open(left, text, version)
            client.open(right, source(40, 1, False), 7)
            result = complete(left, text, version)
            assert not result['analysis']['lastBuildSemantics']['reused'], result
            done('initial-check-seeds-owned-other-source-facts', result)
            for i in range(12):
                marker = ['completionL', 'completionLo', 'completionLoc'][i % 3]
                updated = body(i+1, marker)
                begin, end = text.index(' int edited ='), text.rindex('\r\nend')
                version += 1
                client.send('textDocument/didChange', dict(textDocument=dict(uri=left.as_uri(), version=version),
                    contentChanges=[dict(range=dict(start=position(text, begin), end=position(text, end)),
                                         text=updated[begin:updated.rindex('\r\nend')])]))
                text = updated
                result = complete(left, text, version, marker)
                work = result['analysis']['lastBuildSemantics']
                assert work['contextHits'] == 1 and work['reused'] >= 40 and work['checked'] >= 42, result
            done('twelve-real-range-edits-reuse-only-other-sources', result)
            version += 1
            client.change(left, text, version)
            result = complete(left, text, version, marker)
            assert result['analysis']['cacheHits'] == 1, result
            done('exact-hit-restamps-version', result)
            client.send('textDocument/didClose', dict(textDocument=dict(uri=right.as_uri())))
            client.open(right, source(40, 1, False), 1)
            result = complete(left, text, version, marker)
            assert not result['analysis']['lastBuildSemantics']['reused'], result
            done('reopened-dependency-cannot-borrow-old-memo', result)
            # An unrelated long declaration declines the optional index without
            # reducing language coverage or changing the projected candidate.
            long_text = text + 'func ' + 'z' * 300 + '() as int\r\n return 0\r\nend\r\n'
            version += 1
            client.change(left, long_text, version)
            expect_index = False
            result = complete(left, long_text, version, marker)
            done('long-declaration-falls-back-to-original-root-scan', result)
            version += 1
            client.change(left, text, version)
            expect_index = True
            result = complete(left, text, version, marker)
            done('declaration-change-rebuilds-index-with-current-names', result)
            expect_index = False
            client.change(right, 'export func compute(int value) as string\n return "changed"\nend\n', 2)
            result = complete(left, text, version, marker)
            assert 'string' in result['items'][0]['detail'], result
            assert not result['analysis']['lastBuildSemantics']['reused'], result
            done('dependency-interface-change-updates-current-candidate-type', result)
            broken = body(200, 'missing.completionL')
            version += 1
            client.change(left, broken, version)
            result = complete(left, broken, version, available=False)
            done('invalid-receiver-withdraws-candidates', result)
            text = body(201)
            version += 1
            client.change(left, text, version)
            result = complete(left, text, version)
            done('projection-recovery-preserves-type-checks', result)
            client.change(left, 'export func compute(int value) as int\n return value\nend\n', version+1)
            right_body = 'export func compute(int value) as int\n int completionLocal = value\n return completionL\nend\n'
            client.change(right, right_body, 3)
            result = complete(right, right_body, 3)
            assert not result['analysis']['lastBuildSemantics']['reused'], result
            done('query-source-change-discards-previous-query-memo', result)
            client.stop()
            assert client.process.returncode == 0
        assert len(cases) == 20 and all(sha(p) == h for p, h in protected.items())
        report['status'] = 'passed'
    finally:
        for i, client in enumerate(clients):
            client.stop()
            (output/f'transcript-{i}.json').write_text(json.dumps(client.transcript, ensure_ascii=False, indent=2), encoding='utf8')
        (output/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(dict(status=report['status'], cases=len(cases), proof=str(output/'result.json'))))


if __name__ == '__main__':
    main()
