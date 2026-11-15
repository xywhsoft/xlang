"""Use the real directory gate variant to check owner and lease/job lifetime."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import time
from check_lsp_automatic_diagnostics import AutomaticClient


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args(); server, out = args.server.resolve(strict=True), args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    clients, gates, cases = [], [], []
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    report = dict(status='failed', serverSha256=sha(server), cases=cases, physicalDirectoryGate=True, performanceQualified=False)
    first, second = out/'first', out/'second'
    first.mkdir(); second.mkdir()
    (first/'a.xl').write_text('func firstJob() as int\n return 1\nend\n', encoding='utf8')
    (second/'b.xl').write_text('func secondJob() as int\n return 2\nend\n', encoding='utf8')
    folder = lambda p: dict(uri=p.as_uri(), name=p.name)

    def call(c, method, params, rid):
        c.send(method, params, rid); reply, _ = c.reply(rid); assert 'error' not in reply, reply
        return reply['result']
    def state(c): return c.call('xlang/workspaceDiscoveryState')
    def wait(c, predicate):
        end = time.monotonic()+20
        while True:
            value = state(c)
            if predicate(value): return value
            assert time.monotonic() < end, value
            time.sleep(.01)
    def start(label):
        gate = out/label; gates.append(gate)
        previous = os.environ.get('XLANG_TEST_WORKSPACE_GATE')
        try:
            os.environ['XLANG_TEST_WORKSPACE_GATE'] = str(gate)
            c = AutomaticClient(server, out/'absent-xl.exe', out)
        finally:
            if previous is None: os.environ.pop('XLANG_TEST_WORKSPACE_GATE', None)
            else: os.environ['XLANG_TEST_WORKSPACE_GATE'] = previous
        clients.append(c)
        call(c, 'initialize', dict(workspaceFolders=[folder(first)], capabilities={}, initializationOptions=dict(
            automaticDiagnostics=False, checkOnSave=False, backgroundWorkspaceSymbols=False)), 1)
        c.send('initialized', {})
        end = time.monotonic()+20
        marker = gate.with_suffix('.entered')
        while not marker.is_file():
            assert c.process.poll() is None and time.monotonic() < end
            time.sleep(.005)
        observation = json.loads(marker.read_text())
        value = state(c)
        assert observation['realDirectoryOpened'] and observation['thread'] != value['ownerThread']
        assert value['running'] and not value['idle'] and value['completed'] == 0
        return c, gate
    def release(gate): gate.with_suffix('.release').write_bytes(b'release')
    def done(name, **extra): cases.append(dict(name=name, passed=True, **extra))
    try:
        c, gate = start('lease-gate')
        c.send('workspace/symbol', dict(query='firstJob'), 'waiting')
        c.send('xlang/suspendAnalysis', dict(token=1), 'lease')
        rejected, _ = c.reply('waiting'); assert rejected.get('error', {}).get('code') == -32800, rejected
        lease = c.call('xlang/analysisLeaseState')
        assert not lease['held'] and state(c)['waitingBytes'] == 0
        done('suspension-does-not-acknowledge-an-open-discovery-directory')
        release(gate); held, _ = c.reply('lease')
        assert held['result']['held'] and state(c)['idle'], held
        done('physical-finalizer-completion-precedes-source-lease-acknowledgement')
        added = first/'new.xl'; added.write_text('func afterLeaseJob() as int\n return 3\nend\n', encoding='utf8')
        c.send('workspace/didChangeWatchedFiles', dict(changes=[dict(uri=added.as_uri(), type=1)]))
        pending = c.call('xlang/analysisLeaseState')
        assert pending['held'] and pending['invalidated'] and pending['deferred'] == 1
        assert not state(c)['running']
        done('held-lease-defers-watcher-input-without-starting-discovery')
        call(c, 'xlang/resumeAnalysis', dict(token=1), 'resume')
        rows = c.call('workspace/symbol', dict(query='afterLeaseJob'))
        assert {x['location']['uri'] for x in rows} == {added.as_uri()}, rows
        assert wait(c, lambda v: v['completed'] == 1 and v['idle'])['sources'] == 2
        done('resume-replays-input-and-discovers-the-latest-membership'); c.stop()

        c, gate = start('folder-gate')
        c.send('workspace/didChangeWorkspaceFolders', dict(event=dict(removed=[folder(first)], added=[folder(second)])))
        c.send('workspace/symbol', dict(query=''), 'new-scope')
        value = state(c); assert value['cancelled'] >= 1 and value['pending'] and not value['idle']
        release(gate); reply, _ = c.reply('new-scope'); assert 'error' not in reply, reply
        assert reply['result'] and {x['location']['uri'] for x in reply['result']} == {(second/'b.xl').as_uri()}, reply
        assert wait(c, lambda v: v['completed'] == 1 and v['idle'])['sources'] == 1
        done('cancelled-old-root-cannot-publish-over-new-root'); c.stop()

        c, gate = start('shutdown-gate')
        call(c, 'shutdown', {}, 'shutdown'); c.send('exit')
        assert c.process.poll() is None
        release(gate); assert c.process.wait(timeout=20) == 0
        done('shutdown-joins-the-real-discovery-worker-after-cancellation'); c.stop()
        report['status'] = 'passed'
    finally:
        for gate in gates: release(gate)
        for i, c in enumerate(clients):
            stderr = c.stop(); (out/f'client-{i}.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
            (out/f'client-{i}.stderr').write_text(stderr, encoding='utf8')
        assert sha(server) == report['serverSha256']
        (out/'result.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    print(json.dumps(dict(status='passed', cases=len(cases), output=str(out))))


if __name__ == '__main__':
    main()
