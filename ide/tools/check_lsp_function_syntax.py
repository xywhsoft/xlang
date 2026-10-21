"""Verify function-fragment parsing, current folding ranges and real work.

All edited results are compared with a fresh server's full parse. Timings
cover stdio update+folding, not static semantics/IR, cursor completion or UI.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time
from benchmark_lsp import summary
from check_language_snapshots import Client


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--functions', default='0,100,999')
    parser.add_argument('--samples', type=int, default=15)
    args = parser.parse_args()
    counts = [int(n) for n in args.functions.split(',')]
    if not counts or any(n < 0 or n > 10000 for n in counts) or not 3 <= args.samples <= 100:
        parser.error('Use 0..10000 helper functions and 3..100 samples.')
    server, root = args.server.resolve(strict=True), args.output.resolve()
    baseline = args.baseline.resolve(strict=True) if args.baseline else server
    root.mkdir(parents=True, exist_ok=False)
    protected = {str(p): sha(p) for p in (server, baseline, Path(__file__), Path(__file__).with_name('check_language_snapshots.py'))}
    clients, cases, request = [], [], 10
    report = dict(status='failed', scope='real syntax-fragment work and stdio folding/update latency; not semantic/IR incrementality or UI', inputs=protected, cases=cases)

    def open_client(exe, directory, entry, body, version):
        c = Client(exe, directory / 'no-executor' / 'xl.exe', directory)
        clients.append(c)
        c.initialize()
        c.open(entry, body, version)
        return c

    def call(c, method, params):
        nonlocal request
        request += 1
        c.send(method, params, request)
        m, _ = c.reply(request)
        assert 'error' not in m, m
        return m['result']

    def folding(c, entry, version):
        return call(c, 'textDocument/foldingRange', dict(textDocument=dict(uri=entry.as_uri()), version=version))

    def verify(actual, directory, entry, body, version):
        c = open_client(baseline, directory, entry, body, version)
        try:
            expected = folding(c, entry, version)
            assert actual == expected, (actual[:4], expected[:4])
        finally:
            c.stop()
        assert c.process.returncode == 0, c.stderr

    try:
        for count in counts:
            directory = root / ('helpers-%d' % count)
            directory.mkdir()
            entry = directory / '入口😀.xl'
            disk = b'// untouched disk bytes\n'
            entry.write_bytes(disk)
            prefix = ''.join('func helper_%d(int value) as int\n return value + 1\nend\n' % i for i in range(count))
            tail = 'func tail() as int\n return 9\nend\n'
            initial = prefix + 'func edited() as int\n int result = 7\n return result\nend\n' + tail
            def changed(i):
                return prefix + 'func edited() as int\n string 文本 = "😀-%d"\n%s int result = %d\n return result\nend\n' % (i, '\n' if i & 1 else '', i) + tail
            version, body = 1, initial
            c = open_client(server, directory, entry, body, version)
            rows = dict(first=[], repeat=[], edit=[])
            started = time.perf_counter_ns()
            ranges = folding(c, entry, version)
            rows['first'].append((time.perf_counter_ns() - started) / 1e6)
            verify(ranges, directory, entry, body, version)
            stats = call(c, 'xlang/analysisStats', {})
            assert stats['fullParses'] == 1 and not stats['fragmentParses'] and stats['parsedBytes'] == len(initial.encode()), stats
            assert stats['workerThreadId'] != stats['ownerThreadId'], stats
            for i in range(args.samples):
                started = time.perf_counter_ns()
                assert folding(c, entry, version) == ranges
                rows['repeat'].append((time.perf_counter_ns() - started) / 1e6)
            for i in range(args.samples):
                version += 1
                body = changed(i)
                started = time.perf_counter_ns()
                c.change(entry, body, version)
                ranges = folding(c, entry, version)
                rows['edit'].append((time.perf_counter_ns() - started) / 1e6)
                stats = call(c, 'xlang/analysisStats', {})
                assert stats['fullParses'] == 1 and stats['fragmentParses'] == i + 1 and stats['reusedDeclarations'] == (count + 1) * (i + 1), stats
                assert stats['parsedBytes'] < len(initial.encode()) + 256 * (i + 1), stats
                verify(ranges, directory, entry, body, version)
            version += 1
            c.change(entry, body, version)
            assert folding(c, entry, version) == ranges
            unchanged = call(c, 'xlang/analysisStats', {})
            assert unchanged['parses'] == stats['parses'] and unchanged['cacheHits'] == stats['cacheHits'] + 1, (stats, unchanged)

            # A broken lexical seam must use full parsing, not reuse a stale
            # suffix. Recovery also uses a full parse before local reuse.
            broken = body.replace('return result', 'return "unterminated')
            version += 1
            c.change(entry, broken, version)
            ranges = folding(c, entry, version)
            assert ranges == []
            verify(ranges, directory, entry, broken, version)
            errors = call(c, 'xlang/analysisStats', {})
            assert errors['fullParses'] == 2 and errors['fragmentParses'] == args.samples + 1, errors
            version += 1
            c.change(entry, body, version)
            ranges = folding(c, entry, version)
            verify(ranges, directory, entry, body, version)
            recovery = call(c, 'xlang/analysisStats', {})
            assert recovery['fullParses'] == 3, recovery
            c.send('textDocument/didClose', dict(textDocument=dict(uri=entry.as_uri())))
            c.open(entry, initial, 1)
            folding(c, entry, 1)
            reopened = call(c, 'xlang/analysisStats', {})
            assert reopened['fullParses'] == 4, reopened
            c.stop()
            assert c.process.returncode == 0 and entry.read_bytes() == disk, c.stderr
            cases.append(dict(helpers=count, initialLines=initial.count('\n'), measurements={k: summary(v) for k, v in rows.items()},
                              lastEditedAnalysis=stats, errorAnalysis=errors, reopenedAnalysis=reopened))
        assert protected == {p: sha(p) for p in protected}
        report['status'] = 'passed'
    finally:
        for i, c in enumerate(clients):
            c.stop()
            (root / ('transcript-%03d.json' % i)).write_text(json.dumps(c.transcript, ensure_ascii=False, indent=2), encoding='utf-8')
            (root / ('stderr-%03d.log' % i)).write_text(c.stderr, encoding='utf-8')
        (root / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    for case in cases:
        print(case['initialLines'], {k: v['p95Ms'] for k, v in case['measurements'].items()})


if __name__ == '__main__':
    main()
