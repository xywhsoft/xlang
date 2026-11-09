"""Validate categorized example tables and the paths actually selected by runners.

List mode never executes examples, starts Redis, or opens desktop/hardware APIs.
Historical acceptance reports and their original paths are deliberately excluded.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    root = args.root.resolve()
    runtime = root / 'release/win_x64'
    references = []
    for name in ('featured_extlibs', 'phase1_extlibs', 'platform_extlibs'):
        table = root / 'test' / (name + '.json')
        data = json.loads(table.read_text(encoding='utf-8-sig'))
        for item in data['packages']:
            # These Windows runner tables name real paths relative to extlibs/.
            # Do not let a basename search hide a stale table entry.
            path = runtime / 'examples/extlibs' / item['example']
            assert path.is_file(), (table, item['name'], path)
            references.append(str(path))
            for key in ('cSmoke', 'cExamples'):
                values = item.get(key, [])
                for value in ([values] if isinstance(values, str) else values):
                    if value:
                        path = root / value
                        # Package-owned C tests have their own gate; this check
                        # covers paths affected by the examples reorganization.
                        if path.is_relative_to(runtime / 'examples'):
                            assert path.is_file(), (table, item['name'], path)
                            references.append(str(path))
    runners = ('check_example_runtime_regressions.py', 'check_example_failure_regressions.py',
               'check_vcard_read_views.py', 'run_redis_live_example.py')
    unused = root / 'build/example-layout-list-only-unused'
    before = unused.exists()
    for name in runners:
        command = [sys.executable, str(root / 'tools' / name), '--runtime', str(runtime),
                   '--out', str(unused), '--list']
        if name == 'check_example_failure_regressions.py':
            command += ['--root', str(root), '--compiler', str(runtime / 'xl.exe')]
        result = subprocess.run(command, cwd=root, capture_output=True, timeout=30)
        assert result.returncode == 0, (name, result.stderr.decode(errors='replace'))
        paths = json.loads(result.stdout.decode('utf-8-sig'))
        assert paths and all(Path(path).is_file() for path in paths), name
        references.extend(paths)
    assert unused.exists() == before, '--list must not create an output directory'

    # Expand the suite's dynamic %%F list, which literal-path checks cannot see.
    suite = root / 'test/suites/10_format_extlibs.bat'
    text = suite.read_text(encoding='utf-8-sig')
    loops = re.findall(r'for %%F in \(([^()]*)\) do \(\s*release\\win_x64\\xl.exe run release\\win_x64\\examples\\extlibs\\%%F.xl', text, re.S | re.I)
    assert loops, 'Missing extension example execution loop'
    for group in loops:
        for name in group.split():
            path = runtime / 'examples/extlibs' / (name.replace('\\', '/') + '.xl')
            assert path.is_file(), (suite, path)
            references.append(str(path))
    unit = subprocess.run([sys.executable, str(root/'test/test_runtime_example_layout.py')],
                          cwd=root, capture_output=True, timeout=30)
    assert unit.returncode == 0, unit.stderr.decode(errors='replace')
    report = dict(passed=True, runtime=str(runtime), tableCount=3, runnerCount=len(runners),
                  references=len(references), uniqueSources=len(set(references)),
                  resolverTests=6, exampleProgramsExecuted=False, paths=references)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('Example layout ok:', len(references), 'references;', len(runners), 'runner lists; 6 resolver tests.')


if __name__ == '__main__':
    main()
