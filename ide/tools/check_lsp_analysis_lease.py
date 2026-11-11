"""Real stdio analysis suspension, ordered input replay and retained caches."""
import argparse
import hashlib
import json
from pathlib import Path
import time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_input_state import digest


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args(); root = args.output.resolve(); root.mkdir(parents=True, exist_ok=False)
    server = args.server.resolve(strict=True)
    sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
    protected = {str(f): sha(f) for f in (server, Path(__file__))}
    clients, cases = [], []; report = dict(status='failed', cases=cases, protected=protected)
    good = 'func main(...) as int\n int answer = 7\n return answer\nend\n'

    def done(name, **data): cases.append(dict(name=name, passed=True, **data))

    def client(name, automatic=False):
        folder = root / name; folder.mkdir()
        c = AutomaticClient(server, folder / 'absent/xl.exe', folder); clients.append(c)
        c.send('initialize', dict(initializationOptions=dict(automaticDiagnostics=automatic, checkOnSave=False, diagnosticDelay=0,
                                                            backgroundWorkspaceSymbols=automatic)), 1)
        reply, _ = c.reply(1); assert reply['result']['capabilities']['experimental']['analysisLease'] == 1, reply
        c.send('initialized', {}); return c, folder

    def error(c, method, params, code):
        c.request += 1; c.send(method, params, c.request); reply, _ = c.reply(c.request)
        assert reply['error']['code'] == code, reply

    def pause(c, token):
        r = c.call('xlang/suspendAnalysis', dict(token=token))
        assert r['token'] == token and r['held'] and not r['invalidated'] and not r['deferred'], r
        return r

    try:
        folder = root / 'before-initialize'; folder.mkdir(); c = AutomaticClient(server, folder / 'absent/xl.exe', folder); clients.append(c)
        error(c, 'xlang/suspendAnalysis', dict(token=1), -32002); c.stop(); done('initialization-required')
        c, folder = client('ordered'); entry, other = folder / '入口😀.xl', folder / 'other.xl'
        entry.write_text('// disk placeholder\n', encoding='utf-8'); other.write_text('// disk placeholder\n', encoding='utf-8')
        protected.update({str(f): sha(f) for f in (entry, other)}); c.open(entry, good)
        first = c.call('xlang/inputState'); fold = c.call('textDocument/foldingRange', dict(textDocument=dict(uri=entry.as_uri())))
        stats = c.call('xlang/analysisStats'); pause(c, 1)
        for value in ({}, {'token': 0}, {'token': -1}, {'token': True}, {'token': '2'}, {'token': 1.5}, {'token': 2, 'other': 0}):
            error(c, 'xlang/resumeAnalysis', value, -32602)
        error(c, 'xlang/resumeAnalysis', dict(token=2), -32602)
        error(c, 'xlang/suspendAnalysis', dict(token=2), -32001)
        c.send('$/cancelRequest', dict(id=c.request - 1))
        for method in ('initialize', 'xlang/configureAnalysis', 'textDocument/completion', 'textDocument/signatureHelp',
                       'textDocument/hover', 'textDocument/foldingRange', 'textDocument/references', 'workspace/symbol'):
            error(c, method, {}, -32001)
        assert c.call('xlang/analysisStats')['submitted'] == stats['submitted']
        assert c.call('xlang/inputState') == first
        done('strict-tokens-and-all-new-analysis-admission-blocked-including-reinitialize')
        catalog = dict(schema=1, projects=[], selectedProject='', packagePath='')
        c.batch([
            dict(jsonrpc='2.0', method='textDocument/didOpen', params=dict(textDocument=dict(uri=other.as_uri(), version=1, text=good, languageId='xlang'))),
            dict(jsonrpc='2.0', method='textDocument/didChange', params=dict(textDocument=dict(uri=entry.as_uri(), version=2), contentChanges=[dict(text=good.replace('7', '8'))])),
            dict(jsonrpc='2.0', method='textDocument/didChange', params=dict(textDocument=dict(uri=entry.as_uri(), version=3), contentChanges=[dict(text=good.replace('7', '9'))])),
            dict(jsonrpc='2.0', method='textDocument/didClose', params=dict(textDocument=dict(uri=other.as_uri()))),
            dict(jsonrpc='2.0', method='textDocument/didOpen', params=dict(textDocument=dict(uri=other.as_uri(), version=9, text=good, languageId='xlang'))),
            dict(jsonrpc='2.0', method='workspace/didChangeWatchedFiles', params=dict(changes=[dict(uri=other.as_uri(), type=2)])),
            dict(jsonrpc='2.0', method='workspace/didChangeConfiguration', params=dict(settings=dict(xlang=dict(analysisConfiguration=dict(epoch=1, catalog=catalog))))),
        ])
        state = c.call('xlang/analysisLeaseState')
        assert state['held'] and state['invalidated'] and state['deferred'] == 7 and state['epoch'] == 0, state
        notifications = [m['receive'] for m in c.transcript if m.get('receive', {}).get('method') == 'xlang/analysisLeaseInvalidated']
        assert len(notifications) == 1 and notifications[0]['params']['token'] == 1
        assert c.call('xlang/inputState') == first
        assert c.call('xlang/resumeAnalysis', dict(token=1)) is None
        replayed = c.call('xlang/inputState')
        assert replayed['sha256'] == digest({entry.as_uri(): 3, other.as_uri(): 9}), replayed
        assert replayed['documents'] == 2 and int(replayed['generation']) == int(first['generation']) + 6, replayed
        assert c.call('xlang/analysisLeaseState')['epoch'] == 1
        done('notifications-retained-once-and-replayed-in-stream-order-with-one-invalidation')
        error(c, 'xlang/suspendAnalysis', dict(token=1), -32602)
        error(c, 'xlang/resumeAnalysis', dict(token=1), -32602)
        folded = c.call('textDocument/foldingRange', dict(textDocument=dict(uri=entry.as_uri())))
        before = c.call('xlang/analysisStats'); ack = pause(c, 2)
        c.send('$/cancelRequest', dict(id=c.request))  # exact acknowledged request ID
        assert c.call('xlang/analysisLeaseState')['held']
        c.call('xlang/resumeAnalysis', dict(token=2))
        assert c.call('textDocument/foldingRange', dict(textDocument=dict(uri=entry.as_uri()))) == folded == fold
        after = c.call('xlang/analysisStats')
        assert after['parses'] == before['parses'] and after['cacheHits'] > before['cacheHits'], (before, after)
        assert after['workerThreadId'] == before['workerThreadId'], (before, after)
        done('late-cancel-cannot-release-acknowledged-lease-and-syntax-cache-actor-survives', acknowledgement=ack)
        c.check(entry, 3, 500); checked, _ = c.reply(500); assert checked['result']['count'] == 0, checked
        before = checked['result']['analysis']; pause(c, 3); c.call('xlang/resumeAnalysis', dict(token=3))
        c.check(entry, 3, 501); checked, _ = c.reply(501); after = checked['result']['analysis']
        assert after['builds'] == before['builds'] and after['cacheHits'] > before['cacheHits'], (before, after)
        done('complete-native-semantic-cache-survives-pause-resume', before=before['cacheHits'], after=after['cacheHits'])
        pause(c, 2**63 - 1); error(c, 'xlang/resumeAnalysis', dict(token=2), -32602)
        c.call('xlang/resumeAnalysis', dict(token=2**63 - 1)); error(c, 'xlang/suspendAnalysis', dict(token=2**63 - 1), -32602)
        c.stop(); done('exhausted-token-space-requires-new-connection')
        c, folder = client('automatic', True); entry, dep = folder / 'entry.xl', folder / 'dep.xl'
        body = 'import dep\nfunc main(...) as int\n return dep.value()\nend\n'
        entry.write_text('// overlay\n', encoding='utf-8'); dep.write_text('export func value() as int\n return 7\nend\n', encoding='utf-8')
        c.open(entry, body); c.quiet(); assert not c.latest[entry.as_uri()]['diagnostics']
        pause(c, 1); before = c.stats(); ws_before = c.call('xlang/workspaceSymbolStats')['background']
        # Longer than guard, diagnostic sweep and background refresh intervals.
        time.sleep(1.15)
        after = c.stats(); ws_after = c.call('xlang/workspaceSymbolStats')['background']
        assert not after['checking'] and not after['observing']
        assert after['submittedChecks'] == before['submittedChecks'] and after['completedOperations'] == before['completedOperations'], (before, after)
        assert ws_after['started'] == ws_before['started'] and not ws_after['running'], (ws_before, ws_after)
        dep.write_text('export func value() as int\n return "bad"\nend\n', encoding='utf-8')
        c.call('xlang/resumeAnalysis', dict(token=1)); after = c.quiet()
        assert c.latest[dep.as_uri()]['diagnostics'], (after, c.latest)
        done('automatic-diagnostics-and-idle-index-admission-held-and-closed-disk-input-revalidated-on-resume')
        # Closing a connection while held must still accept shutdown/exit.
        pause(c, 2); c.stop(); assert c.process.returncode == 0, c.process.returncode
        done('shutdown-while-held-exits-cleanly')
        assert all(sha(f) == h for f, h in protected.items()); report['status'] = 'passed'
    finally:
        for i, c in enumerate(clients):
            stderr = c.stop()
            (root / f'server-{i}.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2), encoding='utf-8')
            (root / f'server-{i}.stderr').write_text(stderr, encoding='utf-8')
        (root / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
        print(json.dumps(dict(status=report['status'], cases=len(cases))))


if __name__ == '__main__': main()
