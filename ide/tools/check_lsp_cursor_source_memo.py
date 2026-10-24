"""Verify query-source semantic reuse with current marker metadata over real LSP."""
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
    server, output = args.server.resolve(strict=True), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(server): sha(server), str(Path(__file__)): sha(__file__)}
    cases, clients = [], []
    report = dict(status='failed', scope=__doc__, cases=cases, protected=protected)
    try:
        for mode in ['closed', 'opened', 'standalone']:
            folder = output/mode
            folder.mkdir()
            entry, left, right = [folder/n for n in ['main.xl', 'member_left.xl', 'member_right.xl']]
            main = ('import member_left\nimport member_right\nfunc main(...) as int\n'
                    ' return member_left.compute(3)+member_right.compute(4)\nend\n')
            if mode != 'standalone':
                entry.write_bytes(main.encode())
                protected[str(entry)] = sha(entry)
            client = AutomaticClient(server, folder/'absent/xl.exe', folder)
            clients.append(client)
            if mode == 'standalone':
                client.initialize()
            else:
                initialize(client, entry, [entry, left, right], folder)
                if mode == 'opened':
                    client.open(entry, main, 1)
                client.open(right, source(32, 1, False), 1)
            prefix = source(32, 0, False).split('export func compute(')[0]

            def body(i):
                header = 'func main(...) as int' if mode == 'standalone' else 'export func compute(int value) as int'
                return (prefix + header + '\r\n' + ' // 中文😀 moving marker\r\n'*(i % 4) +
                        f' string 中文 = "😀-{i}"\r\n int completionLocal = {i}\r\n'
                        ' return completionLocal\r\nend\r\n')

            def done(name, result):
                cases.append(dict(name=mode+'-'+name, passed=True, analysis=result['analysis']))

            def complete(text, version, offset, label='completionLocal', available=True):
                client.request += 1
                client.send('textDocument/completion', dict(textDocument=dict(uri=left.as_uri()),
                    version=version, position=position(text, offset)), client.request)
                reply, _ = client.reply(client.request)
                assert 'error' not in reply, reply
                result = reply['result']
                assert result['version'] == version and result['available'] == available, result
                if available:
                    assert [i['label'] for i in result['items']] == [label], result
                    assert result['items'][0]['data']['declaration']['uri'] == left.as_uri(), result
                else:
                    assert not result['items'], result
                assert result['analysis']['lastBuildSemantics']['checked'] >= 1, result
                annotations = result['analysis']['lastBuildAnnotations']
                storage = result['analysis']['lastBuildStorage']
                materialization = result['analysis']['lastBuildMaterialization']
                assert materialization['nodes'] > materialization['pages'] > 0, result
                assert 0 < materialization['chargedBytes'] <= 64*1024*1024, result
                assert materialization['pooledCopies'] == materialization['calls'] > 0, result
                assert not materialization['heapCopies'] and not materialization['poolFallbacks'], result
                assert storage['chargedBytes'] <= 64*1024*1024, result
                if result['analysis']['lastBuildSemantics']['reused']:
                    assert storage['nodes'] > storage['pages'] > 0 and storage['chargedBytes'] > 0, result
                assert not annotations['materialized'] and annotations['retainedBytes'] <= 8*1024*1024, result
                if available:
                    assert annotations['deferred'] == 1 and annotations['retainedBytes'] > 0, result
                else:
                    assert not annotations['deferred'] and not annotations['retainedBytes'], result
                return result

            version, text = 7, body(0)
            client.open(left, text, version)
            offset = text.rindex('completionLocal')+len('completionL')
            result = complete(text, version, offset)
            assert not result['analysis']['lastBuildSemantics']['reused'], result
            done('seed-immutable-original-marker-units', result)
            for i in range(1, 25):
                updated = body(i)
                begin, end = text.index(' // 中文') if ' // 中文' in text else text.index(' string 中文'), text.rindex('\r\nend')
                new_begin = updated.index(' // 中文') if ' // 中文' in updated else updated.index(' string 中文')
                version += 1
                client.send('textDocument/didChange', dict(textDocument=dict(uri=left.as_uri(), version=version),
                    contentChanges=[dict(range=dict(start=position(text, begin), end=position(text, end)),
                                         text=updated[new_begin:updated.rindex('\r\nend')])]))
                text = updated
                offset = text.rindex('completionLocal')+len('completionL')+i % 3
                result = complete(text, version, offset)
                work = result['analysis']['lastBuildSemantics']
                assert work['contextHits'] == 1 and work['reused'] >= (32 if mode == 'standalone' else 64), result
                assert work['checked']+work['reused'] == (34 if mode == 'standalone' else 69), result
                item = result['items'][0]
                start = text.rindex('completionLocal')
                assert item['textEdit'] == dict(newText='completionLocal', range=dict(
                    start=position(text, start), end=position(text, start+len('completionLocal')))), item
            done('twenty-four-marker-moves-reuse-query-source-functions', result)
            # The active function had an immutable unit before this caret move.
            helper = text.index('func work_7(')
            target = text.index('return local', helper)+len('return loc')
            result = complete(text, version, target, 'local')
            done('cached-sibling-becomes-original-checked-projection', result)
            result = complete(text, version, offset)
            done('return-to-original-query-with-multi-generation-marker-stamps', result)
            version += 1
            client.change(left, text, version)
            result = complete(text, version, offset)
            assert result['analysis']['cacheHits'] >= 1, result
            done('same-body-hit-keeps-current-version-and-last-build-work', result)
            moved = '\ufeff// before all 中文😀\r\n'+text.lstrip('\ufeff')
            version += 1
            client.change(left, moved, version)
            offset = moved.rindex('completionLocal')+len('completionLoc')
            result = complete(moved, version, offset)
            done('wide-insertion-preserves-current-source-coordinates', result)
            client.send('textDocument/didClose', dict(textDocument=dict(uri=left.as_uri())))
            version = 1
            client.open(left, moved, version)
            result = complete(moved, version, offset)
            assert not result['analysis']['lastBuildSemantics']['reused'], result
            done('reopen-discards-previous-document-identity', result)
            bad = moved.replace('return completionLocal', 'return missing.completionLocal')
            version += 1
            client.change(left, bad, version)
            complete(bad, version, bad.rindex('completionLocal')+len('completionLoc'), available=False)
            version += 1
            client.change(left, moved, version)
            result = complete(moved, version, offset)
            done('invalid-receiver-withdrawal-and-current-marker-recovery', result)
            for i in range(3):
                header = 'func main(...) as int' if mode == 'standalone' else 'export func compute(int value) as int'
                dot = (prefix+'export class Box\r\n int member\r\nend\r\n'+header+'\r\n'+
                       ' // point moves 中文😀\r\n'*i+' auto box = Box()\r\n return box.\r\nend\r\n')
                version += 1
                client.change(left, dot, version)
                point = dot.rindex('return box.')+len('return box.')
                result = complete(dot, version, point, 'member')
                assert result['items'][0]['textEdit']['range'] == dict(start=position(dot, point), end=position(dot, point)), result
                if i:
                    assert result['analysis']['lastBuildSemantics']['reused'] >= (32 if mode == 'standalone' else 64), result
            done('empty-dot-marker-moves-with-current-source-binding', result)
            client.stop()
            assert client.process.returncode == 0
        assert len(cases) == 27 and all(sha(p) == h for p, h in protected.items())
        report['status'] = 'passed'
    finally:
        for i, client in enumerate(clients):
            client.stop()
            (output/f'transcript-{i}.json').write_text(json.dumps(client.transcript, ensure_ascii=False, indent=2), encoding='utf8')
        (output/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(dict(status=report['status'], cases=len(cases), proof=str(output/'result.json'))))


if __name__ == '__main__':
    main()
