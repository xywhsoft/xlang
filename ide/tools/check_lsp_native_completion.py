"""Native semantic completion over real stdio, with no compiler executable.

Checks actual candidates, input guards, incremental edits, cancellation and
backend negotiation. Uses private source files and records the server hash.
This is not an IDE latency or semantic incrementality benchmark.
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
    root = output / '中文 原生😀'
    root.mkdir()
    entry, dep = root / 'main.xl', root / 'dep.xl'
    compiler = output / 'absent-toolchain' / 'xl.exe'
    disk = b'func main(...) as int\n return 0\nend\n'
    entry.write_bytes(disk)
    protected = {str(p): sha(p) for p in (server, Path(__file__), entry)}
    # Each child inherits a private temp root. Other test processes cannot
    # create/delete the snapshot files observed by this no-temp-file proof.
    temporary = output / 'private-temp'
    temporary.mkdir()
    previous_temp = {key: os.environ.get(key) for key in ('TEMP', 'TMP')}
    for key in previous_temp:
        os.environ[key] = str(temporary)
    before = {p.name for p in temporary.glob('xide-check-*')}
    clients, cases, request_id = [], [], 100
    report = dict(status='failed', scope='native stdio completion; not UI latency or semantic reuse',
                  protected=protected, cases=cases)

    def done(name, **data):
        cases.append(dict(name=name, passed=True, **data))

    def client(backend=None):
        c = Client(server, compiler, root)
        clients.append(c)
        options = dict(checkOnSave=False)
        if backend is not None:
            options['completionBackend'] = backend
        c.send('initialize', dict(capabilities={}, initializationOptions=options), 1)
        reply, _ = c.reply(1)
        if backend == 'invalid':
            assert reply['error']['code'] == -32602, reply
            # Retry a valid initialize before normal shutdown/exit.
            c.send('initialize', dict(capabilities={}, initializationOptions=dict(checkOnSave=False)), 2)
            initialized, _ = c.reply(2)
            assert 'result' in initialized, initialized
            c.send('initialized', {})
            return c
        caps = reply['result']['capabilities']['experimental']
        native = backend != 'cli'
        assert caps['nativeCursorCompletion'] == native, caps
        assert caps['completionBackend'] == ('native' if native else 'cli'), caps
        c.send('initialized', {})
        return c

    def request(c, text, marker, version, path=entry, entry_path=None):
        nonlocal request_id
        request_id += 1
        params = dict(textDocument=dict(uri=path.as_uri()), version=version,
                      position=position(text, text.rindex(marker) + len(marker)))
        if entry_path:
            params['entryDocument'] = dict(uri=entry_path.as_uri())
        c.send('textDocument/completion', params, request_id)
        return request_id

    def result(c, query_id, version, labels):
        reply, _ = c.reply(query_id)
        assert 'error' not in reply, reply
        value = reply['result']
        assert value['available'] and value['version'] == version and value['queryId'] == query_id, value
        assert {v['label'] for v in value['items']} == set(labels), value
        assert value['guardScope'] == 'background-poll', value
        return value

    def invalidated(c, query_id):
        while True:
            message = c.next()
            if message.get('method') == 'xlang/completionInvalidated':
                assert message['params']['queryId'] == query_id and message['params']['reason'], message
                return

    try:
        c = client()
        text = 'func main(...) as int\n int result = 7\n return res\nend\n'
        c.open(entry, text)
        value = result(c, request(c, text, 'res', 1), 1, ['result'])
        assert value['items'][0]['detail'] == 'int'
        assert not compiler.exists()
        done('default-native-local-type-without-compiler')
        for _ in range(5):
            result(c, request(c, text, 'res', 1), 1, ['result'])
        done('repeated-native-queries-remain-current')
        changed = text.replace('int result = 7', 'string result = "s"')
        c.send('textDocument/didChange', dict(textDocument=dict(uri=entry.as_uri(), version=2),
               contentChanges=[dict(range=dict(start=dict(line=1, character=1),
                                               end=dict(line=1, character=15)),
                                    text='string result = "s"')]))
        value = result(c, request(c, changed, 'res', 2), 2, ['result'])
        assert value['items'][0]['detail'] == 'string'
        done('ranged-edit-recomputes-semantic-type')
        unicode = '\ufefffunc main(...) as int\r\n string 结果 = "😀"\r\n return 结后缀\r\nend\r\n'
        c.change(entry, unicode, 3)
        value = result(c, request(c, unicode, '结', 3), 3, ['结果'])
        edit = value['items'][0]['textEdit']
        assert edit['range'] == dict(start=dict(line=2, character=8), end=dict(line=2, character=11)), edit
        done('bom-crlf-unicode-whole-token-edit')
        dependency = 'export func value() as int\n int member = 7\n return mem\nend\n'
        imported = 'import dep\nfunc main(...) as int\n return dep.value()\nend\n'
        c.change(entry, imported, 4)
        c.open(dep, dependency, 31)
        value = result(c, request(c, dependency, 'mem', 31, dep, entry), 31, ['member'])
        assert value['items'][0]['data']['declaration']['uri'] == dep.as_uri(), value
        assert not dep.exists()
        done('unsaved-import-target-and-entry-use-distinct-versions')
        c.stop()

        dep.write_bytes(b'export class Box\n int value\nend\n')
        saved = dep.read_bytes()
        members = 'import dep\nfunc main(...) as int\n auto box = dep.Box()\n return box.\nend\n'
        for name, replace in [('disk-bytes', False), ('disk-identity', True)]:
            c = client('native')
            c.open(entry, members)
            q = request(c, members, 'box.', 1)
            result(c, q, 1, ['value'])
            if replace:
                replacement = root / 'replacement.tmp'
                replacement.write_bytes(saved)
                os.replace(replacement, dep)
            else:
                dep.write_bytes(saved + b'\n')
            invalidated(c, q)
            done(name + '-invalidates-native-source-guard')
            c.stop()
            dep.write_bytes(saved)

        c = client()
        c.open(entry, text)
        q = request(c, text, 'res', 1)
        result(c, q, 1, ['result'])
        c.change(entry, text + '\n', 2)
        invalidated(c, q)
        result(c, request(c, text + '\n', 'res', 2), 2, ['result'])
        done('published-native-result-invalidates-and-recovers-on-edit')
        unavailable = 'func main(...) as int\n return missing.\nend\n'
        c.change(entry, unavailable, 3)
        reply, _ = c.reply(request(c, unavailable, 'missing.', 3))
        value = reply['result']
        assert not value['available'] and not value['items'] and value['error'], value
        done('unavailable-native-projection-is-explicit')
        c.stop()

        # Real expensive frontend work keeps the protocol owner responsive.
        large = ''.join(f'func helper_{i}(int value) as int\n return value + 1\nend\n'
                        for i in range(2000)) + text
        c = client()
        c.open(entry, large)
        q = request(c, large, 'res', 1)
        c.send('native-test/ping', {}, 9000)
        ping, seen = c.reply(9000)
        assert ping['error']['code'] == -32601 and not any(v.get('id') == q for v in seen), seen
        began = time.perf_counter_ns()
        c.send('$/cancelRequest', dict(id=q))
        reply, _ = c.reply(q)
        assert reply['error']['code'] == -32800, reply
        elapsed = (time.perf_counter_ns() - began) / 1e6
        done('native-semantic-work-does-not-block-owner-or-cancel', cancelReplyMs=elapsed)
        c.change(entry, text, 2)
        result(c, request(c, text, 'res', 2), 2, ['result'])
        c.send('native-test/barrier', {}, 9001)
        _, seen = c.reply(9001)
        assert not any(v.get('id') == q and 'result' in v for v in seen), seen
        done('cancelled-native-result-cannot-publish-late-success')
        c.stop()

        c = client('cli')
        c.open(entry, text)
        reply, _ = c.reply(request(c, text, 'res', 1))
        assert reply['error']['code'] == -32603, reply
        done('explicit-cli-oracle-does-not-silently-use-native')
        c.stop()
        c = client('invalid')
        c.stop()
        done('invalid-backend-option-rejected')
        assert all(sha(Path(p)) == h for p, h in protected.items())
        assert not compiler.exists()
        after = {p.name for p in temporary.glob('xide-check-*')}
        assert after == before, sorted(after - before)
        done('private-sources-tools-and-temp-snapshot-directory-unchanged')
        report['status'] = 'passed'
    finally:
        for c in clients:
            c.stop()
        for key, old in previous_temp.items():
            if old is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = old
        report['exitCodes'] = [c.process.returncode for c in clients]
        if any(code != 0 for code in report['exitCodes']):
            report['status'] = 'failed'
        report['transcripts'] = [c.transcript for c in clients]
        report['stderr'] = [c.stderr for c in clients]
        (output / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    assert all(c.process.returncode == 0 for c in clients), report['exitCodes']
    print(json.dumps(dict(status='passed', cases=len(cases), proof=str(output / 'result.json'))))


if __name__ == '__main__':
    main()
