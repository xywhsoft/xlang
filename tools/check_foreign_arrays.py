"""Offline native inline-array layout, value semantics and rejection checks."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, time


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'driver', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--source-build', type=Path)
    a = p.parse_args()
    root, driver, out = a.root.resolve(), a.driver.resolve(), a.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    source = root / 'test/cases/foreign_decl_array.xl'
    provider = root / 'test/cases/foreign_decl_array_native.c'
    env = dict(os.environ, XL6_SOURCE_NATIVE_ROOT=str(root))
    rows = []

    def run(name, command, accepted=True, diagnostic=None):
        start = time.monotonic()
        result = subprocess.run(list(map(str, command)), cwd=root, env=env,
                                capture_output=True, timeout=240)
        log = out / (name + '.log')
        log.write_bytes(result.stdout + result.stderr)
        rows.append(dict(name=name, exitCode=result.returncode, accepted=accepted,
                         command=list(map(str, command)), log=str(log),
                         seconds=round(time.monotonic() - start, 3),
                         sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
        (out / 'stages.json').write_text(json.dumps(rows, indent=2)+'\n', encoding='utf-8')
        assert (result.returncode == 0) == accepted, (name, log)
        if diagnostic:
            assert diagnostic in log.read_text(encoding='utf-8-sig', errors='replace'), log
        print(name, result.returncode, flush=True)

    generated = out / 'array.c'
    run('cgen', [driver, source, 'cgen-default', generated])
    text = generated.read_text(encoding='utf-8-sig')
    assert 'Elements[16]' in text and 'Elements[3]' in text
    assert 'C array extent' in text
    assert 'xlir_xvalue_object_get_literal' not in text
    runtime = root / ('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
    if os.name == 'nt':
        shutil.copy2(runtime, out / runtime.name)
    includes = ['-I'+str(root), '-I'+str(root/'release/include'), '-I'+str(driver.parent/'include')]
    for opt in ('O0', 'O2'):
        exe = out / (opt + ('.exe' if os.name == 'nt' else '.out'))
        run(opt+'-build', ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-label',
            '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-'+opt,
            *includes, generated, provider, root/'test/cases/foreign_decl_module_host.c',
            runtime, '-o', exe])
        run(opt+'-run', [exe])
    for mode in ('run', 'module', 'exe'):
        target = out / ('native-'+mode)
        run('tcc-'+mode, [driver, source, mode, target, provider])
        if mode == 'exe':
            run('tcc-standalone', [target.with_suffix('.exe') if os.name == 'nt' else target])
    original = source.read_text(encoding='utf-8-sig')
    declarations = original.split('async func matrixAfter', 1)[0]
    invalid = {
        'zero-extent': declarations.replace('m[16]', 'm[0]'),
        'negative-extent': declarations.replace('m[16]', 'm[-1]'),
        'fractional-extent': declarations.replace('m[16]', 'm[1.5]'),
        'overflow-extent': declarations.replace('m[16]', 'm[4294967296]'),
        'managed-element': declarations.replace('c_float m[16]', 'string m[16]'),
        'ordinary-class': 'class Bad\n c_int a[3]\nend\n',
        'const-array': declarations+'const Matrix value = data()\nvalue.m[0] = c_float(4.0)\n',
        'temporary-array': declarations+'data().m[0] = c_float(4.0)\n',
        'readonly-length': declarations+'Matrix value=data()\nvalue.m.length=3\n',
        'bad-index': declarations+'Matrix value=data()\nauto item=value.m[1.5]\n',
        'wrong-element': declarations+'Matrix value=data()\nvalue.m[0]="text"\n',
        'wrong-array-type': declarations+'Matrix value=data()\nVertices other=Vertices()\nvalue.m=other.transforms\n',
        'unsupported-safe-index': declarations+'Matrix value=data()\nauto item=value.m?[0]\n',
    }
    for name, content in invalid.items():
        case, target = out/(name+'.xl'), out/(name+'.c')
        case.write_text(content, encoding='utf-8')
        run(name, [driver, case, 'cgen-default', target], False)
        assert not target.exists(), target
    for name, fields in {'wrong-extent': 'c_float m[15]', 'wrong-element-ABI': 'c_int m[16]'}.items():
        content = declarations.replace('c_float m[16]', fields)+'Matrix value=Matrix()\n'
        case, target = out/(name+'.xl'), out/(name+'.c')
        case.write_text(content, encoding='utf-8')
        run(name+'-cgen', [driver, case, 'cgen-default', target])
        run(name+'-C-ABI', ['gcc', '-std=c11', *includes, '-c', target, '-o', out/(name+'.o')],
            False, 'C array')
    if a.source_build:
        build = json.loads(a.source_build.read_text(encoding='utf-8-sig'))
        assert build['Complete'] and build['InputsUnchanged']
        assert Path(build['Driver']).resolve() == driver
        objects = [row['Path'] for row in build['Objects'] if Path(row['Source']).name not in
                   ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
        for row in build['Objects']:
            assert hashlib.sha256(Path(row['Path']).read_bytes()).hexdigest() == row['Sha256'].lower()
        systems = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else \
            ['-Wl,--export-dynamic', '-pthread', '-ldl', '-lm', '-Wl,-rpath,'+str(runtime.parent)]
        contract = root / 'test/cases/foreign_decl_array_ir_contract.c'
        for opt in ('O0', 'O2'):
            exe = out / ('ir-contract-'+opt+('.exe' if os.name == 'nt' else '.out'))
            arguments = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
                         '-I'+str(root/'release'), *includes, '-'+opt, str(contract),
                         *objects, str(runtime), *systems, '-o', str(exe)]
            rsp = out / ('ir-contract-'+opt+'.rsp')
            rsp.write_text('\n'.join('"'+str(s).replace('\\', '/')+'"' for s in arguments)+'\n', encoding='utf-8')
            run('ir-contract-'+opt+'-build', ['gcc', '@'+str(rsp)])
            run('ir-contract-'+opt+'-run', [exe, source])


if __name__ == '__main__':
    main()
