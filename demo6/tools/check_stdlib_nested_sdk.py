"""Default/nested Context SDK regression using a pinned full-source compiler.
Physical SDK beside each executable; isolated cwd; error/reuse; strict C,
JIT/module. Compiler-import AOT still requires generated TCC resources and
shipping-artifact acceptance; it is not waived or counted as passed here.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess
from check_stdlib_slot_map import stage_private_sdk
from xlir_public_build_inputs import verify_public_build

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'public-directory', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    root, public, out = [getattr(args, n).resolve() for n in ('root', 'public_directory', 'out')]
    cc = shutil.which('gcc'); assert cc and not out.exists()
    build, pinned, _ = verify_public_build(root, public)
    driver = Path(build['Driver'])
    runtime = root/'release/xl.dll' if os.name == 'nt' else Path(build['NativeRoot'])/'release/libxl.so'
    manifest, sdk, _ = stage_private_sdk(root)
    cases = [root/'test/cases'/n for n in ('stdlib_nested_sdk.xl', 'stdlib_string_bridge_boundaries.xl')]
    host_source = root/'test/cases/stdlib_nested_sdk_host.c'
    pinned.update((Path(__file__).resolve(), Path(cc), runtime, manifest, host_source, *cases))
    pinned.update(Path(r[k]) for r in sdk for k in ('Source', 'Path'))
    inputs = [dict(Path=str(p), Sha256=sha(p)) for p in sorted(pinned)]
    out.mkdir(); isolated = out/'empty-work'; isolated.mkdir()
    shutil.copytree(public/'include', out/'include')
    if os.name == 'nt': shutil.copy2(runtime, out/'xl.dll')
    env = dict(os.environ, XL6_NESTED_SDK_WORK=str(isolated), XL6_TCC_DLL=str(runtime))
    if os.name == 'nt':
        tcc_root = root/'res/tcc/win64/x86_64'
        env.update(XL6_TCC_ROOT=str(tcc_root), XL6_TCC_INCLUDE=str(root/'lib/tcc/include'),
            XL6_TCC_WIN_INCLUDE=str(tcc_root/'include'),
            XL6_TCC_WINAPI_INCLUDE=str(tcc_root/'include/winapi'),
            XL6_TCC_LIB=str(tcc_root/'lib'), XL6_TCC_LIBTCC1=str(tcc_root/'lib/libtcc1.a'))
    else:
        tcc_root = Path(build['NativeRoot'])/'build/linux/tcc-dist'
        env.update(XL6_SOURCE_NATIVE_ROOT=build['NativeRoot'], XL6_TCC_ROOT=str(tcc_root),
            XL6_TCC_INCLUDE=str(tcc_root/'include'), XL6_TCC_LIB=str(tcc_root),
            XL6_TCC_LIBTCC1=str(tcc_root/'libtcc1.a'))
    rows = []; products = []
    report = dict(Complete=False, InputsUnchanged=False, Inputs=inputs, Rows=rows, Products=products,
        Scope=__doc__, ShippingArtifact=False, Full53Acceptance=False,
        UnexecutedRequirements=['compiler-import AOT with generated TCC resource package'])
    def product(p): products.append(dict(Path=str(p), Sha256=sha(p)))
    def run(name, command, rejection=False):
        result = subprocess.run(list(map(str, command)), cwd=root, env=env, capture_output=True, timeout=300)
        data = result.stdout+result.stderr; log = out/(name+'.log'); log.write_bytes(data)
        rows.append(dict(Name=name, Command=list(map(str, command)), Exit=result.returncode,
            ExpectedRejection=rejection, Log=str(log), LogSha256=sha(log)))
        assert result.returncode != 0 if rejection else result.returncode == 0, (name, data[-6000:])
        return data
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-I.',
        '-Irelease', '-Irelease/include', '-Ilib/tcc', '-I'+str(public/'include')]
    if os.name != 'nt': flags += ['-D_GNU_SOURCE']
    unused = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-const-variable',
        '-Wno-unused-parameter', '-Wno-unused-label', '-Wno-unused-but-set-variable',
        '-Wno-missing-field-initializers', '-Wno-pointer-sign']
    systems = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else ['-pthread', '-ldl', '-lm']
    links = [runtime, *systems] + ([] if os.name == 'nt' else ['-Wl,-rpath,'+str(runtime.parent)])
    objects = [r['Path'] for r in build['Objects'] if Path(r['Path']).stem not in
        ('xlir_public_source_driver', 'xlir_public_source_linux_driver')]
    try:
        host = out/('default-host.exe' if os.name == 'nt' else 'default-host')
        run('host-build', [cc, *flags, '-O2', host_source, *objects, *links, '-o', host]); product(host)
        run('host-isolated', [host, isolated])
        absent = out/'no-sdk'; absent.mkdir(); missing_host = absent/host.name
        shutil.copy2(host, missing_host); product(missing_host)
        if os.name == 'nt': shutil.copy2(runtime, absent/'xl.dll')
        data = run('host-missing-sdk', [missing_host, isolated], True)
        assert b'__xl_runtime/xruntime_decl.h' in data and b'not found' in data
        for case in cases:
            generated = out/(case.stem+'.c')
            run(case.stem+'-generate', [driver, case, 'cgen-default', generated]); product(generated)
            for opt in ('O0', 'O2'):
                obj = out/(case.stem+'-'+opt+'.o')
                run(case.stem+'-'+opt, [cc, *flags, *unused, '-'+opt, '-c', generated, '-o', obj]); product(obj)
            for mode in ('run', 'module'):
                run(case.stem+'-'+mode, [driver, case, mode, out/(case.stem+'-'+mode)])
        report['InputsUnchanged'] = all(sha(r['Path']) == r['Sha256'] for r in inputs)
        assert report['InputsUnchanged']; report['Complete'] = True
    finally:
        for path in out.rglob('*'):
            if path.is_file() and path.parts[len(out.parts)] in ('include', 'no-sdk'):
                if str(path) not in {r['Path'] for r in products}: product(path)
        report['PhaseCount'] = len(rows)
        (out/'results.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print('Default/nested Context SDK regression: PASS', flush=True)

if __name__ == '__main__': main()
