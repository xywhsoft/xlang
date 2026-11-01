"""Compare a verified direct C forwarding body with handwritten C on TCC/GCC."""
from pathlib import Path
import argparse, ctypes as c, json, os, re, subprocess, hashlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('root', 'driver', 'out'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--source-build', type=Path)
    args = parser.parse_args()
    root, driver, out = args.root.resolve(), args.driver.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    env = dict(os.environ, XL6_SOURCE_NATIVE_ROOT=str(root))
    rows = []
    gcc_ops = []

    def run(name, command):
        process = subprocess.run(list(map(str, command)), cwd=root, env=env, capture_output=True, timeout=180)
        (out/(name+'.log')).write_bytes(process.stdout+process.stderr)
        assert process.returncode == 0, name
        return process.stdout.decode(errors='replace')

    generated = out/'generated.c'
    run('generate', [driver, root/'test/cases/foreign_decl_handle.xl', 'cgen-default', generated])
    text = generated.read_text(encoding='utf-8-sig')
    match = re.search(r'static int (xlir_fn_\d+)\(foreign_decl_widget (s\d+)\)\n\{\n    \(void\)\2;\n    return foreign_decl_child_count\(\2\);\n\}', text)
    assert match, 'Exact forwarding C placement not selected'
    handwritten = out/'handwritten.c'
    handwritten.write_text('#include "test/cases/foreign_decl_handle.h"\n'
        'int handwritten_count(foreign_decl_widget value) { return foreign_decl_child_count(value); }\n')
    includes = [root, root/'release/include', driver.parent/'include']
    if os.name == 'nt':
        library = c.CDLL(str(root/'release/xlc.dll'))
    else:
        library = c.CDLL(str(root/'release/libxlc.so'))
    signatures = {'tcc_new': ([], c.c_void_p), 'tcc_delete': ([c.c_void_p], None),
        'tcc_set_lib_path': ([c.c_void_p, c.c_char_p], None),
        'tcc_add_include_path': ([c.c_void_p, c.c_char_p], c.c_int),
        'tcc_set_output_type': ([c.c_void_p, c.c_int], c.c_int),
        'tcc_set_options': ([c.c_void_p, c.c_char_p], c.c_int),
        'tcc_add_file': ([c.c_void_p, c.c_char_p], c.c_int),
        'tcc_output_file': ([c.c_void_p, c.c_char_p], c.c_int)}
    for name, (arguments, result) in signatures.items():
        getattr(library, name).argtypes, getattr(library, name).restype = arguments, result
    for name, source, symbol in [('generated', generated, match[1]), ('handwritten', handwritten, 'handwritten_count')]:
        obj = out/(name+'-tcc.o'); state = library.tcc_new(); assert state
        try:
            path = root/'res/tcc/win64/x86_64' if os.name == 'nt' else root/'build/linux/tcc-dist'
            library.tcc_set_lib_path(state, str(path).encode())
            for directory in includes:
                assert library.tcc_add_include_path(state, str(directory).encode()) == 0
            assert library.tcc_set_output_type(state, 3) == 0  # TCC_OUTPUT_OBJ, lib/tcc/libtcc.h
            assert library.tcc_set_options(state, b'-std=c11') == 0
            assert library.tcc_add_file(state, str(source).encode()) == 0
            assert library.tcc_output_file(state, str(obj).encode()) == 0
        finally:
            library.tcc_delete(state)
        disassembly = run(name+'-tcc-disassembly', ['objdump', '-d', '--disassemble='+symbol, obj])
        instructions = []
        for line in disassembly.splitlines():
            parsed = re.match(r'\s*[0-9a-f]+:\s+((?:[0-9a-f]{2}\s+)+)\s*(.*)', line)
            if not parsed: continue
            instructions.append(dict(bytes=parsed[1].split(), operation=parsed[2]))
            if parsed[2].startswith('ret'): break
        assert instructions and instructions[-1]['operation'].startswith('ret')
        rows.append(dict(name=name, compiler='TCC', instructions=instructions,
                         bytes=sum(len(row['bytes']) for row in instructions)))
        assembly = out/(name+'-gcc.s')
        run(name+'-gcc', ['gcc', '-std=c11', '-O2', '-fkeep-static-functions', '-S', *['-I'+str(d) for d in includes], source, '-o', assembly])
        body = assembly.read_text().split(symbol+':', 1)[1].split('.seh_endproc' if os.name == 'nt' else '.size', 1)[0]
        ops = [line.strip() for line in body.splitlines() if line.startswith('\t') and line.strip() and not line.strip().startswith('.')]
        expected = 'jmp\tforeign_decl_child_count'+('' if os.name == 'nt' else '@PLT')
        assert ops in ([expected], ['endbr64', expected]), ops
        gcc_ops.append(ops)
    assert rows[0]['bytes'] == rows[1]['bytes']
    assert [r['bytes'] for r in rows[0]['instructions']] == [r['bytes'] for r in rows[1]['instructions']]
    assert gcc_ops[0] == gcc_ops[1]
    report = dict(scope='Exact trivial getter only; excludes managed/error/cleanup/callback adaptation paths',
        generatedSha256=hashlib.sha256(generated.read_bytes()).hexdigest(),
        tcc=rows, gccO2=gcc_ops, identicalTccInstructionBytes=True)
    (out/'comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    print('TCC exact instruction bytes:', rows[0]['bytes'], 'GCC O2: one tail jump each')
    if args.source_build:
        build = json.loads(args.source_build.read_text(encoding='utf-8-sig'))
        assert build['Complete'] and build['InputsUnchanged'] and Path(build['Driver']).resolve() == driver
        for row in build['Objects']:
            assert hashlib.sha256(Path(row['Path']).read_bytes()).hexdigest() == row['Sha256'].lower()
        objects = [r['Path'] for r in build['Objects'] if Path(r['Source']).name not in
                   ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
        runtime = root/('release/xl.dll' if os.name == 'nt' else 'release/libxl.so')
        if os.name == 'nt':
            import shutil
            shutil.copy2(runtime, out/runtime.name)
        systems = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else \
            ['-Wl,--export-dynamic', '-pthread', '-ldl', '-lm', '-Wl,-rpath,'+str(runtime.parent)]
        for opt in ('O0', 'O2'):
            executable = out/('IR-'+opt+('.exe' if os.name == 'nt' else '.out'))
            command = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-'+opt,
                '-I'+str(root/'release'), *['-I'+str(p) for p in includes],
                root/'test/cases/foreign_decl_forwarding_ir_contract.c', *objects, runtime, *systems, '-o', executable]
            rsp = out/('IR-'+opt+'.rsp')
            rsp.write_text('\n'.join('"'+str(v).replace('\\', '/')+'"' for v in command)+'\n')
            run('IR-'+opt+'-build', ['gcc', '@'+str(rsp)])
            print(run('IR-'+opt+'-run', [executable, root/'test/cases/foreign_decl_handle.xl']).strip())


if __name__ == '__main__':
    main()
