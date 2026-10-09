"""Saved raw callback red/green test against two full source-built compilers.

Only the actual public API/TCC host is linked; no private IR layouts cross the
two compiler versions. The unchanged observer must detect a missing generated
Enter/Leave in the baseline and accept the fixed compiler. TCC is a released
tool input, not a rebuilt shipping product. No sanitizer/full53 claim.
"""
import argparse, hashlib, json, os, shutil, subprocess, sys
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('before-root', 'before-public', 'after-root', 'after-public', 'host-source',
                 'vectors-source', 'original-source', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args(); out = args.out.resolve(); assert not out.exists()
    host, vectors, original = (args.host_source.resolve(), args.vectors_source.resolve(), args.original_source.resolve())
    source = args.after_root.resolve()/'test/cases/xlir_raw_activity.xl'
    cc = shutil.which('gcc'); assert cc
    files = {Path(__file__).resolve(), Path(sys.executable), Path(cc), host, vectors, original, source}
    lanes = []
    for name, root, public in (('before', args.before_root, args.before_public), ('after', args.after_root, args.after_public)):
        root, public = root.resolve(), public.resolve(); manifest = public/'build.json'
        build = json.loads(manifest.read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly'] and build['EnableExeCompiler'] == 1
        if os.name != 'nt': assert build['Complete'] and build['InputsUnchanged']
        files.add(manifest)
        for row in build['Inputs']+build['Objects']+build['Headers']:
            assert sha(row['Path']).lower() == row['Sha256'].lower(), row['Path']; files.add(Path(row['Path']))
        runtime = root/'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot'])/'release/libxl.so'
        files.update((runtime, root/'test/support/module_product_memory.c', root/'test/support/module_product_memory.h'))
        files.add(root/'test/cases/xlir_foreign_raw_callback_error.xl')
        lanes.append((name, root, public, build, runtime))
    inputs = [{'Path': str(path), 'Sha256': sha(path)} for path in sorted(files)]
    rows, products, errors = [], [], []
    report = {'Complete': False, 'InputsUnchanged': False, 'Inputs': inputs, 'Products': products,
              'Rows': rows, 'Errors': errors, 'Scope': __doc__, 'ExpectedBaselineAssertion': True,
              'ShippingLibraryAcceptance': False, 'Full53Acceptance': False, 'Sanitized': False}
    out.mkdir(parents=True)
    def run(name, command, root, env, expected_assertion=False):
        command = list(map(str, command))
        if os.name == 'nt': result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=180)
        else:
            import resource
            def no_core(): resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=180, preexec_fn=no_core)
        data = result.stdout+result.stderr; log = out/(name+'.log'); log.write_bytes(data)
        products.append({'Path': str(log), 'Sha256': sha(log)})
        rows.append({'Name': name, 'Command': command, 'Exit': result.returncode, 'ExpectedAssertion': expected_assertion})
        print(name+': '+str(result.returncode), flush=True)
        if expected_assertion: assert result.returncode != 0 and b'xrtAtomic64Load(&enters' in data, data[-6000:]
        else: assert result.returncode == 0, data[-6000:]
        return data
    try:
        for name, root, public, build, runtime in lanes:
            lane = out/name; lane.mkdir(); env = os.environ.copy()
            flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-O0', '-DXL_STATIC_EMBED',
                '-I'+str(root), '-I'+str(root/'release'), '-I'+str(root/'release/include'),
                '-I'+str(root/'lib/tcc'), '-I'+str(public/'include'), '-I'+str(vectors.parent)]
            links = [row['Path'] for row in build['Objects'] if Path(row['Path']).name not in
                ('xlir_public_source_driver.o', 'xlir_public_source_linux_driver.o')]
            if os.name == 'nt':
                shutil.copy2(runtime, lane/'xl.dll'); links += [str(runtime), '-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32']
            else:
                flags += ['-D_GNU_SOURCE']; env['XL6_SOURCE_NATIVE_ROOT'] = build['NativeRoot']
                links += [str(runtime), '-Wl,-rpath,'+str(runtime.parent), '-pthread', '-ldl', '-lm']
            binary = lane/('tcc'+('.exe' if os.name == 'nt' else ''))
            define = '-DXL6_RAW_ACTIVITY_SOURCE="'+source.as_posix()+'"'
            run(name+'.compile', [cc, *flags, define, '-Wl,--wrap=xl6CodeActivityEnterV1',
                '-Wl,--wrap=xl6CodeActivityLeaveV1', host, original, root/'test/support/module_product_memory.c',
                *links, '-o', binary], root, env)
            products.append({'Path': str(binary), 'Sha256': sha(binary)})
            tcc_lib = root/'res/tcc/win64/x86_64' if os.name == 'nt' else Path(build['NativeRoot'])/'build/linux/tcc-dist'
            data = run(name+'.run', [binary, runtime, tcc_lib, lane/'temps', public/'include'], root, env, name == 'before')
            if name == 'after': assert b'exact tool-only heap baseline' in data
        report['InputsUnchanged'] = all(sha(row['Path']) == row['Sha256'] for row in inputs)
        report['Complete'] = report['InputsUnchanged']
    except Exception as error: errors.append(repr(error)); print(repr(error), file=sys.stderr)
    finally: (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return 0 if report['Complete'] else 1

if __name__ == '__main__': raise SystemExit(main())
