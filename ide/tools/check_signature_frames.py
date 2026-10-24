"""Measure actual IDE parameter-hint frames; native facts remain the correctness oracle.

Modes: ordinary comma input with a changed argument, manual query after a source
edit, exact manual repetition, and caret retrigger while the window stays open.
Automatic diagnostics are disabled to isolate this path. These measurements do
not certify whole-project scale, display scanout, RSS or overall IDE performance.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
from check_lsp_module_semantics import source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--helpers', type=int, default=96)
    parser.add_argument('--samples', type=int, default=10)
    args = parser.parse_args()
    if not 1 <= args.helpers <= 512 or not 2 <= args.samples <= 120:
        parser.error('helpers must be 1..512 and samples 2..120')
    fixture = args.fixture.resolve(strict=True)
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    program = out/'mixed-signature.xl'
    body = source(args.helpers, 0, True).split('export func compute(')[0]
    body += ('#doc\r\ndesc: 参数提示性能检查😀\r\n#end\r\n'
        'func choose(int left,string right) as int\r\n return left\r\nend\r\n'
        'func choose(string left,string right) as int\r\n return 0\r\nend\r\n'
        'func main(...) as int\r\n return choose(7, "hello")\r\nend\r\n')
    program.write_bytes(body.encode('utf8'))
    protected = {str(p): sha(p) for p in [fixture, fixture.parent/'xlang-ls.exe',
        fixture.parent/'xge.dll', program, Path(__file__), Path(__file__).with_name('check_lsp_module_semantics.py')]}
    rows, cold, commands = [], [], []
    report = dict(status='failed', scope=__doc__, samples=rows, cold=cold,
        commands=commands, protected=protected, helpers=args.helpers, lines=body.count('\n'))
    try:
        for run, reverse in enumerate([0, 1, 1, 0]):
            command = [str(fixture), str(out/f'run-{run}'), str(args.compiler.resolve()),
                str(program), str(args.samples), str(reverse)]
            cp = subprocess.run(command, capture_output=True, timeout=600, cwd=fixture.parent)
            log = out/f'run-{run}.log'
            log.write_bytes(cp.stdout+cp.stderr)
            commands.append(dict(command=command, exit=cp.returncode, log=str(log), logSha256=sha(log)))
            assert cp.returncode == 0, (cp.stdout+cp.stderr).decode('utf8', 'replace')
            values = [json.loads(v) for v in cp.stdout.decode('utf8').splitlines() if v.startswith('{')]
            first = [v for v in values if 'coldMs' in v]
            measured = [v for v in values if 'mode' in v]
            assert len(first) == 1 and len(measured) == 4*args.samples
            cold.append(dict(run=run, **first[0]))
            for mode in range(4):
                subset = [v for v in measured if v['mode'] == mode]
                assert sorted(v['sample'] for v in subset) == list(range(args.samples))
                assert all(v['frames'] > 0 and v['documentVersion'] > 0 and v['renderSubmittedMs'] >= v['inputMs'] for v in subset)
            rows.extend(dict(run=run, **v) for v in measured)
            print(f'Signature frames run {run}: {len(measured)} measured and one cold request passed', flush=True)
        assert all(sha(p) == h for p, h in protected.items())
        def stats(values):
            a = sorted(values)
            return dict(samples=len(a), p50=a[math.ceil(.5*len(a))-1],
                p95=a[math.ceil(.95*len(a))-1], p99=a[math.ceil(.99*len(a))-1], maximum=a[-1])
        report['summary'] = {str(mode): {field: stats([v[field] for v in rows if v['mode'] == mode])
            for field in ['inputMs', 'renderSubmittedMs', 'maxFrameMs', 'guiCpuMs']} for mode in range(4)}
        report['status'] = 'passed'
    finally:
        (out/'result.json').write_text(json.dumps(report, ensure_ascii=False, indent=2)+'\n', encoding='utf8')
    print(json.dumps(dict(status=report['status'], samples=len(rows), proof=str(out/'result.json'))))


if __name__ == '__main__':
    main()
