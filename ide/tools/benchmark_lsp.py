"""Measure real completion round trips; do not confuse text sync with analysis reuse."""
import argparse
import hashlib
import json
import math
import os
import platform
import subprocess
import time
from pathlib import Path

from check_language_snapshots import Client


def sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def summary(samples):
    ordered = sorted(samples)
    def percentile(p):
        return round(ordered[max(0, math.ceil(p * len(ordered)) - 1)], 3)
    return dict(samples=len(samples), minMs=round(ordered[0], 3),
                p50Ms=percentile(.5), p95Ms=percentile(.95), p99Ms=percentile(.99),
                maxMs=round(ordered[-1], 3), rawMs=samples)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--samples', type=int, default=15)
    parser.add_argument('--functions', default='0,100')
    parser.add_argument('--require-cursor-reuse', action='store_true',
                        help='Verify real retained cursor build/hit counters on every sample.')
    args = parser.parse_args()
    functions = [int(n) for n in args.functions.split(',')]
    if not 3 <= args.samples <= 100 or not functions or len(set(functions)) != len(functions) or any(n < 0 or n > 10000 for n in functions):
        parser.error('Use 3..100 samples and 0..10000 functions per fixture.')
    server = args.server.resolve(strict=True)
    compiler = args.compiler.resolve(strict=True)
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    protected = [server, compiler,
                 compiler.with_name('xlc.dll'), compiler.with_name('xl.dll'),
                 Path(__file__), Path(__file__).with_name('check_language_snapshots.py')]
    if server.with_name('xge.dll').is_file():
        protected.append(server.with_name('xge.dll'))
    inputs = [dict(path=str(p), sha256=sha(p)) for p in protected]
    report = dict(format='xlang-lsp-roundtrip-benchmark-1', status='running',
                  scope='stdio completion, with exact-query counters when required; not UI latency or changed-function incrementality',
                  clock='time.perf_counter_ns', machine=dict(platform=platform.platform(),
                  processor=platform.processor(), logicalProcessors=os.cpu_count()), inputs=inputs, cases=[])
    report['gitRevision'] = subprocess.check_output(
        ['git', 'rev-parse', 'HEAD'], cwd=Path(__file__).resolve().parents[1], text=True).strip()
    try:
        for count in functions:
            root = output / ('functions-' + str(count))
            root.mkdir()
            entry = root / 'main.xl'
            prefix = ''.join('func helper_%d(int value) as int\n return value + 1\nend\n' % i
                             for i in range(count))
            text = prefix + 'func main(...) as int\n int result = 7\n return res\nend\n'
            # The disk entry is deliberately different: query must use the open buffer.
            disk = b'func main(...) as int\n return 0\nend\n'
            entry.write_bytes(disk)
            line = 3 * count + 2
            started = time.perf_counter_ns()
            client = Client(server, compiler, root)
            try:
                client.initialize()
                if args.require_cursor_reuse:
                    capabilities=client.transcript[1]['receive']['result']['capabilities']['experimental']
                    assert capabilities['retainedCursorCompletion'],capabilities
                initialized = time.perf_counter_ns()
                client.open(entry, text, 1)
                rows = dict(first=[], repeat=[], sameBodyVersion=[], edit=[])
                next_id = 100
                version = 1
                expected_builds,expected_hits,expected_worker,last_analysis=1,0,None,None
                def query(kind):
                    nonlocal next_id,expected_worker,last_analysis
                    request_id = next_id
                    next_id += 1
                    began = time.perf_counter_ns()
                    client.send('textDocument/completion', dict(
                        textDocument=dict(uri=entry.as_uri()), version=version,
                        position=dict(line=line, character=11)), request_id)
                    message, _ = client.reply(request_id)
                    finished = time.perf_counter_ns()
                    elapsed = (finished - began) / 1e6
                    assert 'error' not in message, message
                    result = message['result']
                    assert result['available'] and result['version'] == version, result
                    items = result['items']
                    assert len(items) == 1 and items[0]['label'] == 'result', items
                    assert items[0]['textEdit'] == dict(newText='result', range=dict(
                        start=dict(line=line, character=8), end=dict(line=line, character=11)))
                    if args.require_cursor_reuse:
                        stats=result['analysis']
                        assert stats['builds']==expected_builds and stats['cacheHits']==expected_hits,stats
                        assert stats['workerThreadId']>0 and stats['workerThreadId']!=stats['captureThreadId'],stats
                        assert stats['adapterThreadId'] not in (stats['workerThreadId'],stats['captureThreadId']),stats
                        if expected_worker is None:expected_worker=stats['workerThreadId']
                        assert expected_worker==stats['workerThreadId'],stats
                        last_analysis=stats
                    rows[kind].append(round(elapsed, 6))
                    # Keep only the current sample's transcript out of the measured interval.
                    client.transcript.clear()
                    return finished
                query('first')
                for _ in range(args.samples):
                    expected_hits+=1
                    query('repeat')
                for _ in range(args.samples):
                    version+=1;expected_hits+=1
                    began=time.perf_counter_ns();client.change(entry,text,version)
                    finished=query('sameBodyVersion')
                    rows['sameBodyVersion'][-1]=round((finished-began)/1e6,6)
                for n in range(args.samples):
                    version += 1
                    expected_builds+=1
                    updated = text.replace('result = 7', 'result = ' + str(n + 8))
                    began = time.perf_counter_ns()
                    client.change(entry, updated, version)
                    finished = query('edit')
                    # Edit samples include sending the actual change as well as the query.
                    rows['edit'][-1] = round((finished - began) / 1e6, 6)
                assert entry.read_bytes() == disk
                report['cases'].append(dict(functions=count, lines=text.count('\n'),
                    utf8Bytes=len(text.encode('utf8')), startupInitializeMs=round((initialized-started)/1e6, 3),
                    measurements={key:summary(value) for key,value in rows.items()},
                    retainedCursorCountersVerified=args.require_cursor_reuse,finalAnalysis=last_analysis,
                    verifiedCandidates=True))
            finally:
                client.stop()
                (root / 'server.stderr.log').write_text(client.stderr, encoding='utf8')
        for row in inputs:
            assert sha(row['path']) == row['sha256'], row['path']
        report['status'] = 'passed'
    except BaseException as error:
        report.update(status='failed', error=repr(error))
        raise
    finally:
        (output / 'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps(dict(status=report['status'], cases=len(report['cases']), proof=str(output)), ensure_ascii=False))


if __name__ == '__main__':
    main()
