"""Exercise production read-only references across every configured native scope."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_analysis_configuration import LISTS, PROFILE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(); server = args.server.resolve(strict=True)
    root = args.output.resolve(); root.mkdir(parents=True, exist_ok=False)
    paths = {name: root/name for name in ('a.xl', 'closed.xl', 'b.xl', 'c.xl', 'loose.xl', 'virtual.xl', 'extlibs/shared.xl')}
    paths['extlibs/shared.xl'].parent.mkdir()
    body = '\nimport extlibs.shared as api\nfunc main(...) as int\n return api.amount(1)\nend\n'
    declaration = 'export func amount(int value) as int\n return value\nend\n'
    for name, path in paths.items():
        if name == 'virtual.xl': continue
        text = declaration if name == 'extlibs/shared.xl' else ('func amount(int value) as int\n return value\nend\nfunc main(...) as int\n return amount(1)\nend\n' if name == 'c.xl' else body)
        path.write_bytes(text.encode())
    def project(name, entry, members):
        lists = {key: [] for key in LISTS}; lists['members'] = [paths[p].as_posix() for p in members]
        return dict(schema=1, kind='project', project=(root/name).as_posix(), configuration='Debug',
                    entry=paths[entry].as_posix(), directory=root.as_posix(), analysisProfile=PROFILE,
                    targetKind='exe', packagePath=None, lists=lists)
    a, b, c = project('A.xlp', 'a.xl', ['a.xl', 'closed.xl']), project('B.xlp', 'b.xl', ['b.xl']), project('C.xlp', 'c.xl', ['c.xl'])
    catalog = dict(schema=2, projects=[a, b, c], selectedProject=a['project'], packagePath=None,
                   standaloneRoots=[paths['loose.xl'].as_posix()], complete=True)
    configuration = dict(epoch=1, catalog=catalog); clients, cases = [], []
    unicode_path = root/'源码😀.xl'
    unicode_body = '\ufeffimport extlibs.shared as api\r\nfunc main(...) as int\r\n string label = "😀"; return api.amount(1)\r\nend\r\n'
    unicode_path.write_bytes(unicode_body.encode())
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(p): sha(p) for p in [server, Path(__file__), unicode_path, *[p for p in paths.values() if p.exists()]]}
    report = dict(status='failed', scope='Actual stdio exact native binding references,worker frames,all scopes and input/config publication fences;no formal latency or RSS claim.', cases=cases, protected=protected)
    def done(name, **extra): cases.append(dict(name=name, passed=True, **extra))
    def start(config=configuration, proof=False):
        client = AutomaticClient(server, root/'missing-xl.exe', root); clients.append(client)
        options = dict(automaticDiagnostics=False, checkOnSave=False, workspaceSymbolBackgroundIndex=False)
        if config is not None: options['analysisConfiguration'] = config
        client.send('initialize', dict(capabilities=dict(experimental=dict(referenceInputProof=proof)), initializationOptions=options), 1)
        reply, _ = client.reply(1); assert 'error' not in reply and reply['result']['capabilities']['referencesProvider'], reply
        client.send('initialized', {}); return client
    def params(name='a.xl', include=True, line=3, character=14):
        return dict(textDocument=dict(uri=paths[name].as_uri()), position=dict(line=line, character=character), context=dict(includeDeclaration=include))
    def request(client, value, rid='references', code=None):
        client.send('textDocument/references', value, rid); reply, _ = client.reply(rid)
        assert type(reply['id']) is type(rid) and reply['id'] == rid, reply
        if code is not None:
            assert reply.get('error', {}).get('code') == code, reply
            return reply
        assert 'error' not in reply, reply
        return reply['result']
    def verify(rows, include=True, virtual=False, proof=False):
        names = ['a.xl', 'closed.xl', 'b.xl', 'loose.xl'] + (['extlibs/shared.xl'] if include else []) + (['virtual.xl'] if virtual else [])
        assert len(rows) == len(names) and {row['uri'] for row in rows} == {paths[n].as_uri() for n in names}, rows
        for row in rows:
            point = row['range']; declaration_row = row['uri'] == paths['extlibs/shared.xl'].as_uri()
            assert point == dict(start=dict(line=0 if declaration_row else 3, character=12), end=dict(line=0 if declaration_row else 3, character=18)), row
            assert ('xlang' in row) == proof, row
    def batch(client, value, method, payload):
        client.batch([dict(jsonrpc='2.0', method='textDocument/references', params=value, id='active'),
                      dict(jsonrpc='2.0', method=method, params=payload)])
        reply, _ = client.reply('active'); return reply
    try:
        client = start()
        for name, rid in [('empty-string', ''), ('zero', 0), ('int64-min', -(2**63)), ('int64-max', 2**63-1), ('unicode-escaped', '引用😀-"\\id'), ('max-string', 'x'*65535)]:
            verify(request(client, params(), rid)); work = client.call('xlang/referenceStats')
            assert work['workspaceScopes'] == work['workspaceCompleted'] == 4 and work['scopeValidations'] == 2 and work['encodedFrameBytes'] > 0, work
            done('closed-cursor-exact-id-' + name, work=work)
        verify(request(client, params(include=False)), include=False); done('includeDeclaration-false-keeps-every-use')
        client.open(paths['a.xl'], body, 2); verify(request(client, params())); done('open-cursor-has-same-complete-scopes')
        request(client, dict(params(), version=1), code=-32602); done('stale-overlay-version-is-invalid')
        assert request(client, params(line=0, character=0)) == []
        assert client.call('xlang/referenceStats')['noSymbol'] == 1; done('valid-no-symbol-is-guarded-empty-result')
        closed = start(); closed.open(paths['b.xl'], body, -2**31)
        verify(request(closed, params('closed.xl'))); done('closed-cursor-with-other-open-overlay')
        request(closed, params('closed.xl', line=999), code=-32602); done('closed-out-of-range-position-is-invalid')
        request(closed, dict(params('closed.xl'), version=0), code=-32602); done('closed-cursor-cannot-claim-overlay-version')
        proof = start(proof=True); proof.open(paths['a.xl'], body, 2**31-1); proof.open(paths['virtual.xl'], body, -2**31)
        rows = request(proof, params()); verify(rows, virtual=True, proof=True)
        docs = {row['uri']: row['xlang']['document'] for row in rows}
        assert docs[paths['virtual.xl'].as_uri()]['version'] == -2**31 and docs[paths['a.xl'].as_uri()]['version'] == 2**31-1, docs
        assert docs[paths['extlibs/shared.xl'].as_uri()]['origin'] == 'disk', docs
        done('unowned-virtual-overlay-and-negotiated-complete-proofs')
        standalone = start(config=None); verify_rows = request(standalone, params('loose.xl'))
        assert {row['uri'] for row in verify_rows} == {paths['loose.xl'].as_uri(), paths['extlibs/shared.xl'].as_uri()}, verify_rows
        done('standalone-closed-file-needs-no-workspace')
        unicode_client = start(config=None)
        line = unicode_body.splitlines()[2]; character = len(line[:line.index('amount')].encode('utf-16-le'))//2
        unicode_params = dict(textDocument=dict(uri=unicode_path.as_uri()), position=dict(line=2, character=character+2), context=dict(includeDeclaration=True))
        for opened in (False, True):
            if opened: unicode_client.open(unicode_path, unicode_body, 3)
            rows = request(unicode_client, unicode_params)
            assert len(rows) == 2 and {row['uri'] for row in rows} == {unicode_path.as_uri(), paths['extlibs/shared.xl'].as_uri()}, rows
            row = next(row for row in rows if row['uri'] == unicode_path.as_uri())
            assert row['range'] == dict(start=dict(line=2, character=character), end=dict(line=2, character=character+6)), row
            done(('open' if opened else 'closed')+'-BOM-CRLF-Unicode-path-and-UTF16-range')
        half_surrogate = len(line[:line.index('😀')].encode('utf-16-le'))//2+1
        request(unicode_client, dict(unicode_params, position=dict(line=2, character=half_surrogate)), code=-32602)
        done('half-surrogate-is-an-invalid-UTF16-position')
        for name, method, payload, code in [
            ('cancel', '$/cancelRequest', dict(id='active'), -32800),
            ('open-change', 'textDocument/didChange', dict(textDocument=dict(uri=paths['a.xl'].as_uri(), version=2), contentChanges=[dict(text=body+'\n')]), -32801),
            ('close', 'textDocument/didClose', dict(textDocument=dict(uri=paths['a.xl'].as_uri())), -32801),
            ('configuration', 'workspace/didChangeConfiguration', dict(settings=dict(xlang=dict(analysisConfiguration=dict(epoch=2, catalog=catalog)))), -32801),
            ('workspace-folders', 'workspace/didChangeWorkspaceFolders', dict(event=dict(added=[dict(uri=root.as_uri(), name=root.name)], removed=[])), -32801),
        ]:
            fresh = start(); fresh.open(paths['a.xl'], body, 1)
            reply = batch(fresh, params(), method, payload); assert reply.get('error', {}).get('code') == code, reply
            done('queued-' + name + '-fences-pending-frame')
        fresh = start(); reply = batch(fresh, params(), 'textDocument/didOpen', dict(textDocument=dict(uri=paths['b.xl'].as_uri(), languageId='xlang', version=1, text=body)))
        assert reply.get('error', {}).get('code') == -32801, reply; done('closed-cursor-is-fenced-by-other-overlay-open')
        assert all(sha(Path(path)) == digest for path, digest in protected.items()); report['status'] = 'passed'
    finally:
        for index, client in enumerate(clients):
            stderr = client.stop(); (root/f'client-{index}.json').write_text(json.dumps(client.transcript, ensure_ascii=False, indent=2)+'\n', encoding='utf8'); (root/f'client-{index}.stderr').write_text(stderr, encoding='utf8')
        (root/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(root/'result.json')), ensure_ascii=False))


if __name__ == '__main__':
    main()
