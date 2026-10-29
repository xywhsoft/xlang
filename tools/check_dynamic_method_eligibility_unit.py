"""Run the current private IR unit with an explicitly pinned dependency cohort.

The test includes the production IR implementation itself. Dependency objects
resolve its other, unexercised compiler functions; this is not proof of a whole
compiler or a shipping product. Those need the independent product gate.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import shutil
import subprocess
import sys
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'dependency-build', 'runtime', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, out = args.root.resolve(), args.out.resolve()
    assert not out.exists(), 'Preserve previous evidence; use a fresh directory'
    dependency = json.loads(args.dependency_build.read_text(encoding='utf-8-sig'))
    assert dependency['Complete'] and dependency['InputsUnchanged']
    for row in dependency['Inputs'] + dependency['Headers'] + dependency['Objects']:
        assert sha(row['Path']) == row['Sha256'].lower(), row['Path']
    windows = os.name == 'nt'
    cc = Path(shutil.which('gcc')).resolve()
    runtime = args.runtime.resolve()
    assert any(Path(row['Path']).resolve() == runtime and row['Sha256'].lower() == sha(runtime)
               for row in dependency['Inputs'] + dependency['Headers'])
    objects = [row['Path'] for row in dependency['Objects'] if Path(row['Source']).name not in
               ('xl6_ir.c', 'xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    test = root / 'test/cases/xlir_dynamic_method_eligibility.c'
    driver = Path(dependency['Driver'])
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
             '-I' + str(root), '-I' + str(root / 'release'), '-I' + str(root / 'release/include'),
             '-I' + str(driver.parent / 'include'), '-Wno-unused-function', '-Wno-unused-variable',
             '-Wno-unused-parameter', '-Wno-unused-label', '-Wno-unused-but-set-variable',
             '-Wno-unused-const-variable', '-Wno-missing-field-initializers']
    links = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if windows else [
        '-pthread', '-ldl', '-lm', '-Wl,--export-dynamic', '-Wl,-rpath,' + str(runtime.parent)]
    if not windows:
        flags.append('-D_GNU_SOURCE')
    pinned = (Path(__file__).resolve(), Path(sys.executable), args.dependency_build.resolve(),
              runtime, cc, test, root / 'src/ir/xl6_ir.c')
    report = dict(Complete=False, Inputs={str(p): sha(p) for p in pinned}, Rows=[],
                  ShippingAccepted=False, CurrentIRImplementationIncluded=True,
                  DependencyBuild=str(args.dependency_build.resolve()), Scope=__doc__)
    out.mkdir(parents=True)
    if windows:
        shutil.copy2(runtime, out / runtime.name)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def run(name, command):
        command = list(map(str, command))
        actual = command
        if command[0] == str(cc):
            response = out / (name + '.rsp')
            response.write_text('\n'.join('"' + s.replace('\\', '/') + '"' for s in command[1:]), encoding='utf8')
            actual = [str(cc), '@' + str(response)]
        log = out / (name + '.log')
        started = time.monotonic()
        with log.open('wb') as stream:
            code = subprocess.run(actual, cwd=root, stdout=stream, stderr=subprocess.STDOUT, timeout=180).returncode
        report['Rows'].append(dict(Name=name, Command=command, Exit=code,
                                   Log=str(log), Sha256=sha(log), Seconds=time.monotonic() - started))
        save()
        print(name, code, flush=True)
        assert code == 0, str(log)

    for optimization in ('O0', 'O2'):
        obj = out / ('unit.' + optimization + '.o')
        binary = out / ('unit.' + optimization + ('.exe' if windows else ''))
        dependencies = out / ('unit.' + optimization + '.d')
        run(optimization + '.compile', [cc, *flags, '-' + optimization, '-MMD', '-MF', dependencies,
                                        '-c', test, '-o', obj])
        # GCC's dependency output escapes whitespace and drive-colons. Retain
        # the actual current headers used by this implementation unit, not
        # merely the dependency cohort's older header inventory.
        content = dependencies.read_text(encoding='utf8').replace('\\\n', ' ')
        _, separator, body = content.partition(': ')
        assert separator, dependencies
        for name in shlex.split(body):
            path = Path(name).resolve()
            digest = sha(path)
            assert str(path) not in report['Inputs'] or report['Inputs'][str(path)] == digest
            report['Inputs'][str(path)] = digest
        run(optimization + '.link', [cc, obj, *objects, runtime, *links, '-o', binary])
        run(optimization + '.run', [binary])
    report['InputsUnchanged'] = all(sha(p) == h for p, h in report['Inputs'].items())
    report['Complete'] = report['InputsUnchanged'] and len(report['Rows']) == 6
    report['Products'] = [dict(Path=str(p), Sha256=sha(p)) for p in out.rglob('*')
                          if p.is_file() and p.name != 'results.json']
    save()
    assert report['Complete']


if __name__ == '__main__':
    main()
