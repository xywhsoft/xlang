"""Offline compiler-stage failure diagnostics and context recovery acceptance.

Links unchanged full-source compiler objects with four dependency --wrap hooks.
No production fault switches, TCC execution, network calls or automatic retries.
This proves diagnostic/rollback contracts, NOT the cause of an intermittent
historical compilation failure. Source-product only, not shipping acceptance.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument('--source-build', type=Path, required=True)
    parser.add_argument('--native-root', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    manifest = args.source_build.resolve()
    out = args.out.resolve()
    build = json.loads(manifest.read_text(encoding='utf-8-sig'))
    assert not out.exists()
    assert build['SourceXrt'] and not build['RuntimeOnly']
    assert build['EnableExeCompiler'] == 1
    records = build['Inputs'] + build['Objects'] + build['Headers']
    assert all(sha(row['Path']) == row['Sha256'].lower() for row in records)
    driver = Path(build['Driver'])
    assert sha(driver) == build['DriverSha256'].lower()
    cc = Path(shutil.which('gcc') or 'E:/software/w64devkit/bin/gcc.exe').resolve()
    fixture = root / 'test/support/compiler_stage_failure.c'
    inputs = {str(path): sha(path) for path in
              (Path(__file__).resolve(), fixture, manifest, driver, cc)}
    objects = [row['Path'] for row in build['Objects'] if Path(row['Source']).name
               not in ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    assert len(objects) == len(build['Objects']) - 1
    out.mkdir(parents=True)
    rows, products = [], []
    report = dict(Complete=False, InputsUnchanged=False, LogsUnchanged=False,
                  Inputs=inputs, Rows=rows, Products=products, SourceBuild=str(manifest),
                  HistoricalRootCauseResolved=False, NetworkTests=False,
                  SourceProductAcceptance=True, ShippingLibraryAcceptance=False, Scope=__doc__)

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')

    def product(path):
        products.append(dict(Path=str(path), Sha256=sha(path)))
        save()

    def run(name, command):
        command = list(map(str, command))
        log = out / (name + '.log')
        started = time.monotonic()
        with log.open('wb') as stream:
            result = subprocess.run(command, cwd=root, stdout=stream,
                                    stderr=subprocess.STDOUT, timeout=180)
        rows.append(dict(Name=name, Command=command, Exit=result.returncode,
                         Log=str(log), Sha256=sha(log), Seconds=time.monotonic()-started))
        save()
        print(name + ': ' + str(result.returncode), flush=True)
        assert result.returncode == 0, (name, str(log))
        return log.read_text(encoding='utf8', errors='replace')

    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I'+str(root),
             '-I'+str(root/'release'), '-I'+str(root/'release/include')]
    if os.name != 'nt':
        assert args.native_root
        flags += ['-D_GNU_SOURCE']
    save()
    try:
        for opt in ('O0', 'O2'):
            obj = out / ('stage.'+opt+'.o')
            run(opt+'.compile', [cc, *flags, '-'+opt, '-c', fixture, '-o', obj])
            product(obj)
            binary = out / ('stage.'+opt+('.exe' if os.name == 'nt' else ''))
            link = [*objects, obj, *['-Wl,--wrap='+name for name in
                    ('xl6IrLowerModule', 'xl6IrVerifyModule',
                     'xl6IrCgenVerifyEmittable', 'xl6IrCgenEmitCEx')]]
            if os.name == 'nt':
                link += [root/'release/xl.dll', '-lws2_32', '-liphlpapi', '-lshell32',
                         '-lole32', '-ladvapi32']
                if not (out/'xl.dll').exists():
                    shutil.copy2(root/'release/xl.dll', out/'xl.dll')
                    product(out/'xl.dll')
            else:
                lib = args.native_root.resolve() / 'release'
                link += ['-L'+str(lib), '-lxl', '-Wl,-rpath,'+str(lib),
                         '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
            link += ['-o', binary]
            response = out / (opt+'.rsp')
            response.write_text('\n'.join('"'+str(item).replace('\\', '/')+'"'
                                         for item in link)+'\n', encoding='utf8')
            product(response)
            run(opt+'.link', [cc, '@'+str(response)])
            product(binary)
            proof = run(opt+'.run', [binary, root, out])
            assert 'Compiler stage failures: 10 exact file/module diagnostics' in proof
        report['InputsUnchanged'] = all(sha(path) == value for path, value in inputs.items()) and \
            all(sha(row['Path']) == row['Sha256'].lower() for row in records)
        report['LogsUnchanged'] = all(sha(row['Log']) == row['Sha256'] for row in rows)
        assert report['InputsUnchanged'] and report['LogsUnchanged']
        assert all(sha(row['Path']) == row['Sha256'] for row in products)
        report['Complete'] = True
    except BaseException as error:
        report['Error'] = repr(error)
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
