"""Verify standard batched selection ranges over real stdio without a compiler."""
import argparse
import hashlib
import json
from pathlib import Path
import time

from check_language_snapshots import Client

SOURCE = ('func main(...) as int\n string label = "中文😀"\n return 27+1\nend\n')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def position(source, offset):
    prefix = source[:offset]
    return dict(line=prefix.count('\n'), character=len(prefix.rsplit('\n', 1)[-1].encode('utf-16-le'))//2)


def validate(rows, points, source):
    assert isinstance(rows, list) and len(rows) == len(points), rows
    end = position(source, len(source))
    for row, point in zip(rows, points):
        previous = None
        depth = 0
        while row:
            assert set(row) <= {'range', 'parent'} and 'range' in row, row
            value = row['range']
            assert set(value) == {'start', 'end'}, value
            a, b = [tuple(value[k][n] for n in ('line', 'character')) for k in ('start', 'end')]
            p = tuple(point[n] for n in ('line', 'character'))
            assert a <= p <= b, (row, point)
            if previous:
                assert a <= previous[0] and b >= previous[1] and (a, b) != previous
            previous = (a, b)
            row = row.get('parent')
            depth += 1
            assert depth <= 128
        assert previous == ((0, 0), tuple(end[n] for n in ('line', 'character')))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    entry, large = output/'入口😀.xl', output/'large.xl'
    entry.write_text('// preserved disk body\n', encoding='utf8')
    missing = output/'no-compiler'/'xl.exe'
    protected = {str(p): sha(p) for p in (server, entry, Path(__file__), Path(__file__).with_name('check_language_snapshots.py'))}
    client = Client(server, missing, output)
    sequence, cases, status = 10, [], 'failed'

    def request(method, params=None):
        nonlocal sequence
        sequence += 1
        client.send(method, params, sequence)
        message, _ = client.reply(sequence)
        return message

    def result(method, params=None):
        message = request(method, params)
        assert 'error' not in message, message
        return message['result']

    def params(points, path=entry, **extra):
        return dict(textDocument=dict(uri=path.as_uri()), positions=points, **extra)

    def select(points, source=SOURCE, path=entry, **extra):
        rows = result('textDocument/selectionRange', params(points, path, **extra))
        validate(rows, points, source)
        return rows

    def done(name, **details):
        cases.append(dict(name=name, passed=True, **details))

    try:
        cap = result('initialize', dict(capabilities={}, initializationOptions=dict(automaticDiagnostics=False, checkOnSave=False)))['capabilities']
        assert cap['selectionRangeProvider'] is True
        client.send('initialized', {})
        client.open(entry, SOURCE, 1)
        first = position(SOURCE, SOURCE.index('27')+1)
        points = [first, position(SOURCE, len(SOURCE)), first, dict(line=0, character=0)]
        rows = select(points)
        literal = dict(start=position(SOURCE, SOURCE.index('27')), end=position(SOURCE, SOURCE.index('27')+2))
        assert rows[0]['range'] == literal and rows[0] == rows[2]
        observed = result('xlang/analysisStats')
        assert observed['parses'] == 1 and observed['workerThreadId'] != observed['ownerThreadId']
        done('standard-capability-batch-order-duplicate-and-original-literal-bounds')
        for _ in range(12):
            assert select(points) == rows
        observed = result('xlang/analysisStats')
        assert observed['parses'] == 1 and observed['cacheHits'] >= 12
        assert result('textDocument/selectionRange', params([])) == []
        assert len(select([first]*256)) == 256
        done('retained-parser-empty-and-maximum-batch', parses=observed['parses'])
        emoji = position(SOURCE, SOURCE.index('😀'))
        assert select([emoji])
        bad = [None, {}, [dict(line=-1, character=0)], [dict(line=999, character=0)],
               [dict(line=0, character=1.5)], [dict(line=emoji['line'], character=emoji['character']+1)], [first]*257]
        for values in bad:
            assert request('textDocument/selectionRange', params(values))['error']['code'] == -32602
        assert request('textDocument/selectionRange', params([first], version=999))['error']['code'] == -32602
        assert request('textDocument/selectionRange', params([first], output/'closed.xl'))['error']['code'] == -32602
        done('invalid-arrays-signed-values-surrogate-interior-closed-and-stale-rejected')
        changed = '// shifted 😀\r\n'+SOURCE
        client.send('textDocument/didChange', dict(textDocument=dict(uri=entry.as_uri(), version=2), contentChanges=[
            dict(range=dict(start=dict(line=0, character=0), end=dict(line=0, character=0)), text='// shifted 😀\r\n')]))
        shifted = position(changed, changed.index('27')+1)
        value = select([shifted], changed, version=2)
        assert value[0]['range'] == dict(start=position(changed, changed.index('27')), end=position(changed, changed.index('27')+2))
        done('incremental-change-current-utf16-coordinates')
        bom = '\ufeff'+SOURCE.replace('\n', '\r\n')
        client.change(entry, bom, 3)
        assert select([position(bom, bom.index('27')+1), position(bom, bom.index('😀'))], bom, version=3)
        done('bom-crlf-chinese-emoji-ranges')
        broken = 'func broken('
        client.change(entry, broken, 4)
        p = position(broken, len(broken))
        assert select([p], broken)[0]['range'] == dict(start=p, end=p)
        client.change(entry, SOURCE, 5)
        assert select(points) == rows
        done('source-error-empty-cursor-and-recovery')
        deep = 'func main(...) as int\n return '+'+'.join(['1']*100)+'\nend\n'
        client.change(entry, deep, 6)
        value = select([dict(line=1, character=8)], deep)[0]
        depth = 0
        while value:
            depth += 1
            value = value.get('parent')
        assert depth > 64
        client.change(entry, SOURCE, 7)
        assert select(points) == rows
        done('deep-parent-chain-roundtrip', depth=depth)
        heavy = ''.join(f'func filler{i}() as int\n return {i}\nend\n' for i in range(25000))
        client.open(large, heavy, 1)
        active = 100000
        client.send('textDocument/selectionRange', params([dict(line=0, character=7)], large), active)
        until = time.monotonic()+5
        while time.monotonic() < until:
            observed = result('xlang/analysisStats')
            assert not any(x.get('receive', {}).get('id') == active for x in client.transcript)
            if observed['running'] == 1:
                break
        else:
            raise AssertionError('no live native worker')
        client.send('$/cancelRequest', dict(id=active))
        message, _ = client.reply(active)
        assert message['error']['code'] == -32800
        done('live-parser-cancellation-keeps-protocol-owner-responsive')
        for active, method, code in [(100001, 'change', -32801), (100002, 'close', -32800)]:
            client.send('textDocument/selectionRange', params([dict(line=0, character=7)], large), active)
            if method == 'change':
                client.change(large, '// latest\n'+heavy, 2)
            else:
                client.send('textDocument/didClose', dict(textDocument=dict(uri=large.as_uri())))
            message, _ = client.reply(active)
            assert message['error']['code'] == code
        assert select(points) == rows
        for active in (100000, 100001, 100002):
            assert sum(x.get('receive', {}).get('id') == active for x in client.transcript) == 1
        done('edit-close-cancel-with-no-late-success-or-owner-contamination')
        assert not missing.exists() and not large.exists()
        done('no-compiler-or-source-file-created')
        status = 'passed'
    finally:
        stderr = client.stop()
        (output/'stderr.log').write_text(stderr, encoding='utf8')
        (output/'transcript.json').write_text(json.dumps(client.transcript, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
        assert all(sha(p) == h for p, h in protected.items())
        proof = dict(status=status, cases=cases, exitCode=client.process.returncode, protected=protected,
                     scope='standard structural selection correctness and lifecycle; no IDE UI or formal performance acceptance')
        (output/'result.json').write_text(json.dumps(proof, indent=2)+'\n', encoding='utf8')
    assert client.process.returncode == 0
    print(json.dumps(dict(status=status, cases=len(cases), proof=str(output/'result.json'))))


if __name__ == '__main__':
    main()
