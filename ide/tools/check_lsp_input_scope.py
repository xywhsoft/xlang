"""Verify dependency-scoped core reuse under unchanged full-stream input guards.

Actual stdio checks/completion/signature queries, current overlay envelopes,
global invalidation notifications and independently computed binding digests.
Includes standalone, opened project entry and closed configured entry.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
import time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_analysis_configuration import LISTS, PROFILE
from check_lsp_input_state import digest
from check_language_definitions import position


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server, output = args.server.resolve(strict=True), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected = {str(p): sha(p) for p in [server, Path(__file__), *[Path(__file__).with_name(n) for n in
        ['check_lsp_automatic_diagnostics.py', 'check_lsp_analysis_configuration.py', 'check_lsp_input_state.py',
         'check_language_snapshots.py', 'check_language_definitions.py']]]}
    cases, clients = [], []
    status = 'failed'
    try:
        for scenario in ['standalone', 'opened-project', 'disk-project']:
            directory = output/scenario
            directory.mkdir()
            configured, opened_entry = scenario != 'standalone', scenario != 'disk-project'
            entry = directory/'main.xl'
            active = directory/'active.xl' if configured else entry
            helper, extra, another = [directory/n for n in ['helper.xl', 'unrelated.xl', 'another.xl']]
            driver = 'import active\nfunc main(...) as int\n return active.value()\nend\n'
            body = ('\ufeffimport helper\r\nfunc plus(int left,string right) as int\r\n return left\r\nend\r\n' +
                ('export func value() as int\r\n' if configured else 'func main(...) as int\r\n') +
                ' int computed=helper.value()\r\n int doneLocal=plus(computed,"hello")\r\n return doneLocal\r\nend\r\n')
            api = 'export func value() as int\n return 7\nend\n'
            for path in {entry, active, helper, extra, another}:
                path.write_bytes(driver.encode() if path == entry and configured else b'// must remain unsaved\n')
                protected[str(path)] = sha(path)
            lists = {key: [] for key in LISTS}
            lists['members'] = [p.as_posix() for p in [entry, active, helper, extra, another]]
            lists['members'] = list(dict.fromkeys(lists['members']))
            context = dict(schema=1, kind='project', project=(directory/'project.xlp').as_posix(),
                configuration='debug', entry=entry.as_posix(), directory=directory.as_posix(),
                analysisProfile=PROFILE, targetKind='exe', packagePath='', lists=lists)
            packet = lambda epoch: dict(epoch=epoch, catalog=dict(schema=1, projects=[copy.deepcopy(context)],
                selectedProject=context['project'], packagePath=''))
            options = dict(automaticDiagnostics=False, checkOnSave=False)
            if configured:
                options['analysisConfiguration'] = packet(1)
            c = AutomaticClient(server, directory/'absent/xl.exe', directory)
            clients.append(c)
            c.send('initialize', dict(capabilities={}, initializationOptions=options), 1)
            initialized, _ = c.reply(1)
            assert initialized['result']['serverInfo']['version'] == '0.56', initialized
            c.send('initialized', {})
            versions, bodies = {}, {}
            def opened(path, text, version=1):
                c.open(path, text, version)
                versions[path], bodies[path] = version, text
            def changed(path, text):
                versions[path] += 1
                bodies[path] = text
                c.change(path, text, versions[path])
            if configured and opened_entry:
                opened(entry, driver)
            opened(active, body)
            opened(helper, api)
            opened(extra, '// first unrelated input')
            def done(name, **fields):
                cases.append(dict(name=scenario+'-'+name, passed=True, **fields))
            def call(method, params):
                until = time.monotonic()+20
                while True:
                    c.request += 1
                    c.send(method, params, c.request)
                    reply, _ = c.reply(c.request)
                    if method != 'xlang/check' or reply.get('error', {}).get('code') != -32001:
                        break
                    assert time.monotonic() < until, reply
                    time.sleep(.001)
                assert 'error' not in reply, reply
                return reply['result'], c.request
            epoch = 1
            def trio():
                checked, _ = call('xlang/check', dict(textDocument=dict(uri=active.as_uri()), version=versions[active]))
                assert checked['ok'] and not checked['count'] and checked['version'] == versions[active], checked
                groups = {Path(row['path']): row for row in checked['documents']}
                for path, version in versions.items():
                    normalized = bodies[path].replace('\r\n', '\n').replace('\r', '\n').encode()
                    group = groups[path]
                    assert group['version'] == version and group['origin'] == 'overlay', group
                    assert group['normalizedSize'] == len(normalized) and group['normalizedSha256'] == hashlib.sha256(normalized).hexdigest(), group
                completion, completion_id = call('textDocument/completion', dict(textDocument=dict(uri=active.as_uri()),
                    position=position(body, body.index('return done')+len('return done')), version=versions[active]))
                assert completion['version'] == versions[active] and any(v['label'] == 'doneLocal' for v in completion['items']), completion
                signature, signature_id = call('textDocument/signatureHelp', dict(textDocument=dict(uri=active.as_uri()),
                    position=position(body, body.index('"hello"')+2), version=versions[active], context=dict(triggerKind=1, isRetrigger=False)))
                assert signature['version'] == versions[active] and signature['queryId'] == signature_id and signature['activeParameter'] == 1, signature
                assert signature['signatures'][signature['activeSignature']]['label'] == 'plus(int left, string right) as int', signature
                if configured:
                    assert checked['entryOpen'] == opened_entry, checked
                    for value in [checked, completion, signature]:
                        stamp = value['analysisContext']
                        assert stamp['epoch'] == epoch and stamp['configuration'] == context['configuration'] and Path(stamp['entry']) == entry, stamp
                return [v['analysis'] for v in [checked, completion, signature]], [completion_id, signature_id]
            def receipt():
                value = c.call('xlang/inputState')
                assert value['documents'] == len(versions) and value['sha256'] == digest({p.as_uri(): v for p, v in versions.items()}), value
                return value
            stats, prior_ids = trio()
            assert all(v['builds'] == 1 and not v['inputScopeHits'] for v in stats), stats
            previous = receipt()
            done('initial-native-builds-and-current-source-proofs', analysis=stats)
            for i in range(6):
                start = len(c.transcript)
                changed(extra, f'\ufeff// 中文😀 unused generation {i}\r\n deliberately unfinished ({i}')
                current = receipt()
                assert current['sha256'] != previous['sha256'] and int(current['generation']) > int(previous['generation']), current
                stats, ids = trio()
                assert all(v['builds'] == 1 and v['inputScopeHits'] == i+1 for v in stats), stats
                notifications = [m['receive'] for m in c.transcript[start:] if 'receive' in m]
                for method, rid in zip(['xlang/completionInvalidated', 'xlang/signatureInvalidated'], prior_ids):
                    assert any(m.get('method') == method and m['params']['queryId'] == rid for m in notifications), (method, rid, notifications)
                done(f'unobserved-edit-{i}-core-hit-current-version-and-existing-invalidation', analysis=stats, inputState=current)
                previous, prior_ids = current, ids
            changed(extra, bodies[extra])
            current = receipt()
            stats, prior_ids = trio()
            assert all(v['builds'] == 1 and v['inputScopeHits'] == 6 for v in stats) and current['sha256'] != previous['sha256'], stats
            done('same-body-new-version-rebinds-current-envelope', analysis=stats)
            opened(another, '// new visible overlay')
            stats, _ = trio()
            assert all(v['builds'] == 2 and v['inputScopeHits'] == 6 for v in stats), stats
            done('open-membership-rebuilds', analysis=stats)
            c.send('textDocument/didClose', dict(textDocument=dict(uri=another.as_uri())))
            versions.pop(another); bodies.pop(another)
            stats, _ = trio()
            assert all(v['builds'] == 3 and v['inputScopeHits'] == 6 for v in stats), stats
            done('close-membership-rebuilds', analysis=stats)
            changed(helper, api.replace('return 7', 'return 8'))
            stats, _ = trio()
            assert all(v['builds'] == 4 and v['inputScopeHits'] == 6 for v in stats), stats
            done('actual-import-body-rebuilds-and-publishes-current-bytes', analysis=stats)
            c.send('textDocument/didClose', dict(textDocument=dict(uri=extra.as_uri())))
            versions.pop(extra); text = bodies.pop(extra)
            opened(extra, text, 1)
            stats, _ = trio()
            assert all(v['builds'] == 5 and v['inputScopeHits'] == 6 for v in stats), stats
            done('same-path-new-document-identity-rebuilds', analysis=stats)
            if configured:
                epoch = 2
                context['configuration'] = 'release'
                call('xlang/configureAnalysis', packet(epoch))
                stats, _ = trio()
                assert all(v['builds'] == 6 and v['inputScopeHits'] == 6 for v in stats), stats
                done('configuration-change-rebuilds-and-current-epoch', analysis=stats)
            receipt()
            c.stop()
        assert all(sha(p) == h for p, h in protected.items())
        status = 'passed'
    finally:
        for i, c in enumerate(clients):
            stderr = c.stop()
            (output/f'client-{i}-stderr.log').write_text(stderr, encoding='utf8')
            (output/f'client-{i}-transcript.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2), encoding='utf8')
        report = dict(status=status, scope=__doc__, cases=cases, protected=protected)
        (output/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(dict(status=status, cases=len(cases), evidence=str(output/'result.json'))))


if __name__ == '__main__':
    main()
