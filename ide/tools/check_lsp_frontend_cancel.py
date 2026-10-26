"""Real stdio cancellation, current-version recovery and shutdown fences.

Native fixtures prove interruption inside semantic/IR phases. This wire suite
proves the request remains pending at an owner barrier, then cancels it and
validates the next accepted model. It makes no internal-phase/timing claim.
"""
from pathlib import Path
import argparse
import hashlib
import json
import time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import source
from check_language_definitions import position


def params(method, path, body, version):
    result = dict(textDocument=dict(uri=path.as_uri()), version=version)
    if method.endswith('completion'):
        result['position'] = position(body, body.index('return done') + len('return done'))
    if method.endswith('signatureHelp'):
        result.update(position=position(body, body.index('"hello"') + 2),
                      context=dict(triggerKind=1, isRetrigger=False))
    return result


def validate(method, reply, version):
    assert 'error' not in reply, reply
    r = reply['result']
    assert r['version'] == version and r['analysis']['workerThreadId'] != r['analysis']['captureThreadId'], r
    if method == 'xlang/check':
        assert r['ok'] and not r['count'] and r['semanticDefinitions'], r
    elif method.endswith('completion'):
        assert any(x['label'] == 'doneLocal' for x in r['items']), r
    else:
        assert r['activeParameter'] == 1 and r['signatures'][r['activeSignature']]['label'] == 'choose(int left, string right) as int', r
    return r


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args()
    server = args.server.resolve(strict=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    tail = ('func choose(int left,string right) as int\r\n return left\r\nend\r\n'
            'func main(...) as int\r\n int doneLocal=choose(7,"hello")\r\n return doneLocal\r\nend\r\n')
    heavy = source(512, 0, True).split('export func compute(')[0] + tail
    small = '\ufeff// 中文😀\r\n' + tail
    cases, commands = [], []
    protected = {str(server): sha(server), str(Path(__file__)): sha(__file__)}
    report = dict(status='failed', scope=__doc__, cases=cases, commands=commands, protected=protected)
    try:
        for method in ['xlang/check', 'textDocument/completion', 'textDocument/signatureHelp']:
            for action in ['explicit', 'change', 'reopen', 'shutdown']:
                folder = out / (method.split('/')[-1] + '-' + action)
                folder.mkdir()
                path = folder / '编辑😀.xl'
                path.write_bytes(b'// unsaved overlays never modify disk\n')
                protected[str(path)] = sha(path)
                c = AutomaticClient(server, folder / 'absent/xl.exe', folder)
                try:
                    c.send('initialize', dict(capabilities={}, initializationOptions=dict(automaticDiagnostics=False, checkOnSave=False)), 1)
                    init, _ = c.reply(1)
                    assert init['result']['serverInfo']['version'] == '0.81', init
                    c.send('initialized', {})
                    c.open(path, heavy, 1)
                    c.send(method, params(method, path, heavy, 1), 10)
                    c.send('xlang/diagnosticStats', {}, 11)
                    barrier, seen = c.reply(11)
                    assert 'error' not in barrier and not any(x.get('id') == 10 for x in seen), seen
                    if action == 'explicit':
                        c.send('$/cancelRequest', dict(id=10))
                        code = -32800
                    elif action == 'change':
                        c.change(path, small, 2)
                        code = -32801
                    elif action == 'reopen':
                        c.send('textDocument/didClose', dict(textDocument=dict(uri=path.as_uri())))
                        code = -32800
                    else:
                        c.send('shutdown', {}, 12)
                        reply, events = c.reply(12)
                        assert 'error' not in reply, reply
                        old = next((x for x in events if x.get('id') == 10), None)
                        if old is None:
                            old, later = c.reply(10)
                            events += later
                        assert old.get('error', {}).get('code') == -32800, events
                        assert not any(x.get('params', {}).get('diagnostics') for x in events), events
                        c.send('exit')
                        c.process.stdin.close()
                        c.process.wait(timeout=10)
                        cases.append(dict(method=method, action=action, passed=True))
                        continue
                    cancelled, seen = c.reply(10)
                    assert cancelled.get('error', {}).get('code') == code, cancelled
                    assert not any(x.get('params', {}).get('diagnostics') for x in seen), seen
                    version = 1 if action == 'reopen' else 2
                    if action == 'reopen':
                        c.open(path, small, version)
                    elif action == 'explicit':
                        c.change(path, small, version)
                    until = time.monotonic() + 20
                    while True:
                        c.request += 1
                        c.send(method, params(method, path, small, version), c.request)
                        reply, _ = c.reply(c.request)
                        if reply.get('error', {}).get('code') != -32001:
                            break
                        assert time.monotonic() < until, reply
                        time.sleep(.001)
                    result = validate(method, reply, version)
                    # Repeated accepted facts must still use the current model.
                    c.request += 1
                    c.send(method, params(method, path, small, version), c.request)
                    again, _ = c.reply(c.request)
                    repeated = validate(method, again, version)
                    assert repeated['analysis']['builds'] == result['analysis']['builds'], (result, repeated)
                    cases.append(dict(method=method, action=action, version=version, passed=True, analysis=repeated['analysis']))
                finally:
                    stderr = c.stop()
                    (folder / 'stderr.log').write_text(stderr, encoding='utf8')
                    (folder / 'transcript.json').write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2), encoding='utf8')
                    commands.append(dict(server=str(server), exit=c.process.returncode))
                    assert not c.process.returncode
        assert len(cases) == 12 and all(sha(path) == digest for path, digest in protected.items())
        report['status'] = 'passed'
    finally:
        (out / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(out / 'result.json'))))


if __name__ == '__main__':
    main()
