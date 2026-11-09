"""Actual idle index refresh,coalescing,foreground preemption and input fences."""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
import time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_analysis_configuration import LISTS, PROFILE


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    root = args.output.resolve(); root.mkdir(parents=True, exist_ok=False)
    entry, helper, orphan = [root / name for name in ('main.xl', 'helper.xl', '未打开😀.xl')]
    body = 'import helper\nfunc main(...) as int\n return helper.value()\nend\n'
    library = '/* 中文😀 */\r\nclass Payload\r\n int number\r\nend\r\nexport func value() as int\r\n return 1\r\nend\r\n'
    dirty = library.replace('number', 'amount').replace('return 1', 'return 2')
    other = 'func orphanWork() as int\n return 7\nend\nfunc main(...) as int\n return orphanWork()\nend\n'
    for path, text in [(entry, body), (helper, library), (orphan, other)]: path.write_bytes(text.encode())
    lists = {name: [] for name in LISTS}; lists['members'] = [p.as_posix() for p in (entry, helper, orphan)]
    project = dict(schema=1, kind='project', project=(root / 'A.xlp').as_posix(), configuration='Debug',
                   entry=entry.as_posix(), directory=root.as_posix(), analysisProfile=PROFILE,
                   targetKind='exe', packagePath=None, lists=lists)
    def packet(epoch=1, projects=None):
        return dict(epoch=epoch, catalog=dict(schema=1, projects=copy.deepcopy([project] if projects is None else projects),
                    selectedProject='', packagePath=None))
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(p): sha(p) for p in (server, Path(__file__), Path(__file__).with_name('check_lsp_automatic_diagnostics.py'), Path(__file__).with_name('check_language_snapshots.py'))}
    clients, cases = [], []
    report = dict(status='failed', scope='Actual native stdio default idle refresh and explicit opt-out;no standalone warm-up protocol shortcut,no source guard bypass,no formal latency or RSS qualification.', protected=protected, cases=cases)
    def done(name, **data): cases.append(dict(name=name, passed=True, **data))
    def client(options=None, configuration=True, ready=True):
        c = AutomaticClient(server, root / 'absent/xl.exe', root); clients.append(c)
        opts = dict(automaticDiagnostics=False, checkOnSave=False)
        if configuration: opts['analysisConfiguration'] = packet()
        opts.update(options or {})
        c.send('initialize', dict(capabilities={}, initializationOptions=opts), 1)
        reply, _ = c.reply(1); assert 'error' not in reply, reply
        assert reply['result']['capabilities']['experimental']['workspaceSymbolBackgroundIndex'] is True, reply
        if ready: c.send('initialized', {})
        return c
    def stats(c): return c.call('xlang/workspaceSymbolStats')['background']
    def wait(c, predicate, timeout=30):
        until = time.monotonic() + timeout
        while True:
            current = stats(c)
            if predicate(current): return current
            assert time.monotonic() < until, current
            time.sleep(.01)
    def symbols(c, query): return c.call('workspace/symbol', dict(query=query))
    def watch(c, path): c.send('workspace/didChangeWatchedFiles', dict(changes=[dict(uri=path.as_uri(), type=2)]))
    big = ''.join(f'func warm{i:03}() as int\n return {i}\nend\n' for i in range(512)) + 'func main(...) as int\n return warm000()\nend\n'
    large = root / 'big.xl'; large.write_bytes(big.encode())
    try:
        c = client(ready=False); time.sleep(.65)
        assert stats(c)['started'] == 0
        c.send('initialized', {}, 50); rejected, _ = c.reply(50)
        assert rejected.get('error', {}).get('code') == -32600 and stats(c)['started'] == 0
        c.send('initialized', {}); first = wait(c, lambda s: s['completed'] == 1)
        assert first['enabled'] and first['roots'] == 3 and first['captured'] == 2 and first['covered'] == 1 and first['index.rows'] == 6, first
        assert not first['running'] and not first['pending'] and not first['failed'], first
        done('default-native-zero-open-refresh-waits-for-proper-initialized-notification', background=first)
        assert len(symbols(c, 'Payload number')) == 1
        work = c.call('xlang/workspaceSymbolStats'); assert work['reused'] == 2 and work['captured'] == 0, work
        time.sleep(.65); assert stats(c)['started'] == first['started']
        done('first-foreground-query-reuses-prebuilt-index-and-satisfies-idle-cohort', foreground=work)

        messages = [dict(jsonrpc='2.0', method='textDocument/didOpen', params=dict(textDocument=dict(uri=helper.as_uri(), languageId='xlang', version=1, text=library)))]
        messages += [dict(jsonrpc='2.0', method='textDocument/didChange', params=dict(textDocument=dict(uri=helper.as_uri(), version=version), contentChanges=[dict(text=dirty)])) for version in range(2, 21)]
        c.batch(messages); coalesced = wait(c, lambda s: s['completed'] == 2)
        assert coalesced['started'] == first['started'] + 1 and coalesced['captured'] == 1 and coalesced['reused'] == 1, coalesced
        assert len(symbols(c, 'Payload amount')) == 1 and symbols(c, 'Payload number') == []
        done('burst-open-and-nineteen-edits-coalesce-into-one-current-overlay-refresh', background=coalesced)
        c.send('textDocument/didClose', dict(textDocument=dict(uri=helper.as_uri())))
        closed = wait(c, lambda s: s['completed'] == 3)
        assert closed['captured'] == 1 and closed['reused'] == 1 and len(symbols(c, 'Payload number')) == 1, closed
        helper.write_bytes(dirty.encode()); watch(c, helper)
        watched = wait(c, lambda s: s['completed'] == 4)
        assert watched['captured'] == 1 and watched['reused'] == 1 and len(symbols(c, 'Payload amount')) == 1, watched
        done('overlay-close-and-watched-equal-length-disk-change-refresh-current-cohort', closed=closed, watched=watched)
        c.call('xlang/configureAnalysis', packet(2, [])); removed = wait(c, lambda s: s['completed'] == 5)
        assert removed['epoch'] == 2 and removed['index.rows'] == 0 and symbols(c, '') == [], removed
        done('empty-catalog-refresh-releases-old-cohort', background=removed); c.stop()
        helper.write_bytes(library.encode())

        entry.write_bytes('func main(...) as int\n return "invalid"\nend\n'.encode())
        c = client(); failed = wait(c, lambda s: s['failed'] == 1)
        assert failed['started'] == 1 and not failed['completed'] and failed['lastError'], failed
        time.sleep(1); assert stats(c)['started'] == failed['started']
        c.request += 1; c.send('workspace/symbol', dict(query='Payload number'), c.request); reply, _ = c.reply(c.request)
        assert reply.get('error', {}).get('code') == -32803, reply
        entry.write_bytes(body.encode()); watch(c, entry); repaired = wait(c, lambda s: s['completed'] == 1)
        assert repaired['failed'] == 1 and not repaired['lastError'] and len(symbols(c, 'Payload number')) == 1, repaired
        done('source-failure-one-attempt-without-timer-storm-and-watch-repair-recovers', failed=failed, repaired=repaired); c.stop()

        c = client(dict(backgroundWorkspaceSymbols=False)); c.open(helper, dirty)
        time.sleep(.75); disabled = stats(c); assert not disabled['enabled'] and disabled['started'] == 0, disabled
        assert len(symbols(c, 'Payload amount')) == 1
        assert stats(c)['started'] == 0
        done('explicit-opt-out-keeps-normal-on-demand-search', background=disabled); c.stop()

        c = client(configuration=False); c.open(large, big)
        running = wait(c, lambda s: s['running'])
        folded = c.call('textDocument/foldingRange', dict(textDocument=dict(uri=large.as_uri())))
        interrupted = stats(c); assert folded and interrupted['cancelled'] >= 1, interrupted
        changed = big.replace('warm000', 'fresh00'); c.change(large, changed, 2)
        current = wait(c, lambda s: s['completed'] >= 1 and not s['pending'] and not s['running'])
        assert current['index.rows'] == 513 and len(symbols(c, 'fresh00')) == 1 and symbols(c, 'warm000') == [], current
        done('real-512-function-background-task-preempted-by-foreground-and-edit-rebuilds-current-input', running=running, interrupted=interrupted, current=current); c.stop()

        c = client(dict(automaticDiagnostics=True), configuration=False)
        bad = 'func main(...) as int\n return missingName\nend\n'; c.open(large, bad)
        c.quiet(); failed = wait(c, lambda s: s['failed'] == 1)
        assert len(c.latest[large.as_uri()]['diagnostics']) == 1
        c.change(large, 'func main(...) as int\n return 0\nend\n', 2); c.quiet()
        recovered = wait(c, lambda s: s['completed'] >= 1)
        assert not c.latest[large.as_uri()]['diagnostics'] and not recovered['lastError'], recovered
        done('automatic-diagnostics-retain-priority-and-repair-before-idle-maintenance', background=recovered); c.stop()

        for options in [dict(backgroundWorkspaceSymbols='yes'), dict(backgroundWorkspaceSymbols=True, diagnosticsBackend='cli')]:
            c = AutomaticClient(server, root / 'absent/xl.exe', root); clients.append(c)
            c.send('initialize', dict(capabilities={}, initializationOptions=dict(checkOnSave=False, automaticDiagnostics=False, **options)), 1)
            rejected, _ = c.reply(1); assert rejected.get('error', {}).get('code') == -32602, rejected
            c.send('initialize', dict(capabilities={}, initializationOptions=dict(checkOnSave=False, automaticDiagnostics=False, backgroundWorkspaceSymbols=False)), 2)
            reply, _ = c.reply(2); assert 'error' not in reply, reply
            c.send('initialized', {}); c.stop()
        done('invalid-option-and-cli-background-request-rejected-before-initialization')

        c = client(configuration=False); c.open(large, big); active = wait(c, lambda s: s['running'])
        c.stop(); assert c.process.returncode == 0
        done('shutdown-cancels-and-joins-active-idle-work', background=active)
        assert all(sha(Path(p)) == h for p, h in protected.items())
        report['status'] = 'passed'
    finally:
        cleanup = []
        for index, c in enumerate(clients):
            stderr = c.stop(); (root / f'client-{index}.stderr.log').write_text(stderr, encoding='utf-8')
            (root / f'client-{index}.transcript.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2), encoding='utf-8')
            cleanup.append(dict(client=index, exit=c.process.returncode))
        report['clients'] = cleanup
        (root / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    assert all(row['exit'] == 0 for row in report['clients']), report['clients']
    print(f'workspace-symbol-refresh passed: {len(cases)} cases')


if __name__ == '__main__': main()
