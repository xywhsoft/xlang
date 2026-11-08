"""Actual stdio callable scopes, negotiated symbol shape and UTF-16 ranges.

No executable compiler is supplied: these queries must use the native actor.
Missing/false hierarchy capability deliberately exercises SymbolInformation.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import statistics
import time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position


def flatten(symbols, uri, parent=None):
    rows = []
    for item in symbols:
        row = dict(name=item['name'], kind=item['kind'],
                   location=dict(uri=uri, range=item['range']))
        if parent is not None:
            row['containerName'] = parent
        rows.append(row)
        rows.extend(flatten(item['children'], uri, item['name']))
    return rows


def checked(client, path, version):
    until = time.monotonic()+30
    while True:
        client.request += 1
        client.send('xlang/check', dict(textDocument=dict(uri=path.as_uri()), version=version), client.request)
        reply, _ = client.reply(client.request)
        if reply.get('error', {}).get('code') == -32001:
            assert time.monotonic() < until, reply
            time.sleep(.001)
            continue
        assert 'error' not in reply, reply
        assert reply['result']['version'] == version, reply
        return reply['result']


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--server', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    a = ap.parse_args()
    server = a.server.resolve(strict=True)
    root = a.output.resolve(); root.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(p): sha(p) for p in (server, Path(__file__),
        Path(__file__).with_name('check_language_snapshots.py'),
        Path(__file__).with_name('check_lsp_module_semantics.py'))}
    cases, clients = [], []
    report = dict(status='failed', cases=cases, protected=protected,
        scope='Real native stdio document symbols, capability negotiation, callable scopes, edits and ranges; timings are observations, not global performance acceptance')
    path = root/'大纲 中文😀.xl'
    source = ('\ufeff// 中文😀\r\nclass Box\r\n'
        ' async func fetch(int input) as int\r\n  int hidden_method=input+1\r\n  return hidden_method\r\n end\r\nend\r\n'
        'extern "probe_value" func native_value(int input) as int\r\n'
        'async func fetch(int input) as int\r\n int hidden_local=input+1\r\n return hidden_local\r\nend\r\n'
        'func main(...) as int\r\n return 0\r\nend\r\n')
    # A deliberately different on-disk text proves the open buffer is used.
    path.write_text('// disk placeholder\n', encoding='utf-8')
    protected[str(path)] = sha(path)

    def client(capabilities):
        c = AutomaticClient(server, root/'absent-compiler/xl.exe', root); clients.append(c)
        c.send('initialize', dict(capabilities=capabilities,
            initializationOptions=dict(automaticDiagnostics=False, checkOnSave=False)), 1)
        reply, _ = c.reply(1)
        return c, reply

    def caps(value):
        return dict(textDocument=dict(documentSymbol=dict(hierarchicalDocumentSymbolSupport=value)))

    def query(c):
        return c.call('textDocument/documentSymbol', dict(textDocument=dict(uri=path.as_uri())))

    def resource_error(c, document):
        c.request += 1; request = c.request
        c.send('textDocument/documentSymbol', dict(textDocument=dict(uri=document.as_uri())), request)
        until = time.monotonic()+12
        while True:
            assert time.monotonic() < until, 'Symbol resource failure never completed (possible analysis retry storm)'
            reply = c.next()
            if reply.get('id') == request:
                assert reply.get('error', {}).get('code') == -32603, reply
                return
            assert 'id' not in reply, reply

    try:
        rich, reply = client(caps(True)); assert reply['result']['capabilities']['documentSymbolProvider'] is True, reply
        rich.send('initialized', {}); rich.open(path, source, 1)
        assert checked(rich, path, 1)['ok']
        expected = query(rich)
        assert [n['name'] for n in expected] == ['Box', 'native_value', 'fetch', 'main'], expected
        box = expected[0]
        assert [(n['name'], n['kind']) for n in box['children']] == [('fetch', 6)], box
        assert not box['children'][0]['children'] and not expected[2]['children']
        for item, offset in [(expected[2], source.rindex('fetch')),
                             (box['children'][0], source.index('fetch'))]:
            assert item['selectionRange'] == dict(start=position(source, offset), end=position(source, offset+5)), item
        flat_expected = flatten(expected, path.as_uri())
        cases.append(dict(name='native-async-method-and-extern-outline-excludes-locals', passed=True))
        for name, cp in [('missing', {}), ('false', caps(False)), ('empty-document-symbol', dict(textDocument=dict(documentSymbol={})))]:
            c, reply = client(cp); assert 'error' not in reply, reply
            c.send('initialized', {}); c.open(path, source, 1)
            if name == 'missing':
                # A standard client need not call the private xlang/check API.
                assert query(c) == flat_expected
            assert checked(c, path, 1)['ok']
            assert query(c) == flat_expected
            cases.append(dict(name=name+'-returns-flat-symbol-information', passed=True))
            c.stop()
        for value in [None, 0, 1, 'true', [], {}]:
            c, reply = client(caps(value)); assert reply.get('error', {}).get('code') == -32602, reply
            # A rejected initialize must not poison later negotiation or XRT.
            c.send('initialize', dict(capabilities=caps(True), initializationOptions=dict(automaticDiagnostics=False, checkOnSave=False)), 2)
            reply, _ = c.reply(2); assert 'error' not in reply, reply
            c.send('initialized', {}); c.open(path, source, 1); assert checked(c, path, 1)['ok']; assert query(c) == expected
            cases.append(dict(name='reject-and-recover-'+type(value).__name__+'-'+str(value), passed=True)); c.stop()
        changed = '// moved 😀\r\n' + source.removeprefix('\ufeff').replace('async func fetch(int input)', 'async func renamed(int input)')
        rich.change(path, changed, 2); assert checked(rich, path, 2)['ok']
        current = query(rich); assert [n['name'] for n in current] == ['Box', 'native_value', 'renamed', 'main']
        assert current[0]['children'][0]['name'] == 'renamed'
        assert current[2]['selectionRange']['start'] == position(changed, changed.rindex('renamed'))
        cases.append(dict(name='changed-buffer-replaces-names-and-utf16-ranges', passed=True))
        samples = []
        for _ in range(32):
            begin = time.perf_counter(); assert query(rich) == current; samples.append((time.perf_counter()-begin)*1000)
        report['cachedQueryObservationMs'] = dict(samples=samples, p50=statistics.median(samples), p95=sorted(samples)[30])
        small = 'func main(...) as int\n return 0\nend\n'
        for name, document, count, cp in [
                ('symbol-count', root/'too-many.xl', 4097, caps(True)),
                ('repeated-uri-budget', root.joinpath(*(['segment'+'x'*180]*22), 'virtual.xl'), 350, {})]:
            c, reply = client(cp); assert 'error' not in reply, reply; c.send('initialized', {})
            body = ''.join(f'func item_{i}() as int\n return {i}\nend\n' for i in range(count)) + small
            c.open(document, body, 1); before = checked(c, document, 1); assert before['ok'], before
            resource_error(c, document); resource_error(c, document)
            after = checked(c, document, 1); assert after['analysis']['builds'] == before['analysis']['builds'], (before, after)
            c.change(document, small, 2); assert checked(c, document, 2)['ok']
            value = c.call('textDocument/documentSymbol', dict(textDocument=dict(uri=document.as_uri())))
            assert len(value) == 1 and value[0]['name'] == 'main', value
            if name == 'repeated-uri-budget': assert value[0]['location']['uri'] == document.as_uri(), value
            cases.append(dict(name=name+'-fails-once-without-reanalysis-and-recovers', passed=True, uriBytes=len(document.as_uri().encode()), symbols=count+1)); c.stop()
        for c in clients: c.stop()
        for p, h in protected.items(): assert sha(p) == h, p
        report['status'] = 'passed'
    finally:
        for i, c in enumerate(clients):
            (root/f'client-{i}-stderr.log').write_text(c.stop(), encoding='utf-8')
            (root/f'client-{i}-transcript.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
        (root/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(dict(status=report['status'], cases=len(cases), cachedQueryObservationMs=report['cachedQueryObservationMs']), ensure_ascii=False))


if __name__ == '__main__':
    main()
