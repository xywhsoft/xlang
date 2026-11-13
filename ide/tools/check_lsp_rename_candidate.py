"""Real native candidate RPC; read-only output, strict bindings and owner fences."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
from check_language_snapshots import Client
from check_lsp_analysis_configuration import LISTS, PROFILE


def digest(rows):
    data = b'xlang-input-state-v1\0' + len(rows).to_bytes(4, 'big')
    for uri, version in sorted(rows, key=lambda row: hashlib.sha256(row[0].encode()).digest()):
        data += hashlib.sha256(uri.encode()).digest() + (version & 0xffffffff).to_bytes(4, 'big')
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    root = out/'native rename 空间😀'
    root.mkdir()
    compiler = out/'absent-xl.exe'
    paths = {name: root/(name+'.xl') for name in ('main', 'second', 'shared', 'unopened', 'isolated', 'standalone')}
    bodies = dict(main='\ufeffimport shared\r\nfunc main(...) as int\r\n return shared.compute(1)\r\nend\r\n',
                  second='import shared\nfunc main(...) as int\n return shared.compute(2)\nend\n',
                  shared='\ufeffexport func compute(int value) as int\r\n return value\r\nend\r\n',
                  unopened='import shared\nfunc main(...) as int\n return shared.compute(3)\nend\n',
                  isolated='func compute(int value) as int\n return value\nend\nfunc main(...) as int\n return compute(4)\nend\n',
                  standalone='import shared\nfunc main(...) as int\n return shared.compute(5)\nend\n')
    for key, path in paths.items():
        path.write_bytes(bodies[key].encode())
    projects = []
    for name, entry, members in [('A', 'main', ['main', 'shared']), ('B', 'second', ['second', 'shared', 'unopened']), ('C', 'isolated', ['isolated'])]:
        lists = {key: [] for key in LISTS}
        lists['members'] = [paths[key].as_posix() for key in members]
        projects.append(dict(schema=1, kind='project', project=(root/(name+'.xlp')).as_posix(),
                             configuration='debug', entry=paths[entry].as_posix(), directory=root.as_posix(),
                             analysisProfile=PROFILE, targetKind='exe', packagePath=None, lists=lists))
    catalog = dict(schema=1, selectedProject=projects[0]['project'], packagePath=None, projects=projects)
    protected = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in [server, Path(__file__), *paths.values()]}
    clients, cases = [], []
    report = dict(status='failed', cases=cases, protected=protected, fullGoalComplete=False, performanceQualified=False)
    request = 100

    def done(name, **fields):
        cases.append(dict(name=name, passed=True, **fields))

    def call(c, method, params, code=None):
        nonlocal request
        request += 1
        c.send(method, params, request)
        reply, _ = c.reply(request)
        if code is None:
            assert 'error' not in reply, reply
            return reply['result']
        assert reply.get('error', {}).get('code') == code, reply
        return reply

    def start(config=catalog, opened=None, backend='native', version=2147483647):
        c = Client(server, compiler, root)
        clients.append(c)
        c.send('initialize', dict(capabilities={}, initializationOptions=dict(checkOnSave=False,
                    automaticDiagnostics=False, diagnosticsBackend=backend)), 1)
        reply, _ = c.reply(1)
        caps = reply['result']['capabilities']
        assert caps['experimental']['renameCandidate'] == (1 if backend == 'native' else 0), caps
        assert not caps.get('renameProvider'), caps
        c.send('initialized', {})
        if config is not None:
            call(c, 'xlang/configureAnalysis', dict(epoch=1, catalog=config))
        for key in ['shared'] if opened is None else opened:
            c.open(paths[key], bodies[key], version)
        return c

    def intent(c, key='shared', config=catalog, roots=None, marker='compute', count=None, version=2147483647):
        state = call(c, 'xlang/inputState', {})
        original = bodies[key]
        at = original.index(marker)+1
        before = original[:at]
        line = before.count('\n')
        column = len(before.rsplit('\n', 1)[-1].encode('utf-16-le'))//2
        return dict(textDocument=dict(uri=paths[key].as_uri(), version=version),
                    position=dict(line=line, character=column), newName='renamed_😀',
                    xlang=dict(schema=1, token='ffffffffffffffff', catalog=copy.deepcopy(config),
                        standaloneRoots=[paths['standalone'].as_posix()] if roots is None else roots,
                        scopeCount=len(config['projects'])+1 if count is None else count,
                        inputState=dict(digest=state['sha256'], documents=state['documents'])))

    def verify(value, params, scopes=4, documents=5):
        assert set(value) == {'schema', 'binding', 'candidate'} and value['schema'] == 1, value
        b = value['binding']
        assert b == dict(method='xlang/renameCandidate', token=params['xlang']['token'],
                         inputDigest=params['xlang']['inputState']['digest'], documents=params['xlang']['inputState']['documents']), b
        candidate = value['candidate']
        assert candidate['schema'] == 1 and candidate['scopeCount'] == scopes, candidate
        changes = candidate['workspaceEdit']['documentChanges']
        assert len(changes) == documents and sum(len(row['edits']) for row in changes) >= documents, changes
        assert all(edit['newText'] == params['newName'] for row in changes for edit in row['edits'])
        guards = candidate['inputGuards']
        assert len(guards) >= scopes+1 and all(set(g) == {'schema', 'context', 'files', 'resolution'} for g in guards), guards
        assert all(g['schema'] == 1 and g['files'] for g in guards)
        if scopes == 4:
            assert any(any(Path(f['path']).name == 'isolated.xl' for f in g['files']) for g in guards), guards
            overlay = [row for row in changes if row['textDocument']['uri'] == paths['shared'].as_uri()]
            assert len(overlay) == 1 and overlay[0]['textDocument']['version'] == 2147483647
        return candidate

    try:
        c = start()
        p = intent(c)
        assert p['xlang']['inputState']['digest'] == digest([(paths['shared'].as_uri(), 2147483647)])
        candidate = verify(call(c, 'xlang/renameCandidate', p), p)
        done('actual-four-scope-imported-declaration-and-complete-proof', guards=len(candidate['inputGuards']))
        verify(call(c, 'xlang/renameCandidate', p), p)
        done('warm-repeat-remains-bound-and-read-only')
        invalid = []
        def bad(name, fn, code=-32602):
            item = copy.deepcopy(p)
            fn(item)
            invalid.append((name, item, code))
        bad('unknown-root-field', lambda v: v.update(extra=True))
        bad('unknown-extension-field', lambda v: v['xlang'].update(extra=True))
        bad('fractional-schema', lambda v: v['xlang'].update(schema=1.5))
        bad('zero-token', lambda v: v['xlang'].update(token='0'*16))
        bad('uppercase-token', lambda v: v['xlang'].update(token='F'*16))
        bad('wrong-receipt', lambda v: v['xlang']['inputState'].update(digest='0'*64), -32801)
        bad('wrong-document-count', lambda v: v['xlang']['inputState'].update(documents=2))
        bad('stale-version', lambda v: v['textDocument'].update(version=1), -32801)
        bad('fractional-version', lambda v: v['textDocument'].update(version=1.5))
        bad('unknown-uri', lambda v: v['textDocument'].update(uri=paths['main'].as_uri()))
        bad('invalid-identifier', lambda v: v.update(newName='return'))
        bad('empty-name', lambda v: v.update(newName=''))
        bad('invalid-line', lambda v: v['position'].update(line=100000))
        bad('wrong-catalog-selection', lambda v: v['xlang']['catalog'].update(selectedProject=projects[1]['project']), -32801)
        bad('omitted-scope', lambda v: v['xlang'].update(scopeCount=3))
        bad('duplicate-root', lambda v: v['xlang']['standaloneRoots'].append(paths['standalone'].as_posix()))
        bad('redundant-project-root', lambda v: v['xlang'].update(standaloneRoots=[paths['main'].as_posix()]))
        bad('relative-root', lambda v: v['xlang'].update(standaloneRoots=['standalone.xl']))
        bad('c-root', lambda v: v['xlang'].update(standaloneRoots=[(root/'native.c').as_posix()]))
        for name, item, code in invalid:
            call(c, 'xlang/renameCandidate', item, code)
            done(name)
        for invalid_id in ('named-request', -7):
            c.send('xlang/renameCandidate', p, invalid_id)
            assert c.reply(invalid_id)[0].get('error', {}).get('code') == -32602
        done('custom-candidate-method-requires-positive-integer-id')
        call(c, 'textDocument/rename', p, -32601)
        done('standard-rename-remains-unadvertised-and-unimplemented')
        c.open(paths['standalone'], bodies['standalone'], 2147483647)
        omitted = intent(c, roots=[], count=3)
        call(c, 'xlang/renameCandidate', omitted, -32602)
        done('cannot-omit-other-open-standalone-document')
        c.stop()
        c = start(opened=['main'])
        p = intent(c, 'main')
        value = call(c, 'xlang/renameCandidate', p)
        assert len(value['candidate']['workspaceEdit']['documentChanges']) == 5
        assert value['binding']['inputDigest'] == p['xlang']['inputState']['digest']
        done('qualified-use-through-actual-checked-provider')
        paths['isolated'].write_text('func broken(\n', encoding='utf8')
        call(c, 'xlang/renameCandidate', p, -32803)
        paths['isolated'].write_bytes(bodies['isolated'].encode())
        done('unrelated-invalid-scope-rejects-whole-candidate')
        # Send invalidation/cancellation in the same pipe write as the intent.
        def batch(messages):
            raw = b''
            for method, params, rid in messages:
                m = dict(jsonrpc='2.0', method=method, params=params)
                if rid is not None:
                    m['id'] = rid
                c.transcript.append(dict(send=m))
                data = json.dumps(m, ensure_ascii=False, separators=(',', ':')).encode()
                raw += f'Content-Length: {len(data)}\r\n\r\n'.encode()+data
            c.process.stdin.write(raw)
            c.process.stdin.flush()
        request += 1
        rid = request
        batch([('xlang/renameCandidate', p, rid), ('$/cancelRequest', dict(id=rid), None)])
        reply, _ = c.reply(rid)
        assert reply.get('error', {}).get('code') == -32800, reply
        done('actual-protocol-cancel-without-partial-result')
        request += 1
        rid = request
        batch([('xlang/renameCandidate', p, rid), ('textDocument/didChange', dict(textDocument=dict(uri=paths['main'].as_uri(), version=2147483647), contentChanges=[dict(text=bodies['main']+'\n')]), None)])
        # didChange with an equal version is invalid, so use a fresh client/version.
        reply, _ = c.reply(rid)
        assert 'result' in reply, reply
        done('rejected-invalid-notification-does-not-invent-new-input')
        c.stop()
        c = start(opened=['main'], version=1)
        p = intent(c, 'main', version=1)
        request += 1
        rid = request
        batch([('xlang/renameCandidate', p, rid), ('textDocument/didChange', dict(textDocument=dict(uri=paths['main'].as_uri(), version=2), contentChanges=[dict(text=bodies['main']+'\n')]), None)])
        reply, _ = c.reply(rid)
        assert reply.get('error', {}).get('code') == -32801, reply
        state = call(c, 'xlang/inputState', {})
        assert state['sha256'] == digest([(paths['main'].as_uri(), 2)])
        done('actual-document-change-discards-worker-result')
        c.stop()
        c = start(opened=['main'])
        p = intent(c, 'main')
        request += 1
        rid = request
        batch([('xlang/renameCandidate', p, rid), ('textDocument/didClose', dict(textDocument=dict(uri=paths['main'].as_uri())), None), ('textDocument/didOpen', dict(textDocument=dict(uri=paths['main'].as_uri(), text=bodies['main'], version=2147483647, languageId='xlang')), None)])
        reply, _ = c.reply(rid)
        assert reply.get('error', {}).get('code') == -32801, reply
        done('same-uri-version-close-reopen-invalidates-original-identity')
        c.stop()
        c = start(opened=['main'])
        p = intent(c, 'main')
        request += 1
        rid = request
        batch([('xlang/renameCandidate', p, rid), ('xlang/configureAnalysis', dict(epoch=2, catalog=catalog), rid+1)])
        replies = {}
        while len(replies) < 2:
            m = c.next()
            if m.get('id') in (rid, rid+1):
                replies[m['id']] = m
        assert replies[rid].get('error', {}).get('code') == -32801 and 'result' in replies[rid+1], replies
        request += 1
        done('configuration-epoch-change-discards-queued-worker-result')
        c.stop()
        c = start(opened=['main'])
        p = intent(c, 'main')
        request += 1
        rid = request
        batch([('xlang/renameCandidate', p, rid), ('xlang/suspendAnalysis', dict(token=1), rid+1)])
        replies = {}
        while len(replies) < 2:
            m = c.next()
            if m.get('id') in (rid, rid+1):
                replies[m['id']] = m
        assert replies[rid].get('error', {}).get('code') == -32800 and replies[rid+1].get('result', {}).get('held'), replies
        request += 1
        call(c, 'xlang/renameCandidate', p, -32001)
        call(c, 'xlang/resumeAnalysis', dict(token=1))
        assert 'candidate' in call(c, 'xlang/renameCandidate', p)
        done('lease-cancels-and-waits-for-physical-orchestration-quiescence')
        c.stop()
        single = dict(schema=1, selectedProject='', packagePath=None, projects=[])
        c = start(single, ['isolated'])
        p = intent(c, 'isolated', single, [paths['isolated'].as_posix()], marker='compute(4)', count=1)
        verify(call(c, 'xlang/renameCandidate', p), p, 1, 1)
        done('single-file-no-project-or-workspace')
        c.stop()
        c = start(None)
        c.send('xlang/renameCandidate', {}, 700)
        assert c.reply(700)[0].get('error', {}).get('code') == -32602
        done('requires-acknowledged-catalog')
        c.stop()
        c = start(None, backend='cli')
        call(c, 'xlang/renameCandidate', {}, -32601)
        done('cli-backend-does-not-offer-native-candidates')
        report['status'] = 'passed'
    finally:
        paths['isolated'].write_bytes(bodies['isolated'].encode())
        report['stderr'] = [c.stop() for c in clients]
        report['transcripts'] = [c.transcript for c in clients]
        report['protectedUnchanged'] = all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == h for p, h in protected.items())
        (out/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    stderr = [value for value in report['stderr'] if value]
    assert report['protectedUnchanged'] and stderr == ['xlang-ls: rejected textDocument/didChange notification.\r\n'], (report['protectedUnchanged'], stderr)
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(out/'result.json')), ensure_ascii=False))


if __name__ == '__main__':
    main()
