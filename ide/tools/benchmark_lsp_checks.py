"""Measure verified full-check/index round trips and actual native reuse.

First/repeat/same-body-new-version/body-edit samples are separate. This does
not measure UI latency and does not assume changed-body semantic reuse.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import time
from benchmark_lsp import summary
from check_language_snapshots import Client


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--compiler', type=Path)
    parser.add_argument('--backend', choices=('native', 'cli'), default='native')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=15)
    parser.add_argument('--functions', default='0,100,1000')
    args = parser.parse_args()
    functions = [int(n) for n in args.functions.split(',')]
    if not 3 <= args.samples <= 100 or not functions or len(set(functions)) != len(functions) or any(n < 0 or n > 3000 for n in functions):
        parser.error('Use 3..100 samples and distinct 0..3000 function counts.')
    server, root = args.server.resolve(strict=True), args.output.resolve()
    compiler = args.compiler.resolve() if args.compiler else root / 'absent-executor' / 'xl.exe'
    if args.backend == 'cli' and not compiler.is_file():
        parser.error('CLI comparison requires an existing compiler.')
    root.mkdir(parents=True, exist_ok=False)
    protected = {str(p): sha(p) for p in (server, Path(__file__), Path(__file__).with_name('check_language_snapshots.py'), Path(__file__).with_name('benchmark_lsp.py'))}
    for p in (compiler, compiler.with_name('xlc.dll'), compiler.with_name('xl.dll')):
        if p.is_file():
            protected[str(p)] = sha(p)
    report = dict(status='failed', scope='stdio full semantic/IR check and index; not UI latency or changed-function incrementality',
                  backend=args.backend, inputs=protected, cases=[], machine=dict(platform=platform.platform(), logicalProcessors=os.cpu_count()))
    try:
        for count in functions:
            directory = root / ('functions-%d' % count)
            directory.mkdir()
            entry = directory / 'main.xl'
            disk = b'func main(...) as int\n return 0\nend\n'
            entry.write_bytes(disk)
            prefix = ''.join('func helper_%d(int value) as int\n return value + 1\nend\n' % i for i in range(count))
            def source(value):
                return prefix + 'func main(...) as int\n int result = %d\n return result\nend\n' % value
            body = source(7)
            client = Client(server, compiler, directory)
            rows = dict(first=[], repeat=[], sameBodyVersion=[], edit=[])
            version, request = 1, 10
            last_stats = None
            try:
                client.initialize(args.backend)
                client.open(entry, body, version)
                for kind, samples in (('first', 1), ('repeat', args.samples), ('sameBodyVersion', args.samples), ('edit', args.samples)):
                    for i in range(samples):
                        if kind in ('sameBodyVersion', 'edit'):
                            version += 1
                            if kind == 'edit':
                                body = source(i % 8)
                            client.change(entry, body, version)
                        request += 1
                        began = time.perf_counter_ns()
                        client.check(entry, version, request)
                        message, _ = client.reply(request)
                        elapsed = (time.perf_counter_ns() - began) / 1e6
                        assert 'error' not in message, message
                        result = message['result']
                        assert result['ok'] and not result['count'] and result['semanticDefinitions'] and result['version'] == version and result['phase'] == 'checked', result
                        assert len(result['documents']) == 1 and result['documents'][0]['version'] == version, result
                        if args.backend == 'native':
                            stats = result['analysis']
                            expected_builds = 2 + i if kind == 'edit' else 1
                            expected_hits = 0 if kind == 'first' else i + 1 if kind == 'repeat' else args.samples + i + 1 if kind == 'sameBodyVersion' else 2 * args.samples
                            assert stats['builds'] == expected_builds and stats['cacheHits'] == expected_hits, (kind, i, stats)
                            assert stats['workerThreadId'] != stats['captureThreadId'] and stats['documents'] == 1 and stats['textBytes'] == len(body.encode()), stats
                            last_stats = stats
                        rows[kind].append(elapsed)
                assert entry.read_bytes() == disk
            finally:
                client.stop()
                (directory / 'transcript.json').write_text(json.dumps(client.transcript, ensure_ascii=False, indent=2), encoding='utf-8')
                (directory / 'stderr.log').write_text(client.stderr, encoding='utf-8')
            assert client.process.returncode == 0, client.stderr
            report['cases'].append(dict(functions=count, lines=3 * count + 4,
                measurements={name: summary(values) for name, values in rows.items()}, finalAnalysis=last_stats))
        assert protected == {p: sha(p) for p in protected}
        report['status'] = 'passed'
    finally:
        (root / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    for case in report['cases']:
        print(case['lines'], {k: v['p95Ms'] for k, v in case['measurements'].items()})


if __name__ == '__main__':
    main()
