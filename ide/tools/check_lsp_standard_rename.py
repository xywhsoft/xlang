"""Standard rename over real stdio/native analysis; never apply server edits."""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import time

from check_language_snapshots import Client
from check_lsp_analysis_configuration import LISTS, PROFILE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    root = out/'standard rename 空间😀'
    root.mkdir()
    bodies = dict(
        main='\ufeffimport shared\r\nfunc main(...) as int\r\n return shared.compute(1)\r\nend\r\n',
        second='import shared\nfunc main(...) as int\n return shared.compute(2)\nend\n',
        shared='\ufeffexport func compute(int value) as int\r\n return value\r\nend\r\nexport func occupied(int value) as int\r\n return value\r\nend\r\n',
        closed='import shared\nfunc main(...) as int\n return shared.compute(3)\nend\n',
        isolated='func compute(int value) as int\n return value\nend\nfunc main(...) as int\n return compute(4)\nend\n',
        loose='func main(...) as int\n int amount = 1\n return amount\nend\n',
        genericGlobal='func helper<T>(T value) as int\n return compute(1)\nend\nfunc compute(int value) as int\n return value\nend\nfunc main(...) as int\n return compute(2)\nend\n',
        genericDependent='class Box\n int amount\nend\nfunc helper<T>(T box) as int\n return box.amount\nend\nfunc main(...) as int\n auto box=Box()\n return box.amount\nend\n')
    paths = {name: root/(name+'.xl') for name in bodies}
    for name, path in paths.items():
        path.write_bytes(bodies[name].encode())
    projects = []
    for name, entry, members in [('A', 'main', ['main', 'shared']),
                                  ('B', 'second', ['second', 'shared', 'closed']),
                                  ('C', 'isolated', ['isolated'])]:
        lists = {key: [] for key in LISTS}
        lists['members'] = [paths[key].as_posix() for key in members]
        projects.append(dict(schema=1, kind='project', project=(root/(name+'.xlp')).as_posix(),
                             configuration='debug', entry=paths[entry].as_posix(),
                             directory=root.as_posix(), analysisProfile=PROFILE,
                             targetKind='exe', packagePath=None, lists=lists))
    catalog = dict(schema=1, selectedProject=projects[0]['project'], packagePath=None, projects=projects)
    protected = {str(path): hashlib.sha256(path.read_bytes()).hexdigest()
                 for path in [server, Path(__file__), *paths.values()]}
    clients, cases, timings = [], [], []
    report = dict(status='failed', cases=cases, timings=timings, protected=protected,
                  performanceQualified=False, fullGoalComplete=False)
    serial = 100

    def done(name, **fields):
        cases.append(dict(name=name, passed=True, **fields))

    def call(c, method, params, code=None, rid=None):
        nonlocal serial
        serial += 1
        rid = serial if rid is None else rid
        before = time.perf_counter()
        c.send(method, params, rid)
        reply, _ = c.reply(rid)
        assert type(reply['id']) is type(rid), reply
        if code is not None:
            assert reply.get('error', {}).get('code') == code, reply
        else:
            assert 'result' in reply and 'error' not in reply, reply
        if method == 'textDocument/rename':
            timings.append(dict(id=rid, milliseconds=(time.perf_counter()-before)*1000,
                                error=reply.get('error', {}).get('code')))
        return reply.get('result')

    def start(config=catalog, modern=True, opened=('main',), backend='native', **roots):
        c = Client(server, out/'missing-xl.exe', root)
        clients.append(c)
        options = dict(checkOnSave=False, automaticDiagnostics=False,
                       backgroundWorkspaceSymbols=False, diagnosticsBackend=backend)
        if config is not None:
            options['analysisConfiguration'] = dict(epoch=1, catalog=config)
        params = dict(capabilities=dict(workspace=dict(workspaceEdit=dict(documentChanges=modern)),
                      textDocument=dict(rename=dict(prepareSupport=True))),
                      initializationOptions=options, **roots)
        c.send('initialize', params, 1)
        reply, _ = c.reply(1)
        assert 'error' not in reply, reply
        caps = reply['result']['capabilities']
        assert bool(caps.get('renameProvider')) == (backend == 'native'), caps
        # No fake prepare provider or defaultBehavior without actual preparation.
        assert not isinstance(caps.get('renameProvider'), dict), caps
        c.send('initialized', {})
        for name in opened:
            c.open(paths[name], bodies[name], -2147483648)
        return c

    def intent(name='main', marker='compute', new_name='renamed_值😀'):
        before = bodies[name][:bodies[name].index(marker)+1]
        return dict(textDocument=dict(uri=paths[name].as_uri()),
                    position=dict(line=before.count('\n'),
                                  character=len(before.rsplit('\n', 1)[-1].encode('utf-16-le'))//2),
                    newName=new_name)

    def verify(value, modern=True, documents=('main', 'second', 'shared', 'closed')):
        assert set(value) == ({'documentChanges'} if modern else {'changes'}), value
        if modern:
            rows = value['documentChanges']
            assert all(set(row) == {'textDocument', 'edits'} for row in rows), rows
            changes = {row['textDocument']['uri']: row['edits'] for row in rows}
            assert len(changes) == len(rows), rows
            for row in rows:
                assert row['textDocument']['version'] == (-2147483648 if row['textDocument']['uri'] == paths['main'].as_uri() else None), row
        else:
            changes = value['changes']
        assert set(changes) == {paths[name].as_uri() for name in documents}, changes
        assert all(set(edit) == {'range', 'newText'} and edit['newText'] == 'renamed_值😀'
                   for edits in changes.values() for edit in edits), changes
        # Qualified references must edit only the identifier, preserving qualifier.
        assert changes[paths['main'].as_uri()][0]['range'] == dict(
            start=dict(line=2, character=15), end=dict(line=2, character=22)), changes
        return changes

    def batch(c, messages):
        raw = b''
        for method, params, rid in messages:
            value = dict(jsonrpc='2.0', method=method, params=params)
            if rid is not None:
                value['id'] = rid
            c.transcript.append(dict(send=value))
            data = json.dumps(value, ensure_ascii=False, separators=(',', ':')).encode()
            raw += f'Content-Length: {len(data)}\r\n\r\n'.encode()+data
        c.process.stdin.write(raw)
        c.process.stdin.flush()

    try:
        c = start(rootUri=root.as_uri())
        p = intent()
        modern = verify(call(c, 'textDocument/rename', p, rid='rename-😀'))
        done('configured-standard-only-request-four-files-including-unopened-cross-project', documents=4)
        verify(call(c, 'textDocument/rename', p, rid=-7))
        verify(call(c, 'textDocument/rename', p, rid=0))
        verify(call(c, 'textDocument/rename', p, rid='0'))
        done('typed-unicode-string-zero-negative-integer-ids')
        extended = copy.deepcopy(p)
        extended.update(workDoneToken='progress-😀', futureExtension=dict(ignored=True))
        verify(call(c, 'textDocument/rename', extended))
        done('optional-progress-token-and-forward-compatible-unknown-fields')
        call(c, 'textDocument/prepareRename', {k: p[k] for k in ('textDocument', 'position')}, -32601)
        done('prepare-remains-explicitly-unimplemented')
        c.stop()
        c = start(modern=False)
        legacy = verify(call(c, 'textDocument/rename', p), modern=False)
        assert legacy == modern
        done('legacy-client-negotiation-matches-versioned-text-edits')
        c.stop()
        for name, change in [
            ('invalid-name', lambda v: v.update(newName='1bad')),
            ('reserved-name', lambda v: v.update(newName='func')),
            ('missing-document-uri', lambda v: v['textDocument'].clear()),
            ('negative-position', lambda v: v['position'].update(line=-1)),
            ('fractional-position', lambda v: v['position'].update(character=1.5)),
            ('invalid-progress-token', lambda v: v.update(workDoneToken={})),
            ('not-open-document', lambda v: v['textDocument'].update(uri=paths['closed'].as_uri())),
        ]:
            c = start()
            invalid = copy.deepcopy(p)
            change(invalid)
            call(c, 'textDocument/rename', invalid, -32602)
            done(name)
            c.stop()
        c = start()
        call(c, 'textDocument/rename', intent(new_name='occupied'), -32803)
        done('whole-scope-conflict-rejects-proposal')
        paths['isolated'].write_text('func broken(\n', encoding='utf8')
        call(c, 'textDocument/rename', p, -32803)
        paths['isolated'].write_bytes(bodies['isolated'].encode())
        done('invalid-unrelated-scope-rejects-proposal')
        c.stop()
        for root_metadata in [dict(rootUri=root.as_uri()), dict(rootPath=str(root)),
                              dict(workspaceFolders=[dict(uri=root.as_uri(), name='workspace')])]:
            c = start(config=None, opened=('loose',), **root_metadata)
            call(c, 'textDocument/rename', intent('loose', 'amount'), -32803)
            done('unknown-workspace-membership-never-collapses-to-open-roster', field=next(iter(root_metadata)))
            c.stop()
        c = start(config=None, opened=('loose',))
        result = call(c, 'textDocument/rename', intent('loose', 'amount'))
        assert len(result['documentChanges']) == 1 and len(result['documentChanges'][0]['edits']) == 2, result
        assert result['documentChanges'][0]['textDocument']['version'] == -2147483648, result
        done('standalone-session-without-catalog-or-private-protocol')
        c.stop()
        c = start(config=None, opened=('genericGlobal',))
        result = call(c, 'textDocument/rename', intent('genericGlobal', 'compute'))
        edits = result['documentChanges'][0]['edits']
        assert len(result['documentChanges']) == 1 and [e['range']['start']['line'] for e in edits] == [1, 3, 7], result
        done('uninstantiated-generic-global-binding-includes-body-reference')
        c.stop()
        c = start(config=None, opened=('genericDependent',))
        call(c, 'textDocument/rename', intent('genericDependent', 'amount'), -32803)
        done('dependent-member-with-incomplete-facts-declines-entire-edit')
        c.stop()
        # All invalidations are queued in the same pipe write as the request.
        for name, method, payload, code in [
            ('explicit-cancel', '$/cancelRequest', dict(id='active'), -32800),
            ('open-document-change', 'textDocument/didChange', dict(textDocument=dict(uri=paths['main'].as_uri(), version=1), contentChanges=[dict(text=bodies['main']+'\n')]), -32801),
            ('close-document', 'textDocument/didClose', dict(textDocument=dict(uri=paths['main'].as_uri())), -32801),
            ('configuration-change', 'workspace/didChangeConfiguration', dict(settings=dict(xlang=dict(analysisConfiguration=dict(epoch=2, catalog=catalog)))), -32801),
            ('workspace-roster-change', 'workspace/didChangeWorkspaceFolders', dict(event=dict(added=[], removed=[])), -32801),
        ]:
            c = start()
            batch(c, [('textDocument/rename', p, 'active'), (method, payload, None)])
            reply, _ = c.reply('active')
            assert reply.get('error', {}).get('code') == code, reply
            done(name+'-discards-pending-response')
            c.stop()
        c = start()
        c.send('workspace/didChangeWorkspaceFolders', dict(event=dict(added=[], removed=[])))
        call(c, 'textDocument/rename', p, -32803)
        c.send('workspace/didChangeConfiguration', dict(settings=dict(xlang=dict(
            analysisConfiguration=dict(epoch=2, catalog=catalog)))))
        verify(call(c, 'textDocument/rename', p))
        done('workspace-change-requires-fresh-complete-configuration')
        c.stop()
        c = start()
        state = call(c, 'xlang/suspendAnalysis', dict(token=1))
        assert state['held'] and not state['invalidated'] and not state['deferred'], state
        c.send('workspace/didChangeWorkspaceFolders', dict(event=dict(added=[], removed=[])))
        state = call(c, 'xlang/analysisLeaseState', {})
        assert state['held'] and state['invalidated'] and state['deferred'] == 1 and state['epoch'] == 1, state
        call(c, 'xlang/resumeAnalysis', dict(token=1))
        call(c, 'textDocument/rename', p, -32803)
        c.send('workspace/didChangeConfiguration', dict(settings=dict(xlang=dict(
            analysisConfiguration=dict(epoch=2, catalog=catalog)))))
        verify(call(c, 'textDocument/rename', p))
        done('workspace-folder-notification-invalidates-lease-and-replays-before-new-rename')
        c.stop()
        c = start()
        batch(c, [('textDocument/rename', p, 'duplicate'), ('textDocument/rename', p, 'duplicate')])
        reply, _ = c.reply('duplicate')
        assert reply.get('error', {}).get('code') == -32600, reply
        done('duplicate-active-id-cancels-original-without-second-response')
        c.stop()
        c = start()
        batch(c, [('textDocument/rename', p, n) for n in range(50, 55)] +
                 [('$/cancelRequest', dict(id=n), None) for n in range(50, 54)])
        replies = {}
        while len(replies) < 5:
            reply = c.next()
            if reply.get('id') in range(50, 55):
                assert reply['id'] not in replies, reply
                replies[reply['id']] = reply
        assert replies[54].get('error', {}).get('code') == -32603, replies
        assert all(replies[n].get('error', {}).get('code') == -32800 for n in range(50, 54)), replies
        done('four-request-capacity-and-independent-cancellation')
        c.stop()
        c = start()
        batch(c, [('textDocument/rename', p, 0), ('textDocument/rename', p, '0'),
                  ('$/cancelRequest', dict(id='0'), None)])
        replies = {}
        while len(replies) < 2:
            reply = c.next()
            if reply.get('id') in (0, '0'):
                key = (type(reply['id']).__name__, reply['id'])
                assert key not in replies, reply
                replies[key] = reply
        assert replies[('str', '0')].get('error', {}).get('code') == -32800, replies
        verify(replies[('int', 0)]['result'])
        done('same-spelling-string-and-integer-ids-cancel-independently')
        c.stop()
        c = start(config=None, backend='cli')
        call(c, 'textDocument/rename', p, -32601)
        done('cli-fallback-never-advertises-or-performs-native-rename')
        c.stop()
        report['status'] = 'passed'
    finally:
        paths['isolated'].write_bytes(bodies['isolated'].encode())
        report['stderr'] = [c.stop() for c in clients]
        report['transcripts'] = [c.transcript for c in clients]
        report['protectedUnchanged'] = all(hashlib.sha256(Path(p).read_bytes()).hexdigest() == digest
                                         for p, digest in protected.items())
        (out/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    assert report['protectedUnchanged'] and not any(report['stderr']), report['stderr']
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(out/'result.json')), ensure_ascii=False))


if __name__ == '__main__':
    main()
