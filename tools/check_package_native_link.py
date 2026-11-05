"""Real package-native-link parser qualification using the matched XRT.

Full xlang compiler, native archives and TCC GC execution are separate gates.
No runtime directories or installed package manifests are written.
"""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess, time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'runtime', 'out'):
        parser.add_argument('--' + name, required=True, type=Path)
    args = parser.parse_args()
    root, runtime, out = (v.resolve() for v in (args.root, args.runtime, args.out))
    assert not out.exists() and not out.is_relative_to(root / 'release')
    out.mkdir(parents=True)
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    inputs = [root / p for p in ('src/package/xl6_package.c', 'src/package/xl6_package.h',
        'src/package/xl6_package_native_link.inc', 'src/package/xl6_package_c_source_groups.inc',
        'src/compile/xl6_source_report.c', 'test/cases/package_native_link.c')]
    report = dict(complete=False, passed=False, scope=__doc__,
        inputs={str(p): sha(p) for p in [runtime, *inputs]}, rows=[])

    def run(name, command):
        start = time.monotonic()
        result = subprocess.run(list(map(str, command)), cwd=root, capture_output=True, timeout=180)
        log = out / (name + '.log')
        log.write_bytes(result.stdout + result.stderr)
        report['rows'].append(dict(name=name, command=list(map(str, command)),
            exitCode=result.returncode, seconds=round(time.monotonic() - start, 3), logSha256=sha(log)))
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
        assert not result.returncode, log

    import os
    if os.name == 'nt':
        shutil.copy2(runtime, out / runtime.name)
    cases = [('absent', None, 0), ('empty', {}, 0),
        ('all', dict(functionSections=True, dataSections=True, gcSections=True, printGcSections=True), 15),
        ('gc-only', dict(gcSections=True), 4),
        ('false', dict(functionSections=False, dataSections=False, gcSections=False, printGcSections=False), 0),
        ('unknown', dict(gcSection=True), -1), ('print-without-gc', dict(printGcSections=True), -1),
        ('nul-key', {'gcSections\0ignored': True}, -1), ('not-object', [], -1)]
    for field in ('functionSections', 'dataSections', 'gcSections', 'printGcSections'):
        for tag, value in [('int', 1), ('text', 'true'), ('null', None), ('array', [])]:
            cases.append((field + '-' + tag, {field: value}, -1))
    for opt in ('O0', 'O2'):
        program = out / (opt + ('.exe' if os.name == 'nt' else ''))
        command = ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-' + opt, '-I' + str(root),
            '-I' + str(runtime.parent), '-I' + str(runtime.parent / 'include'),
            root / 'test/cases/package_native_link.c', root / 'src/package/xl6_package.c',
            root / 'src/compile/xl6_source_report.c', runtime, '-o', program]
        if os.name != 'nt':
            command += ['-Wl,-rpath,' + str(runtime.parent), '-ldl', '-lpthread', '-lm']
        run(opt + '-build', command)
        for name, config, expected in cases:
            path = out / (name + '.json')
            data = dict(format='xlang-package-1', name='native_link_test', version='1.0.0', artifact={})
            if config is not None:
                data['artifact']['nativeLink'] = config
            path.write_text(json.dumps(data) + '\n')
            run(opt + '-' + name, [program, path, str(expected)])
    report.update(complete=True, passed=all(sha(Path(p)) == value for p, value in report['inputs'].items()))
    (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    assert report['passed']
    print('Package nativeLink parser:', len(cases), 'cases at O0/O2 passed')


if __name__ == '__main__':
    main()
