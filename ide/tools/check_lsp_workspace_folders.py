"""Native stdio root precedence, transactional folder lifecycle and scope guards."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from check_language_snapshots import Client
from check_lsp_analysis_configuration import LISTS, PROFILE


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    server, out = args.server.resolve(strict=True), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = out/'root 生命周期😀.xl'
    body = 'func main(...) as int\n int amount = 1\n return amount\nend\n'
    source.write_bytes(body.encode())
    root = dict(uri=out.as_uri(), name='workspace😀')
    other = dict(uri=(out/'other').as_uri(), name='other')
    lists = {name: [] for name in LISTS}
    lists['members'] = [source.as_posix()]
    context = dict(schema=1, kind='project', project=(out/'scope.xlp').as_posix(),
                   configuration='debug', entry=source.as_posix(), directory=out.as_posix(),
                   analysisProfile=PROFILE, targetKind='exe', packagePath=None, lists=lists)
    catalog = dict(schema=1, selectedProject=context['project'], packagePath=None, projects=[context])
    clients, cases, expected_logs = [], [], set()
    report = dict(status='failed', cases=cases, automaticDiscoveryImplemented=False,
                  performanceQualified=False, fullGoalComplete=False)
    protected = {str(f): hashlib.sha256(f.read_bytes()).hexdigest() for f in (server, source, Path(__file__))}
    serial = 100

    def done(name):
        cases.append(dict(name=name, passed=True))

    def call(c, method, params, code=None):
        nonlocal serial
        serial += 1
        c.send(method, params, serial)
        reply, _ = c.reply(serial)
        assert type(reply['id']) is int, reply
        if code is None:
            assert 'result' in reply and 'error' not in reply, reply
        else:
            assert reply.get('error', {}).get('code') == code, reply
        return reply.get('result')

    def params(config=None, **roots):
        options = dict(checkOnSave=False, automaticDiagnostics=False, backgroundWorkspaceSymbols=False)
        if config is not None:
            options['analysisConfiguration'] = dict(epoch=1, catalog=config)
        return dict(capabilities=dict(workspace=dict(workspaceEdit=dict(documentChanges=True)),
                    textDocument=dict(rename=dict(prepareSupport=True))), initializationOptions=options, **roots)

    def start(config=None, **roots):
        c = Client(server, out/'missing-xl.exe', out)
        clients.append(c)
        result = call(c, 'initialize', params(config, **roots))
        assert result['capabilities']['renameProvider'] == dict(prepareProvider=True)
        c.send('initialized', {})
        c.open(source, body, 1)
        return c

    point = dict(textDocument=dict(uri=source.as_uri()), position=dict(line=1, character=6))
    intent = dict(**point, newName='next_amount')
    refs = dict(**point, context=dict(includeDeclaration=True))

    def rename(c, code=None):
        result = call(c, 'textDocument/rename', intent, code)
        if code is None:
            changes = result['documentChanges']
            assert len(changes) == 1 and changes[0]['textDocument'] == dict(uri=source.as_uri(), version=1), result
            assert len(changes[0]['edits']) == 2 and all(x['newText'] == 'next_amount' for x in changes[0]['edits']), result
        return result

    def folders(c, added=(), removed=()):
        c.send('workspace/didChangeWorkspaceFolders', dict(event=dict(added=list(added), removed=list(removed))))

    def state(c):
        return call(c, 'xlang/inputState', {})

    def batch(c, method, payload, added=None, code=-32801):
        frames = []
        for method, params, rid in [(method, payload, 'pending-root'), ('workspace/didChangeWorkspaceFolders', dict(event=dict(added=[root] if added is None else added, removed=[])), None)]:
            v = dict(jsonrpc='2.0', method=method, params=params)
            if rid is not None:
                v['id'] = rid
            c.transcript.append(dict(send=v))
            data = json.dumps(v, ensure_ascii=False, separators=(',', ':')).encode()
            frames.append(f'Content-Length: {len(data)}\r\n\r\n'.encode()+data)
        c.process.stdin.write(b''.join(frames))
        c.process.stdin.flush()
        reply, _ = c.reply('pending-root')
        assert type(reply['id']) is str, reply
        if code is None:
            assert 'result' in reply and 'error' not in reply, reply
        else:
            assert reply.get('error', {}).get('code') == code, reply

    try:
        for label, roots in [
            ('absent', {}),
            ('empty-folders-win', dict(workspaceFolders=[], rootUri=out.as_uri(), rootPath=str(out))),
            ('null-folders-win', dict(workspaceFolders=None, rootUri=out.as_uri())),
            ('null-uri-wins-over-path', dict(rootUri=None, rootPath=str(out))),
            ('unused-legacy-field', dict(workspaceFolders=[], rootUri=4)),
        ]:
            c = start(**roots)
            rename(c)
            done(label+'-is-standalone')
            c.stop()
        for label, roots, removed in [
            ('root-path', dict(rootPath=str(out)), root),
            ('root-uri', dict(rootUri=out.as_uri(), rootPath=str(out/'wrong')), root),
            ('folder-array', dict(workspaceFolders=[root], rootUri=(out/'wrong').as_uri()), root),
        ]:
            c = start(**roots)
            rename(c, -32803)
            before = state(c)
            folders(c, removed=[other])
            assert state(c) == before
            rename(c, -32803)
            folders(c, removed=[removed])
            assert int(state(c)['generation']) == int(before['generation'])+1
            rename(c)
            done(label+'-last-removal-restores-standalone-without-catalog')
            c.stop()
        c = start(catalog)
        expected_logs.add(c)
        before = state(c)
        for payload in [dict(event=dict(added=[], removed=[])), dict(event=dict(added=[dict(uri='https://bad/root', name='bad')], removed=[])),
                        dict(event=dict(added=[dict(uri=root['uri'])], removed=[])), dict(event=dict(added=[], removed=None)),
                        dict(event=dict(added=[root, root], removed=[]))]:
            c.send('workspace/didChangeWorkspaceFolders', payload)
            assert state(c) == before
            rename(c)
        done('empty-and-invalid-notifications-preserve-valid-scope-and-generation')
        call(c, 'workspace/didChangeWorkspaceFolders', dict(event=dict(added=[root], removed=[])), -32600)
        assert state(c) == before
        rename(c)
        done('request-form-does-not-mutate-roots')
        folders(c, added=[root])
        assert int(state(c)['generation']) == int(before['generation'])+1
        for method, payload in [('textDocument/rename', intent), ('textDocument/prepareRename', point),
                                ('textDocument/references', refs), ('workspace/symbol', dict(query='main'))]:
            call(c, method, payload, -32803)
        done('real-folder-change-rejects-all-global-work-on-stale-configuration')
        call(c, 'xlang/configureAnalysis', dict(epoch=2, catalog=catalog))
        rename(c)
        assert isinstance(call(c, 'textDocument/references', refs), list)
        assert isinstance(call(c, 'workspace/symbol', dict(query='main')), list)
        done('new-complete-configuration-restores-global-work')
        before = state(c)
        folders(c, added=[dict(uri=root['uri']+'/', name='new display label')], removed=[other])
        assert state(c) == before
        rename(c)
        done('aliases-labels-known-additions-and-unknown-removals-are-semantic-noops')
        folders(c, added=[dict(uri='https://bad/root', name='bad')], removed=[root])
        assert state(c) == before
        rename(c)
        done('invalid-addition-does-not-partially-remove-existing-root')
        folders(c, removed=[root])
        rename(c, -32803)
        call(c, 'xlang/configureAnalysis', dict(epoch=3, catalog=catalog))
        rename(c)
        done('last-root-removal-requires-refresh-when-an-explicit-catalog-exists')
        c.stop()
        c = start(catalog)
        before = state(c)
        batch(c, 'textDocument/rename', intent, added=[], code=None)
        assert state(c) == before
        done('empty-folder-notification-does-not-cancel-a-pending-rename')
        c.stop()
        for method, payload in [('textDocument/prepareRename', point), ('textDocument/rename', intent),
                                ('textDocument/references', refs), ('workspace/symbol', dict(query='main'))]:
            c = start(catalog)
            batch(c, method, payload)
            done(method+'-pending-response-cancelled-by-real-folder-change')
            c.stop()
        c = start()
        call(c, 'xlang/suspendAnalysis', dict(token=1))
        folders(c, added=[root])
        lease = call(c, 'xlang/analysisLeaseState', {})
        assert lease['held'] and lease['invalidated'] and lease['deferred'] == 1, lease
        call(c, 'xlang/resumeAnalysis', dict(token=1))
        rename(c, -32803)
        folders(c, removed=[root])
        rename(c)
        done('analysis-lease-preserves-and-replays-the-root-roster-before-new-work')
        c.stop()
        c = Client(server, out/'missing-xl.exe', out)
        clients.append(c)
        bad = params(workspaceFolders=[root])
        bad['initializationOptions']['automaticDiagnostics'] = 'invalid'
        call(c, 'initialize', bad, -32602)
        call(c, 'initialize', params(workspaceFolders=[]))
        c.send('initialized', {})
        c.open(source, body, 1)
        rename(c)
        done('late-initialize-failure-does-not-leak-old-roots-into-retry')
        c.stop()
        for label, bad in [('missing-name', dict(workspaceFolders=[dict(uri=root['uri'])])),
                           ('nonfile-root', dict(rootUri='https://invalid/root')),
                           ('duplicate-native-path', dict(workspaceFolders=[root, dict(uri=root['uri']+'/', name='alias')])),
                           ('relative-root-path', dict(rootPath='relative')),
                           ('root-count-bound', dict(workspaceFolders=[dict(uri=(out/str(i)).as_uri(), name=str(i)) for i in range(65)])),
                           ('root-byte-bound', dict(workspaceFolders=[dict(uri=(out/str(i)).as_uri(), name='a'*65535) for i in range(32)]))]:
            c = Client(server, out/'missing-xl.exe', out)
            clients.append(c)
            call(c, 'initialize', params(**bad), -32602)
            call(c, 'initialize', params(workspaceFolders=[]))
            c.send('initialized', {})
            c.open(source, body, 1)
            rename(c)
            done(label+'-rejects-without-truncation-and-allows-clean-retry')
            c.stop()
        report['status'] = 'passed'
    finally:
        report['stderr'] = [c.stop() for c in clients]
        report['transcripts'] = [c.transcript for c in clients]
        report['protectedUnchanged'] = all(hashlib.sha256(Path(f).read_bytes()).hexdigest() == h for f, h in protected.items())
        allowed = {'xlang-ls automatic diagnostics: '+message for message in (
            'Only absolute file URIs are supported.',
            'Workspace folder requires a file URI and a UTF-8 display name.',
            'Workspace folder list must be an array within the root limit.',
            'Workspace folders contain duplicate paths or exceed metadata capacity.')}
        report['stderrExpected'] = all(not text or (c in expected_logs and all(line in allowed for line in text.splitlines()))
                                       for c, text in zip(clients, report['stderr']))
        if not report['protectedUnchanged'] or not report['stderrExpected']:
            report['status'] = 'failed'
        (out/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    assert report['protectedUnchanged'] and report['stderrExpected'], report['stderr']
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(out/'result.json')), ensure_ascii=False))


if __name__ == '__main__':
    main()
