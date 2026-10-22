"""Native diagnostics and checked navigation over stdio, without xl.exe.

Private source/temp directories, exact input hashes and complete transcripts.
Queries exercise the current explicit-check lifecycle; automatic analysis and
function/dependency incrementality remain separate delivery requirements.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import time
from check_language_snapshots import Client
from check_language_definitions import position


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    server, output = args.server.resolve(strict=True), args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    root = output / '中文 分析😀'
    root.mkdir()
    entry, dep, extra = root / 'main.xl', root / 'dep.xl', root / 'extra.xl'
    compiler = output / 'absent-toolchain' / 'xl.exe'
    original = b'func main(...) as int\n return 0\nend\n'
    entry.write_bytes(original)
    protected = {str(p): sha(p) for p in (server, Path(__file__), entry)}
    temporary = output / 'private-temp'
    temporary.mkdir()
    old_temp = {k: os.environ.get(k) for k in ('TEMP', 'TMP')}
    for k in old_temp:
        os.environ[k] = str(temporary)
    clients, cases, request_id = [], [], 100
    report = dict(status='failed', scope='native full semantic/typed-IR checks and explicit-check navigation; not automatic LSP or semantic reuse',
                  protected=protected, cases=cases)

    def done(name, **data):
        cases.append(dict(name=name, passed=True, **data))

    def client(backend=None):
        c = Client(server, compiler, root)
        clients.append(c)
        options = dict(checkOnSave=False,automaticDiagnostics=False)
        if backend is not None:
            options['diagnosticsBackend'] = backend
        c.send('initialize', dict(capabilities={}, initializationOptions=options), 1)
        reply, _ = c.reply(1)
        if backend == 'invalid':
            assert reply['error']['code'] == -32602, reply
            c.send('initialize', dict(capabilities={}, initializationOptions=dict(checkOnSave=False,automaticDiagnostics=False)), 2)
            assert 'result' in c.reply(2)[0]
        else:
            caps = reply['result']['capabilities']['experimental']
            native = backend != 'cli'
            assert caps['nativeSnapshotDiagnostics'] == native, caps
            assert caps['diagnosticsBackend'] == ('native' if native else 'cli'), caps
            assert caps['savedDiagnosticsBackend'] == 'cli', caps
        c.send('initialized', {})
        return c

    def check(c, version, errors, phase=None):
        nonlocal request_id
        request_id += 1
        c.check(entry, version, request_id)
        reply, messages = c.reply(request_id)
        assert 'error' not in reply, reply
        value = reply['result']
        assert value['version'] == version and value['count'] == errors and value['checkId'] == request_id, value
        assert value['ok'] == (errors == 0) and value['semanticDefinitions'] == (errors == 0), value
        assert value['guardScope'] == 'background-poll', value
        if phase:
            assert value['phase'] == phase, value
        return value, request_id, messages

    def query(c, method, path=entry, text=None, marker=None, batch=None, version=None):
        nonlocal request_id
        request_id += 1
        params = dict(textDocument=dict(uri=path.as_uri()))
        if text is not None:
            params['position'] = position(text, text.rindex(marker))
        if batch is not None:
            params['checkId'] = batch
        if version is not None:
            params['version'] = version
        c.send('textDocument/' + method, params, request_id)
        return c.reply(request_id)[0]

    def invalidated(c, batch):
        while True:
            message = c.next()
            if message.get('method') == 'xlang/diagnosticsInvalidated':
                assert message['params']['checkId'] == batch and message['params']['reason'], message
                return

    try:
        main = 'import dep\nfunc main(...) as int\n return dep.value()\nend\n'
        bad = 'export func value() as int\n string 中文 = "😀"; int x = missingName\n return 7\nend\n'
        good = 'export func value() as int\n return 7\nend\n'
        c = client()
        c.open(entry, main, 11)
        c.open(dep, bad, 27)
        value, batch, messages = check(c, 11, 1, 'sem')
        diagnostic = value['diagnostics'][0]
        assert Path(diagnostic['file']).resolve() == dep.resolve(), diagnostic
        assert diagnostic['range']['start'] == position(bad, bad.index('missingName')), diagnostic
        assert any(m.get('params', {}).get('uri') == dep.as_uri() and m['params'].get('version') == 27
                   and m['params'].get('diagnostics') for m in messages), messages
        assert not compiler.exists() and not dep.exists()
        done('native-unsaved-dependency-diagnostics-utf16-without-compiler')
        c.change(dep, good, 28)
        value, batch, _ = check(c, 11, 0, 'checked')
        definition = query(c, 'definition', text=main, marker='value', batch=batch, version=11)
        assert definition['result'] == dict(uri=dep.as_uri(), range=dict(
            start=position(good, good.index('value')), end=position(good, good.index('value') + 5))), definition
        hover = query(c, 'hover', text=main, marker='value')
        assert hover['result']['contents']['value'], hover
        outline = query(c, 'documentSymbol')
        assert isinstance(outline['result'], list) and any(v['name'] == 'main' for v in outline['result']), outline
        done('same-native-check-index-supplies-definition-hover-and-outline')
        c.change(dep, good + '\n', 29)
        stale = query(c, 'definition', text=main, marker='value', batch=batch)
        assert stale['error']['code'] == -32801, stale
        _, batch, _ = check(c, 11, 0)
        assert 'result' in query(c, 'definition', text=main, marker='value', batch=batch)
        done('edit-revokes-old-native-index-and-fresh-check-recovers')
        c.stop()

        c = client('native')
        for version, text, phase in [(1, 'func main(\n', 'parse'),
                                     (2, 'import native_missing_package\n', 'import'),
                                     (3, 'func main(...) as int\n return "bad"\nend\n', 'sem')]:
            if version == 1:
                c.open(entry, text, version)
            else:
                c.change(entry, text, version)
            value, _, _ = check(c, version, 1, phase)
            assert value['diagnostics'][0]['message'], value
        done('native-check-preserves-parse-import-and-type-error-phases')
        c.change(entry, original.decode(), 4)
        check(c, 4, 0, 'checked')
        done('native-error-recovery-produces-complete-index')
        c.stop()

        dep.write_bytes(b'\xef\xbb\xbf' + good.replace('\n', '\r\n').encode())
        saved = dep.read_bytes()
        for name, replace in [('bytes', False), ('identity', True)]:
            c = client()
            c.open(entry, main)
            value, batch, messages = check(c, 1, 0)
            assert any(m.get('params', {}).get('uri') == dep.as_uri() and
                       'version' not in m['params'] for m in messages), messages
            if replace:
                replacement = root / 'replacement.tmp'
                replacement.write_bytes(saved)
                os.replace(replacement, dep)
            else:
                dep.write_bytes(saved + b'\r\n')
            invalidated(c, batch)
            stale = query(c, 'definition', text=main, marker='value', batch=batch)
            assert stale['error']['code'] == -32801, stale
            done('native-disk-' + name + '-guard-invalidates-diagnostics-and-index')
            c.stop()
            dep.write_bytes(saved)

        c = client()
        large_valid = ''.join(f'func helper_{i}(int value) as int\n return value + 1\nend\n'
                              for i in range(1000)) + 'func main(...) as int\n return helper_999(7)\nend\n'
        c.open(entry, large_valid)
        value, batch, _ = check(c, 1, 0, 'checked')
        definition = query(c, 'definition', text=large_valid, marker='helper_999', batch=batch)
        at = large_valid.index('helper_999')
        assert definition['result'] == dict(uri=entry.as_uri(), range=dict(
            start=position(large_valid, at), end=position(large_valid, at + len('helper_999')))), definition
        done('large-native-index-keeps-complete-diagnostics-and-navigation')
        c.stop()

        # Full analysis and cursor projection own different trees on different
        # workers; protocol input and a small unrelated query stay responsive.
        c = client()
        small = 'func extra() as int\n int result = 7\n return res\nend\n'
        large = ''.join(f'func helper_{i}(int x) as int\n return x + 1\nend\n'
                        for i in range(2000)) + original.decode()
        c.open(entry, large)
        c.open(extra, small, 7)
        c.check(entry, 1, 9000)
        began = time.perf_counter_ns()
        c.send('native-analysis/ping', {}, 9001)
        ping, seen = c.reply(9001)
        assert ping['error']['code'] == -32601 and not any(v.get('id') == 9000 for v in seen), seen
        latency = (time.perf_counter_ns() - began) / 1e6
        c.send('$/cancelRequest', dict(id=9000))
        c.send('textDocument/completion', dict(textDocument=dict(uri=extra.as_uri()), version=7,
               position=position(small, small.rindex('res') + 3)), 9002)
        replies = {}
        while len(replies) < 2:
            message = c.next()
            if message.get('id') in (9000, 9002):
                replies[message['id']] = message
        cursor = replies[9002]
        assert cursor['result']['available'] and [v['label'] for v in cursor['result']['items']] == ['result'], cursor
        reply = replies[9000]
        assert reply['error']['code'] == -32800, reply
        done('native-static-and-cursor-workers-are-isolated-and-owner-responsive', pingReplyMs=latency)
        c.stop()

        c = client('cli')
        c.open(entry, original.decode())
        c.check(entry, 1, 2)
        assert c.reply(2)[0]['error']['code'] == -32603
        done('explicit-cli-diagnostics-do-not-silently-use-native')
        c.stop()
        c = client('invalid')
        c.stop()
        done('invalid-diagnostics-backend-rejected')
        assert all(sha(Path(p)) == value for p, value in protected.items())
        assert not compiler.exists() and not list(temporary.glob('xide-check-*'))
        done('sources-tools-and-private-temp-are-preserved')
        report['status'] = 'passed'
    finally:
        for c in clients:
            c.stop()
        for k, old in old_temp.items():
            if old is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = old
        report['exitCodes'] = [c.process.returncode for c in clients]
        if any(code != 0 for code in report['exitCodes']):
            report['status'] = 'failed'
        report['transcripts'] = [c.transcript for c in clients]
        report['stderr'] = [c.stderr for c in clients]
        (output / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    assert report['status'] == 'passed', report['exitCodes']
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(output / 'result.json'))))


if __name__ == '__main__':
    main()
