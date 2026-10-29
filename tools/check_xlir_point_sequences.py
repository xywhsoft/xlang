"""Check raw-pointer collection recipes on a verified complete source compiler.
Uses real addresses, JIT/AOT, rejected IR mutations and finite allocation-failure
prefixes. Does not own pointed-to storage or certify whole-platform shipping.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, time
from xlir_public_build_inputs import verify_public_build


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'public', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    root, public, out = (getattr(args, name).resolve() for name in ('root', 'public', 'out'))
    assert not out.exists(), 'Preserve prior pointer collection evidence'
    build, inputs, compared = verify_public_build(root, public)
    windows = os.name == 'nt'
    suffix = '.exe' if windows else ''
    cc = shutil.which('gcc')
    assert cc
    inputs.update((Path(__file__).resolve(), Path(cc).resolve()))
    pins = {str(path): sha(path) for path in inputs}
    out.mkdir(parents=True)
    rows, prefixes = [], {}
    report = dict(Complete=False, Scope=__doc__, Inputs=pins, Rows=rows,
                  AllocationPrefixes=prefixes, ComparedSourceFiles=compared,
                  FullGoalComplete=False, ShippingAccepted=False)
    env = {key: value for key, value in os.environ.items()
           if key.lower() != 'psmodulepath' and not key.upper().startswith('XL6_')}
    if windows:
        provider = root / 'release/xl.dll'
        shutil.copyfile(provider, out / 'xl.dll')
        pins[str(provider)] = sha(provider)
        pins[str(out / 'xl.dll')] = sha(out / 'xl.dll')
        libraries = [str(provider), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
    else:
        native = Path(build['NativeRoot'])
        env['XL6_SOURCE_NATIVE_ROOT'] = str(native)
        libraries = ['-L' + str(native / 'release'), '-lxl',
                     '-Wl,-rpath,' + str(native / 'release'), '-Wl,--export-dynamic', '-pthread', '-ldl', '-lm']
    objects = [row['Path'] for row in build['Objects']
               if Path(row['Source']).name not in ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    assert len(objects) + 1 == len(build['Objects'])
    flags = [*build['CompileFlags'], '-I' + str(public / 'include'),
             '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-parameter',
             '-Wno-unused-label', '-Wno-unused-but-set-variable', '-Wno-missing-field-initializers',
             '-Wno-pointer-sign', '-Wno-format-truncation']

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')

    def run(name, command, expected=(0,)):
        command = list(map(str, command))
        started = time.monotonic()
        with (out / (name + '.log')).open('wb') as log:
            result = subprocess.run(command, cwd=root, env=env, stdout=log,
                                    stderr=subprocess.STDOUT, timeout=180)
        rows.append(dict(Name=name, Command=command, Exit=result.returncode,
                         Expected=list(expected), Seconds=time.monotonic() - started,
                         LogSha256=sha(out / (name + '.log'))))
        save()
        assert result.returncode in expected, (name, (out / (name + '.log')).read_text(errors='replace')[-3000:])
        return result.returncode

    def compile_program(name, arguments):
        response = out / (name + '.rsp')
        response.write_text('\n'.join('"' + str(value).replace('\\', '/').replace('"', '\\"') + '"'
                                      for value in arguments) + '\n', encoding='utf-8')
        pins[str(response)] = sha(response)
        run(name, [cc, '@' + str(response)])

    try:
        for name in ('xlir_point_sequence.xl', 'xlir_point_sequence_host.c',
                     'xlir_point_sequence_native.c', 'xlir_point_sequence_contract.c',
                     'cgen_typed_point.xl', 'sem_typed_point_mismatch.xl'):
            source, target = root / 'test/cases' / name, out / name
            shutil.copyfile(source, target)
            pins[str(source)] = pins[str(target)] = sha(source)
        fixture, driver = out / 'xlir_point_sequence.xl', build['Driver']
        run('emit', [driver, fixture, 'cgen-default', out / 'fixture.c'])
        run('repeat', [driver, fixture, 'cgen-default', out / 'repeat.c'])
        assert sha(out / 'fixture.c') == sha(out / 'repeat.c')
        pins[str(out / 'fixture.c')] = sha(out / 'fixture.c')
        run('original-jit', [driver, out / 'cgen_typed_point.xl', 'run', out / 'original-jit'])
        run('source-jit', [driver, fixture, 'run', out / 'jit'])
        run('source-aot-build', [driver, fixture, 'exe', out / 'aot'])
        run('source-aot', [out / ('aot' + suffix)])
        run('tag-assignment-rejected', [driver, out / 'sem_typed_point_mismatch.xl',
                                      'cgen-default', out / 'bad-tag.c'], (1,))
        assert not (out / 'bad-tag.c').exists()
        assert 'point<AudioTag>' in (out / 'tag-assignment-rejected.log').read_text(errors='replace')
        compile_program('contract-build', [*flags, out / 'xlir_point_sequence_contract.c',
                                           *objects, *libraries, '-o', out / ('contract' + suffix)])
        run('contract', [out / ('contract' + suffix), fixture])
        assert '46 rejected C/header mutations' in (out / 'contract.log').read_text()
        generated = (out / 'fixture.c').read_text(encoding='utf-8')
        probes = re.findall(r'^static xlir_result_i64 (xlir_fn_\d+)\(void\* \w+, void\* \w+, void\* \w+\)', generated, re.M)
        assert probes and len(set(probes)) == 1, probes
        for optimization in ('O0', 'O2'):
            host = out / ('host-' + optimization + suffix)
            native = out / ('native-' + optimization + suffix)
            compile_program('host-build-' + optimization, [*flags, '-' + optimization,
                '-DXL6_POINT_SEQUENCE_GENERATED="' + str(out / 'fixture.c') + '"',
                '-DXL6_POINT_SEQUENCE_PROBE=' + probes[0], out / 'xlir_point_sequence_host.c',
                *objects, *libraries, '-o', host])
            run('host-' + optimization, [host])
            compile_program('native-build-' + optimization, [*flags, '-' + optimization,
                out / 'xlir_point_sequence_native.c', *objects, *libraries, '-o', native])
            run('native-' + optimization, [native])
            for point in range(256):
                if run('fault-' + optimization + '-' + str(point), [native, point], (0, 2)) == 2:
                    assert point > 0
                    prefixes[optimization] = dict(ActualFailures=point, FirstNoHit=point)
                    break
            else:
                raise AssertionError('No finite first no-hit completion')
        report['Complete'] = all(sha(path) == digest for path, digest in pins.items())
        assert report['Complete'], 'Pointer collection inputs changed during verification'
    except Exception as error:
        report['Error'] = repr(error)
        raise
    finally:
        save()
    print('Pointer collection contracts passed: JIT/AOT, real addresses, 46 rejected IR mutations, '
          + str(sum(item['ActualFailures'] for item in prefixes.values())) + ' balanced allocation failures')


if __name__ == '__main__':
    main()
