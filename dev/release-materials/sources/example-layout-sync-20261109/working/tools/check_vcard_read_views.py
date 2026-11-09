"""Check vcard native read views against C, JIT and standalone executables."""
from pathlib import Path
import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time
import uuid
from runtime_layout import runtime_directory, runtime_example


def example_cases(root, runtime):
    package = runtime / 'extlibs/vcard'
    sources = [('literal', root/'test/cases/extlib_vcard.xl'),
               ('boundaries', root/'test/cases/extlib_vcard_boundaries.xl'),
               ('read-cost', root/'test/cases/extlib_vcard_read_views.xl'),
               ('read-views', package/'test/read_views.xl')]
    sources += [(name, package/'test'/(name+'.xl')) for name in (
        'control_characters', 'allocations', 'uri_values', 'structure', 'limits', 'utf8_folding', 'aot_core')]
    sources += [('original', runtime_example(runtime, 'extlibs/vcard.xl')),
                ('folded', package/'examples/folded_contact.xl'),
                ('original-c', package/'examples/vcard.c'),
                ('folded-c', package/'examples/folded_contact_c.c'),
                ('utf8-c', package/'test/test_utf8_folding.c')]
    return sources


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', type=Path, default=runtime_directory(root))
    parser.add_argument('--out', type=Path)
    parser.add_argument('--skip-aot', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--list', action='store_true')
    args = parser.parse_args()
    runtime = args.runtime.resolve()
    package = runtime / 'extlibs/vcard'
    sources = example_cases(root, runtime)
    if args.list:
        assert all(source.is_file() for _, source in sources)
        print(json.dumps([str(source) for _, source in sources])); return
    out = (args.out or root / 'build/test' / ('vcard-' + uuid.uuid4().hex)).resolve()
    out.mkdir(parents=True, exist_ok=False)
    windows = os.name == 'nt'
    cli = runtime / ('xl.exe' if windows else 'xl')
    cc = shutil.which('gcc')
    assert cc and cli.is_file() and package.is_dir()
    env = dict(os.environ, XLANG_PACKAGE_PATH=str(runtime / 'extlibs'))
    if not windows:
        env['LD_LIBRARY_PATH'] = str(runtime) + ':' + env.get('LD_LIBRARY_PATH', '')
    if args.sanitize:
        assert not windows, 'Use Linux GCC for ASan/UBSan'
        env.update(ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',
                   UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
    sha = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
    protected = [p for p in package.rglob('*') if p.is_file()]
    protected += [cli, runtime / ('xl.dll' if windows else 'libxl.so'),
                  runtime / ('xlc.dll' if windows else 'libxlc.so'),
                  runtime / 'include/xl_value_read.h',
                  runtime / 'extlibs/_common/xl_value_read_wire.h', root / 'lib/xrt/xrt.h']
    report = dict(passed=False, runtime=str(runtime), sanitize=args.sanitize,
                  inputs={str(p): sha(p) for p in protected}, rows=[])

    def run(name, command, expected=0, timeout=180):
        command = list(map(str, command))
        start = time.monotonic()
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=timeout)
        log = out / (name + '.log')
        log.write_bytes(result.stdout + result.stderr)
        row = dict(name=name, command=command, exitCode=result.returncode,
                   expected=expected, passed=result.returncode == expected,
                   seconds=round(time.monotonic()-start, 3), logSha256=sha(log))
        print(name, result.returncode, flush=True)
        return row

    def check(name, command, expected=0):
        row = run(name, command, expected)
        report['rows'].append(row)
        assert row['passed'], (name, out / (name + '.log'))

    def executable(name): return out / (name + ('.exe' if windows else ''))
    try:
        check('gcc-version', [cc, '--version'])
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                 '-DXL_VALUE_READ_WIRE_ONLY', '-I' + str(root / 'lib/xrt'),
                 '-I' + str(runtime / 'include'), '-I' + str(package / 'include')]
        links = ['-lWs2_32', '-lAdvapi32', '-lIphlpapi', '-lm'] if windows else ['-lm', '-ldl', '-pthread']
        if args.sanitize:
            flags += ['-O1', '-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-fno-pie', '-no-pie', '-D_GNU_SOURCE', '-pthread']
        units = [('xrt_runtime', package / 'test/xrt_runtime.c')]
        units += [(name, package / (name + '.c')) for name in ('vcard_native', 'vcard_parse', 'vcard_build')]
        jobs = [(name, [cc, *flags, '-c', source, '-o', out / (name + '.o')]) for name, source in units]
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            rows = list(pool.map(lambda job: run('compile-' + job[0], job[1]), jobs))
        report['rows'].extend(rows)
        assert all(row['passed'] for row in rows), out
        objects = [out / (name + '.o') for name, _ in units]
        for name, source in [('c-boundaries', package / 'test/test_vcard.c'),
                             ('c-utf8', package / 'test/test_utf8_folding.c'),
                             ('c-original', package / 'examples/vcard.c'),
                             ('c-folded', package / 'examples/folded_contact_c.c')]:
            binary = executable(name)
            check(name + '-build', [cc, *flags, source, *objects, *links, '-o', binary])
            argv = [out/'long.vcf', out/'four.vcf'] if name == 'c-boundaries' else (
                [out/(name+'.vcf')] if name in ('c-original','c-folded') else [])
            check(name, [binary, *argv])
        check('c-canonical', [sys.executable, package/'test/check_folded.py', out/'c-folded.vcf'])
        check('c-failed-write', [executable('c-original'), out], expected=1)
        assert (out/'c-failed-write.log').read_bytes() == b''
        generated = out/'generated.c'
        check('actual-c-abi', [cli,'cgen',package/'examples/folded_contact.xl','--emit',generated])
        prototypes = re.findall(r'^extern [^\r\n;]*\bxv_(?:parse|build)\([^;\r\n]*\);', generated.read_text(), re.M)
        assert len(prototypes) == 2
        abi = out/'abi.c'
        abi.write_text('#include "vcard_native.h"\n'+'\n'.join(prototypes)+'\n')
        check('native-header-abi', [cc,*flags,'-fsyntax-only',abi])
        wrong = out/'wrong-abi.c'
        wrong.write_text(re.sub(r'extern .* xv_parse\(', 'extern int xv_parse(', abi.read_text()))
        check('native-header-rejects-wrong-abi', [cc,*flags,'-fsyntax-only',wrong], expected=1)

        for name, source in sources:
            assert source.is_file(), source
            report['inputs'][str(source)] = sha(source)
            jit_args = [out/(name+'-jit.vcf')] if name in ('original','original-c','folded','folded-c') else []
            check(name+'-jit', [cli, 'run', source, *(['--', *jit_args] if jit_args else [])])
            if name in ('original','original-c'): assert jit_args[0].read_bytes() == (out/'c-original.vcf').read_bytes()
            if name == 'original-c':
                check('c-jit-failed-write', [cli,'run',source,'--',out], expected=1)
                assert (out/'c-jit-failed-write.log').read_bytes() == b''
            if name in ('folded','folded-c'):
                check(name+'-jit-canonical', [sys.executable, package/'test/check_folded.py', *jit_args])
            if not args.skip_aot:
                binary = executable(name)
                check(name+'-build', [cli, 'build', source, '-o', binary])
                argv = [out/(name+'-aot.vcf')] if jit_args else []
                check(name+'-aot', [binary, *argv])
                if name in ('original','original-c'): assert argv[0].read_bytes() == (out/'c-original.vcf').read_bytes()
                if name in ('folded','folded-c'):
                    check(name+'-aot-canonical', [sys.executable, package/'test/check_folded.py', *argv])
        report['passed'] = True
    finally:
        report['inputsUnchanged'] = all(sha(path) == digest for path, digest in report['inputs'].items())
        report['passed'] = report['passed'] and report['inputsUnchanged']
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    assert report['passed'], out
    print('vcard:', len(report['rows']), 'checks passed;', out)


if __name__ == '__main__':
    main()
