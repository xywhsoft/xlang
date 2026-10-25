"""Verify actual retained semantic/IR analysis over stdio, without xl.exe.

Counters come from the XRT-owned checked session. Entry parsing can reuse
function chunks and eligible semantic facts; typed IR still rebuilds. This is not UI latency acceptance.
"""
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
    server, root = args.server.resolve(strict=True), args.output.resolve()
    root.mkdir(parents=True, exist_ok=False)
    entry, dep = root / '入口😀.xl', root / 'dep.xl'
    disk = b'// protected disk entry, analysis uses the open text\n'
    entry.write_bytes(disk)
    good = 'func main(...) as int\n int result = 7\n return result\nend\n'
    imported = 'import dep\nfunc main(...) as int\n return dep.value()\nend\n'
    dependency = 'export func value() as int\n return 7\nend\n'
    bad = 'export func value() as int\n return "bad"\nend\n'
    clients, cases, request = [], [], 10
    protected = {str(p): sha(p) for p in (server, Path(__file__), entry)}
    report = dict(status='failed', scope='actual retained full semantic/IR session, entry syntax incrementality and current-version adapters; not semantic/IR incrementality or UI latency',
                  protected=protected, cases=cases)

    def done(name, **data):
        cases.append(dict(name=name, passed=True, **data))

    def client():
        c = Client(server, root / 'absent-executor' / 'xl.exe', root)
        clients.append(c)
        c.initialize()
        caps = c.transcript[1]['receive']['result']['capabilities']['experimental']
        assert caps['retainedSemanticChecks'] and caps['retainedNativeInputs'] and caps['incrementalEntrySyntax'] and caps['incrementalFunctionSemantics'], caps
        return c

    def call(c, method, params):
        nonlocal request
        request += 1
        c.send(method, params, request)
        reply, seen = c.reply(request)
        assert 'error' not in reply, reply
        return reply['result'], seen

    def check(c, version, builds, errors=0):
        r, seen = call(c, 'xlang/check', dict(textDocument=dict(uri=entry.as_uri()), version=version))
        assert r['version'] == version and r['count'] == errors and r['ok'] == (errors == 0), r
        stats = r['analysis']
        assert stats['builds'] == builds and stats['workerThreadId'] != stats['captureThreadId'], stats
        assert stats['adapterThreadId'] not in (stats['workerThreadId'], stats['captureThreadId']), stats
        assert any(d['path'].lower() == str(entry).replace('\\', '/').lower() and d['version'] == version for d in r['documents']), r
        return r, seen

    try:
        c = client()
        c.open(entry, good, 1)
        r, _ = check(c, 1, 1)
        stats = r['analysis']
        assert stats['cacheHits'] == 0 and stats['documents'] == 1 and stats['textBytes'] == len(good.encode()), stats
        assert stats['entryFullParses'] == 1 and not stats['entryFragmentParses'] and not stats['entrySyntaxHits'], stats
        owner, _ = call(c, 'xlang/analysisStats', {})
        assert owner['ownerThreadId'] == stats['captureThreadId'], (owner, stats)
        done('real-semantic-thread-and-protocol-owner-identities', stats=stats)
        for i in range(10):
            r, _ = check(c, 1, 1)
            assert r['analysis']['cacheHits'] == i + 1, r
        done('ten-repeat-checks-reuse-checked-AST-IR-and-index')
        c.change(entry, good, 2)
        r, seen = check(c, 2, 1)
        assert r['analysis']['cacheHits'] == 11, r
        assert any(m.get('params', {}).get('uri') == entry.as_uri() and m['params'].get('version') == 2 for m in seen), seen
        definition, _ = call(c, 'textDocument/definition', dict(textDocument=dict(uri=entry.as_uri()), version=2,
            position=position(good, good.rindex('result'))))
        assert definition['range']['start'] == position(good, good.index('result')), definition
        done('same-body-new-version-rebinds-diagnostics-and-navigation')
        broken = 'func main(...) as int\n return missingName\nend\n'
        c.change(entry, broken, 3)
        r, _ = check(c, 3, 2, 1)
        assert r['analysis']['entryFullParses'] == 1 and r['analysis']['entryFragmentParses'] == 1, r
        r, _ = check(c, 3, 2, 1)
        assert r['analysis']['cacheHits'] == 12, r
        done('source-error-result-reused-without-false-success')
        c.change(entry, good, 4)
        r, _ = check(c, 4, 3)
        assert r['analysis']['entryFullParses'] == 1 and r['analysis']['entryFragmentParses'] == 2, r
        extra = root / 'extra.xl'
        c.open(extra, dependency, 1)
        r, _ = check(c, 4, 3)
        assert r['analysis']['documents'] == 2, r
        assert r['analysis']['entrySyntaxHits'] == 0 and r['analysis']['entryFullParses'] == 1 and r['analysis']['inputScopeHits'] == 1, r
        c.send('textDocument/didClose', dict(textDocument=dict(uri=extra.as_uri())))
        r, _ = check(c, 4, 3)
        assert r['analysis']['entrySyntaxHits'] == 0 and r['analysis']['inputScopeHits'] == 2, r
        done('unconsulted-input-set-add-and-close-retain-complete-model')
        c.change(entry, imported, 5)
        c.open(dep, dependency, 7)
        r, _ = check(c, 5, 4)
        assert r['analysis']['entryFullParses'] == 2, r
        c.change(dep, bad, 8)
        r, _ = check(c, 5, 5, 1)
        assert Path(r['diagnostics'][0]['file']).resolve() == dep, r
        assert r['analysis']['entrySyntaxHits'] == 1 and r['analysis']['entryFullParses'] == 2, r
        c.change(dep, dependency, 9)
        check(c, 5, 6)
        c.change(dep, dependency, 10)
        r, _ = check(c, 5, 6)
        assert next(d for d in r['documents'] if Path(d['path']).resolve() == dep)['version'] == 10, r
        done('unsaved-dependency-body-rebuild-and-same-body-version-rebind')
        c.stop()

        dep.write_bytes(dependency.encode())
        c = client()
        c.open(entry, imported, 1)
        check(c, 1, 1)
        replacement = root / 'replacement.tmp'
        replacement.write_bytes(dep.read_bytes())
        os.replace(replacement, dep)
        r, _ = check(c, 1, 2)
        assert r['analysis']['invalidations'] == 1, r
        assert r['analysis']['entrySyntaxHits'] == 1 and r['analysis']['entryFullParses'] == 1, r
        done('same-byte-physical-disk-replacement-invalidates')
        dep.write_bytes(bad.encode())
        r, _ = check(c, 1, 3, 1)
        assert r['analysis']['invalidations'] == 2, r
        done('disk-dependency-edit-rechecked')
        c.stop()

        c = client()
        missing = 'import new_dep\nfunc main(...) as int\n return new_dep.value()\nend\n'
        c.open(entry, missing, 1)
        r, _ = check(c, 1, 1, 1)
        assert r['phase'] == 'import', r
        (root / 'new_dep.xl').write_bytes(dependency.encode())
        r, _ = check(c, 1, 2)
        assert r['analysis']['invalidations'] == 1, r
        assert r['analysis']['entrySyntaxHits'] == 1 and r['analysis']['entryFullParses'] == 1, r
        done('negative-import-candidate-change-invalidates-cached-error')
        c.stop()

        c = client()
        prefix = ''.join('func helper_%d() as int\n int local = %d\n' % (i, i) + ' local = local + 1\n' * 12 + ' return local\nend\n' for i in range(100))
        tail = 'func tail() as int\n int trailing = 9\n' + ' trailing = trailing + 1\n' * 12 + ' return trailing\nend\n'
        def edited(i):
            return '\ufeff' + prefix + ('func main(...) as int\n string 文本 = "😀%d"\n%s int result = %d\n return result + tail()\nend\n' %
                (i, ' // extra shifted line\n' if i & 1 else '', i)) + tail
        body = edited(0)
        c.open(entry, body, 1)
        r, _ = check(c, 1, 1)
        initial_bytes = len(body.encode())
        assert r['analysis']['semanticFunctionsChecked'] == 102 and not r['analysis']['semanticFunctionsReused'], r
        for i in range(1, 16):
            body = edited(i)
            c.change(entry, body, i + 1)
            r, _ = check(c, i + 1, i + 1)
            stats = r['analysis']
            assert stats['entryFullParses'] == 1 and stats['entryFragmentParses'] == i and stats['entryReusedDeclarations'] == 101 * i, stats
            assert stats['entryParsedBytes'] < initial_bytes + 256 * i, stats
            assert stats['semanticFunctionsChecked'] == 102 + i and stats['semanticFunctionsReused'] == 101 * i, stats
            assert stats['semanticContextHits'] == i and 0 < stats['semanticMemoBytes'] <= 64 * 1024 * 1024, stats
            definition, _ = call(c, 'textDocument/definition', dict(textDocument=dict(uri=entry.as_uri()), version=i + 1,
                position=position(body, body.rindex('trailing'))))
            assert definition['range']['start'] == position(body, body.index('trailing')), (definition, i)
            definition, _ = call(c, 'textDocument/definition', dict(textDocument=dict(uri=entry.as_uri()), version=i + 1,
                position=position(body, body.index('tail()'))))
            assert definition['range']['start'] == position(body, body.rindex('tail()')), (definition, i)
        done('fifteen-real-local-entry-parses-with-full-IR-checks-and-rebased-navigation', stats=stats)
        done('actual-semantic-function-reuse-with-current-version-types-and-positions', checked=stats['semanticFunctionsChecked'], reused=stats['semanticFunctionsReused'])
        broken = body.replace('return result + tail()', 'return "unterminated')
        c.change(entry, broken, 17)
        r, _ = check(c, 17, 17, 1)
        assert r['phase'] == 'parse' and r['analysis']['entryFullParses'] == 2 and r['analysis']['entryFragmentParses'] == 16, r
        c.change(entry, body, 18)
        r, _ = check(c, 18, 18)
        assert r['analysis']['entryFullParses'] == 3, r
        c.send('textDocument/didClose', dict(textDocument=dict(uri=entry.as_uri())))
        c.open(entry, body, 1)
        r, _ = check(c, 1, 19)
        assert r['analysis']['entryFullParses'] == 4, r
        done('lexical-error-full-fallback-recovery-and-new-document-identity')
        c.stop()

        c = client()
        large = ''.join('func helper_%d(int value) as int\n return value + 1\nend\n' % i for i in range(1000)) + good
        c.open(entry, large, 1)
        request += 1
        check_id = request
        c.check(entry, 1, check_id)
        request += 1
        ping_id = request
        began = time.perf_counter_ns()
        c.send('unknown-retained-owner-probe', {}, ping_id)
        ping, seen = c.reply(ping_id)
        elapsed = (time.perf_counter_ns() - began) / 1e6
        assert ping['error']['code'] == -32601 and not any(m.get('id') == check_id for m in seen), seen
        reply, _ = c.reply(check_id)
        assert reply['result']['analysis']['builds'] == 1 and reply['result']['semanticDefinitions'], reply
        r, _ = check(c, 1, 1)
        assert r['analysis']['cacheHits'] == 1, r
        done('owner-dispatch-during-real-large-analysis-and-large-result-reuse', ownerRoundtripMs=elapsed)
        c.stop()
        assert entry.read_bytes() == disk and not (root / 'absent-executor').exists()
        assert protected == {p: sha(p) for p in protected}
        done('original-entry-and-native-materials-preserved-without-executor')
        report['status'] = 'passed'
    finally:
        for i, c in enumerate(clients):
            c.stop()
            (root / ('transcript-%02d.json' % i)).write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2), encoding='utf-8')
            (root / ('stderr-%02d.log' % i)).write_text(c.stderr, encoding='utf-8')
        (root / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(dict(status=report['status'], cases=len(cases), proof=str(root / 'result.json'))))


if __name__ == '__main__':
    main()
