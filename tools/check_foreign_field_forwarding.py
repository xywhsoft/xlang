"""Compare one trivial native field getter with handwritten C on the same TCC/GCC."""
from pathlib import Path
from runtime_layout import runtime_directory
from xlir_public_build_inputs import verify_public_build
import argparse, ctypes as c, json, os, re, subprocess, hashlib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('root', 'driver', 'out'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--source-build', type=Path,required=True)
    args = parser.parse_args()
    root, driver, out = args.root.resolve(), args.driver.resolve(), args.out.resolve()
    build,pinned,_=verify_public_build(root,args.source_build.resolve().parent)
    assert Path(build['Driver']).resolve()==driver
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
    run('generate', [driver, root/'test/cases/foreign_native_fields.xl', 'cgen-default', generated])
    text = generated.read_text(encoding='utf-8-sig')
    match = re.search(r'static int (xlir_fn_\d+)\(ManagedChild\* (s\d+)\)\n\{\n    \(void\)\2;\n    return \2->value;\n\}', text)
    assert match, 'Exact forwarding C placement not selected'
    handwritten = out/'handwritten.c'
    handwritten.write_text('#include "test/cases/foreign_managed_view.h"\n'
        'int handwritten_count(ManagedChild* value) { return value->value; }\n')
    includes = [root, runtime_directory(root)/'include', driver.parent/'include']
    if os.name == 'nt':
        library = c.CDLL(json.loads(args.source_build.read_text(encoding='utf-8-sig'))['TccCore'])
    else:
        library = c.CDLL(str(runtime_directory(root)/'libxlc.so'))
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
        assert len([op for op in ops if op!='endbr64'])==2 and ops[-1].startswith('ret'),ops
        gcc_ops.append(ops)
    assert rows[0]['bytes'] == rows[1]['bytes']
    assert [r['bytes'] for r in rows[0]['instructions']] == [r['bytes'] for r in rows[1]['instructions']]
    assert gcc_ops[0] == gcc_ops[1]
    report = dict(scope='Exact trivial getter only; excludes managed/error/cleanup/callback adaptation paths',
        sourceBuild=str(args.source_build.resolve()),tccProviderSha256=hashlib.sha256(Path(build['TccCore']).read_bytes()).hexdigest(),
        toolSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        generatedSha256=hashlib.sha256(generated.read_bytes()).hexdigest(),
        tcc=rows, gccO2=gcc_ops, identicalTccInstructionBytes=True)
    (out/'comparison.json').write_text(json.dumps(report, indent=2)+'\n')
    print('TCC exact instruction bytes:', rows[0]['bytes'], 'GCC O2: one field load and return each')


if __name__ == '__main__':
    main()
