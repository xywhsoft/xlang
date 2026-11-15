"""Exercise default native workspace discovery through real stdio and disk changes."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import time
from check_lsp_automatic_diagnostics import AutomaticClient


class DiscoveryClient(AutomaticClient):
    def __init__(self, *args, decline_watch=False):
        self.decline_watch = decline_watch
        self.registrations = []
        super().__init__(*args)

    def next(self):
        value = super().next()
        if value.get('method') == 'client/registerCapability':
            assert value['id'] == 'xlang:workspace-discovery:watch', value
            rows = value['params']['registrations']
            assert len(rows) == 1 and rows[0]['method'] == 'workspace/didChangeWatchedFiles', rows
            assert rows[0]['registerOptions']['watchers'] == [dict(globPattern='**/*.{xl,xlp,xlw,c,h}', kind=7)], rows
            self.registrations.append(value)
            reply = dict(jsonrpc='2.0', id=value['id'])
            if self.decline_watch:
                reply['error'] = dict(code=-32601, message='Fixture declines dynamic watching')
            else:
                reply['result'] = None
            self.batch([reply])
        return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server, out = args.server.resolve(strict=True), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    clients, cases = [], []
    report = dict(status='failed', cases=cases, serverSha256=sha(server), sourceSha256=sha(Path(__file__)),
                  performanceQualified=False, fullGoalComplete=False)
    root = out/'工作区😀'; root.mkdir()
    entry, shared, closed, loose = [root/n for n in ('main.xl', 'shared.xl', 'closed.xl', 'loose.xl')]
    body = 'import shared\nfunc main(...) as int\n return shared.amount(1)\nend\n'
    declaration = 'export func amount(int value) as int\n return value\nend\n'
    closed_body = 'import shared\nfunc closedJob() as int\n return shared.amount(2)\nend\n'
    loose_body = 'func looseJob() as int\n return 3\nend\n'
    for p, text in ((entry, body), (shared, declaration), (closed, closed_body), (loose, loose_body)):
        p.write_bytes(text.encode())
    manifest = root/'应用.xlp'
    project = dict(format='xlang-project-1', entry=entry.name,
                   ide=dict(name='discovered', files=[dict(id='shared', path=shared.name), dict(id='closed', path=closed.name)]))
    manifest.write_text(json.dumps(project, ensure_ascii=False), encoding='utf8')
    folder = dict(uri=root.as_uri(), name='工作区😀')

    def done(name, **data): cases.append(dict(name=name, passed=True, **data))

    def start(roots=None, options=None, ready=True, watch=True, decline=False, package=None):
        opts = dict(automaticDiagnostics=False, checkOnSave=False, backgroundWorkspaceSymbols=False)
        opts.update(options or {})
        previous = os.environ.get('XLANG_PACKAGE_PATH')
        try:
            if package is not None: os.environ['XLANG_PACKAGE_PATH'] = package
            c = DiscoveryClient(server, out/'absent-xl.exe', out, decline_watch=decline)
        finally:
            if previous is None: os.environ.pop('XLANG_PACKAGE_PATH', None)
            else: os.environ['XLANG_PACKAGE_PATH'] = previous
        clients.append(c)
        c.send('initialize', dict(workspaceFolders=[folder] if roots is None else roots,
               capabilities=dict(workspace=dict(workspaceFolders=True, didChangeWatchedFiles=dict(dynamicRegistration=watch)),
                                 textDocument=dict(rename=dict(prepareSupport=True))), initializationOptions=opts), 1)
        reply, _ = c.reply(1)
        assert 'error' not in reply and reply['result']['capabilities']['experimental'].get('automaticWorkspaceDiscovery') is True, reply
        assert reply['result']['capabilities']['workspace']['workspaceFolders'] == dict(supported=True, changeNotifications=True)
        if ready: c.send('initialized', {})
        return c

    def state(c): return c.call('xlang/workspaceDiscoveryState')
    def wait(c, predicate, seconds=30):
        end = time.monotonic()+seconds
        while True:
            value = state(c)
            if predicate(value): return value
            assert time.monotonic() < end, value
            time.sleep(.01)
    def ready(c, completed=1): return wait(c, lambda v: v['completed'] >= completed and not v['dirty'] and v['idle'])
    def symbols(c, query): return c.call('workspace/symbol', dict(query=query))
    def request(c, method, params, rid, code=None):
        c.send(method, params, rid); reply, _ = c.reply(rid)
        if code is not None:
            assert reply.get('error', {}).get('code') in ((code,) if type(code) is int else code), reply
        else: assert 'error' not in reply, reply
        return reply
    def watch(c, path, kind=2):
        c.send('workspace/didChangeWatchedFiles', dict(changes=[dict(uri=path.as_uri(), type=kind)]))
    def point(text, word):
        before = text[:text.rindex(word)]
        return dict(line=before.count('\n'), character=len(before.rsplit('\n', 1)[-1].encode('utf-16-le'))//2+1)
    empty = lambda epoch: dict(epoch=epoch, catalog=dict(schema=1, selectedProject='', packagePath=None, projects=[]))
    try:
        c = start(ready=False)
        initial = state(c); assert initial['started'] == 0 and initial['dirty'] and not initial['pending'], initial
        done('initialize-does-not-scan-before-initialized')
        c.batch([dict(jsonrpc='2.0', method='initialized', params={}),
                 dict(jsonrpc='2.0', method='initialized', params={}),
                 dict(jsonrpc='2.0', id='early', method='workspace/symbol', params=dict(query='closedJob'))])
        reply, _ = c.reply('early'); assert 'error' not in reply and {x['location']['uri'] for x in reply['result']} == {closed.as_uri()}, reply
        first = ready(c)
        assert first['epoch'] == 0 and first['projects'] == 1 and first['manifests'] == 1 and first['sources'] == 4, first
        assert first['workerThread'] != first['ownerThread'] and first['waitingBytes'] == 0, first
        assert first['watchRegistered'] and len(c.registrations) == 1
        done('default-native-discovery-and-bounded-request-wait', work=first)
        assert {x['location']['uri'] for x in symbols(c, 'looseJob')} == {loose.as_uri()}
        done('closed-standalone-file-with-no-open-documents')
        c.open(entry, body, 1)
        position = point(body, 'amount')
        defs = c.call('textDocument/definition', dict(textDocument=dict(uri=entry.as_uri()), position=position))
        if isinstance(defs, dict): defs = [defs]
        assert len(defs) == 1 and defs[0]['uri'] == shared.as_uri(), defs
        done('discovered-project-context-drives-native-definition')
        params = dict(textDocument=dict(uri=entry.as_uri()), position=position, context=dict(includeDeclaration=True))
        refs = c.call('textDocument/references', params)
        assert {x['uri'] for x in refs} == {entry.as_uri(), closed.as_uri(), shared.as_uri()}, refs
        prepared = c.call('textDocument/prepareRename', dict(textDocument=params['textDocument'], position=position))
        assert prepared['placeholder'] == 'amount', prepared
        renamed = c.call('textDocument/rename', dict(textDocument=params['textDocument'], position=position, newName='quantity'))
        assert set(renamed['changes']) == {entry.as_uri(), closed.as_uri(), shared.as_uri()}, renamed
        done('automatic-membership-supports-complete-references-and-rename')
        virtual = root/'virtual.xl'; c.open(virtual, body, 2)
        assert {x['uri'] for x in c.call('textDocument/references', params)} == {entry.as_uri(), closed.as_uri(), shared.as_uri(), virtual.as_uri()}
        c.send('textDocument/didClose', dict(textDocument=dict(uri=virtual.as_uri())))
        done('virtual-unowned-overlay-joins-discovered-scopes')
        added = root/'新文件.xl'; added.write_text('func addedJob() as int\n return 7\nend\n', encoding='utf8')
        before = state(c)['completed']; watch(c, added, 1)
        assert {x['location']['uri'] for x in symbols(c, 'addedJob')} == {added.as_uri()}
        ready(c, before+1); done('create-event-rebuilds-closed-members-before-query')
        before = state(c)['started']; loose.write_text('func afterTextEdit() as int\n return 4\nend\n', encoding='utf8'); watch(c, loose)
        assert {x['location']['uri'] for x in symbols(c, 'afterTextEdit')} == {loose.as_uri()}
        assert state(c)['started'] == before; done('source-body-edit-does-not-rescan-metadata')
        ignored = root/'build/invalid.xlp'; ignored.parent.mkdir(); ignored.write_text('{broken', encoding='utf8')
        watch(c, ignored, 1); assert symbols(c, 'closedJob') and state(c)['started'] == before
        done('ignored-child-artifact-does-not-trigger-discovery')
        before = state(c)['completed']; project['ide']['name'] = 'changed metadata'
        manifest.write_text(json.dumps(project, ensure_ascii=False, indent=2), encoding='utf8'); watch(c, manifest)
        assert symbols(c, 'closedJob'); ready(c, before+1); done('raw-project-manifest-change-reloads-context')
        quiet = root/'unannounced.xl'; stamp = root.stat()
        quiet.write_text('func unannouncedJob() as int\n return 8\nend\n', encoding='utf8')
        os.utime(root, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))
        before = state(c)['completed']
        request(c, 'workspace/symbol', dict(query='unannouncedJob'), 'stale-proof', (-32801, -32803))
        ready(c, before+1); assert {x['location']['uri'] for x in symbols(c, 'unannouncedJob')} == {quiet.as_uri()}
        done('unannounced-membership-change-rejects-old-result-and-recovers')
        c.send('textDocument/didClose', dict(textDocument=dict(uri=entry.as_uri())))
        c.send('workspace/didChangeWorkspaceFolders', dict(event=dict(removed=[folder], added=[])))
        assert symbols(c, 'closedJob') == [] and ready(c)['projects'] == 0
        done('last-root-removal-clears-discovered-closed-scopes')
        c.send('workspace/didChangeWorkspaceFolders', dict(event=dict(removed=[], added=[folder])))
        assert symbols(c, 'closedJob'); assert ready(c)['projects'] == 1
        done('folder-addition-restores-automatic-workspace')
        request(c, 'xlang/configureAnalysis', empty(1), 'manual-1')
        manual = state(c); assert manual['mode'] == 'explicit' and manual['epoch'] == 1
        assert symbols(c, 'closedJob') == []
        watch(c, manifest); assert state(c)['started'] == manual['started']
        done('explicit-epoch1-preempts-automatic-catalog-without-collision')
        c.send('workspace/didChangeWorkspaceFolders', dict(event=dict(removed=[folder], added=[])))
        request(c, 'workspace/symbol', dict(query=''), 'explicit-dirty', -32803)
        request(c, 'xlang/configureAnalysis', empty(2), 'manual-2')
        assert symbols(c, '') == [] and state(c)['epoch'] == 2
        done('explicit-mode-retains-acknowledged-configuration-lifecycle'); c.stop()

        c = start(ready=False)
        c.batch([dict(jsonrpc='2.0', method='initialized', params={}),
                 dict(jsonrpc='2.0', id='pending', method='workspace/symbol', params=dict(query='closedJob')),
                 dict(jsonrpc='2.0', id='override', method='xlang/configureAnalysis', params=empty(1))])
        a, _ = c.reply('pending'); b, _ = c.reply('override')
        assert a.get('error', {}).get('code') == -32801 and 'error' not in b and state(c)['mode'] == 'explicit'
        assert state(c)['waitingBytes'] == 0
        done('explicit-configuration-cancels-pending-automatic-request'); c.stop()

        c = start(ready=False)
        c.batch([dict(jsonrpc='2.0', method='initialized', params={}),
                 dict(jsonrpc='2.0', id=0, method='workspace/symbol', params=dict(query='closedJob')),
                 dict(jsonrpc='2.0', method='$/cancelRequest', params=dict(id=0))])
        reply, _ = c.reply(0); assert reply.get('error', {}).get('code') == -32800
        assert ready(c)['waitingBytes'] == 0
        done('queued-request-cancellation-retains-id-zero'); c.stop()

        missing = out/'initially missing'; missing_folder = dict(uri=missing.as_uri(), name='missing')
        c = start(roots=[missing_folder])
        request(c, 'workspace/symbol', dict(query=''), 'missing-root', -32803)
        failed = state(c); assert failed['failed'] and failed['completed'] == 0 and failed['lastError']
        missing.mkdir(); recovery = missing/'recover.xl'; recovery.write_text('func recoveredJob() as int\n return 0\nend\n', encoding='utf8')
        assert {x['location']['uri'] for x in symbols(c, 'recoveredJob')} == {recovery.as_uri()}
        done('missing-root-fails-closed-and-explicit-query-retries-after-repair'); c.stop()

        c = start(decline=True); assert symbols(c, 'closedJob') and not ready(c)['watchRegistered']
        assert len(c.registrations) == 1 and any('declined' in x['message'] for x in c.logs)
        done('declined-watch-registration-does-not-block-native-discovery'); c.stop()

        c = start(options=dict(automaticWorkspaceDiscovery=False))
        assert state(c)['mode'] == 'disabled' and state(c)['started'] == 0
        request(c, 'textDocument/rename', dict(textDocument=dict(uri=entry.as_uri()), position=point(body, 'amount'), newName='x'), 'disabled-no-open', -32602)
        assert not c.registrations; done('explicit-opt-out-does-not-scan-or-register'); c.stop()

        c = start(options=dict(backgroundWorkspaceSymbols=True)); ready(c)
        until = time.monotonic()+15
        while True:
            state(c)  # Read-only discovery observation must not restart quiet time.
            background = c.call('xlang/workspaceSymbolStats')['background']
            if background['completed'] >= 1: break
            assert time.monotonic() < until, background
            time.sleep(.01)
        done('discovery-state-observation-does-not-starve-background-index'); c.stop()

        for label, package in [('empty', ''), ('nonempty', str(out/'private-package-root'))]:
            c = start(package=package); assert symbols(c, 'closedJob') and ready(c)['projects'] == 1
            done('captured-package-'+label+'-uses-correct-allocation-lifetime'); c.stop()
        c = start(roots=[], package=str(out/'private-package-root'))
        c.open(loose, loose.read_text(encoding='utf8'), 1)
        assert symbols(c, 'afterTextEdit') and state(c)['started'] == 0
        done('nonempty-package-without-workspace-keeps-standalone-background-scope'); c.stop()
        external = root/'extlibs/declared'; external.mkdir(parents=True)
        external_entry = external/'main.xl'
        external_entry.write_text('func externalJob() as int\n return 9\nend\nfunc main(...) as int\n return externalJob()\nend\n', encoding='utf8')
        (external/'outside.xlp').write_text(json.dumps(dict(format='xlang-project-1', entry='main.xl')), encoding='utf8')
        workspace = dict(format='xlang-workspace-1', name='discovered workspace',
                         projects=[dict(id='app', path=manifest.name), dict(id='outside', path='extlibs/declared/outside.xlp')],
                         files=[dict(id='loose', path=loose.name)])
        (root/'active.xlw').write_text(json.dumps(workspace, ensure_ascii=False), encoding='utf8')
        c = start(); assert {x['location']['uri'] for x in symbols(c, 'externalJob')} == {external_entry.as_uri()}
        scope = ready(c); assert scope['projects'] == 2 and scope['manifests'] == 3, scope
        done('workspace-manifest-loads-explicit-project-under-ignored-parent', work=scope)
        outside_manifest = external/'outside.xlp'
        outside_manifest.write_text(json.dumps(dict(format='xlang-project-1', entry='main.xl', ide=dict(name='renamed external'))), encoding='utf8')
        watch(c, outside_manifest)
        assert symbols(c, 'externalJob') and ready(c, scope['completed']+1)['projects'] == 2
        done('explicit-project-watch-overrides-ignored-parent-filter'); c.stop()
        report['status'] = 'passed'
    finally:
        for i, c in enumerate(clients):
            stderr = c.stop(); (out/f'client-{i}.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
            (out/f'client-{i}.stderr').write_text(stderr, encoding='utf8')
        assert sha(server) == report['serverSha256']
        (out/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps(dict(status='passed', cases=len(cases), output=str(out))))


if __name__ == '__main__':
    main()
