"""Offline exact C record/pointer callbacks and direct caller-resident addresses."""
from pathlib import Path
from runtime_layout import runtime_directory
import argparse, hashlib, json, os, shutil, subprocess, time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('root', 'driver', 'out'):
        parser.add_argument('--'+key, type=Path, required=True)
    parser.add_argument('--source-build', type=Path)
    args = parser.parse_args()
    root, driver, out = args.root.resolve(), args.driver.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    rows = []
    env = dict(os.environ, XL6_SOURCE_NATIVE_ROOT=str(root))

    def run(name, command, accepted=True):
        command = list(map(str, command)); start = time.monotonic()
        process = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=240)
        log = out/(name+'.log'); log.write_bytes(process.stdout+process.stderr)
        rows.append(dict(name=name, command=command, exitCode=process.returncode, accepted=accepted,
                         seconds=round(time.monotonic()-start, 3), log=str(log),
                         sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
        (out/'stages.json').write_text(json.dumps(rows, indent=2)+'\n', encoding='utf-8')
        print(name, process.returncode, flush=True)
        assert (process.returncode == 0) == accepted, log

    runtime = runtime_directory(root)/('xl.dll' if os.name == 'nt' else 'libxl.so')
    if os.name == 'nt': shutil.copy2(runtime, out/runtime.name)
    includes = ['-I'+str(root), '-I'+str(runtime_directory(root)/'include'), '-I'+str(driver.parent/'include')]
    warnings = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-label',
                '-Wno-unused-const-variable', '-Wno-unused-but-set-variable']
    provider = root/'test/cases/foreign_decl_callback_native.c'
    suffix = '.exe' if os.name == 'nt' else '.out'
    for stem in ('callback', 'static_callback'):
        source = root/('test/cases/foreign_decl_'+stem+'.xl')
        generated = out/(stem+'.c')
        run(stem+'-cgen', [driver, source, 'cgen-default', generated])
        text = generated.read_text(encoding='utf-8-sig')
        assert '_Static_assert(_Generic(&foreign_callback_apply_record, foreign_callback_record (*)(' in text
        if stem == 'static_callback':
            assert 'xlir_raw_callback_entry_' not in text and 'xlir_closure_new(' not in text
            assert '= xlir_fn_' in text and '.raw_callback' not in text
        for opt in ('O0', 'O2'):
            exe = out/(stem+'-'+opt+suffix)
            run(stem+'-'+opt+'-build', ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                *warnings, '-'+opt, *includes, generated, provider,
                root/'test/cases/foreign_decl_module_host.c', runtime, '-o', exe])
            run(stem+'-'+opt+'-run', [exe])
        for mode in ('run', 'module', 'exe'):
            prefix = out/(stem+'-'+mode)
            run(stem+'-tcc-'+mode, [driver, source, mode, prefix, provider])
            if mode == 'exe': run(stem+'-tcc-standalone', [prefix.with_suffix('.exe') if os.name == 'nt' else prefix])
    source = root/'test/cases/foreign_decl_static_callback.xl'
    original = source.read_text(encoding='utf-8-sig')
    negatives = {
        'missing-callback-contract': original.replace('c_callback ', ''),
        'scalar-parameter': original.replace('@c_static_callback(0) func applyRecord', '@c_static_callback(1) func applyRecord'),
        'out-of-range': original.replace('@c_static_callback(0)', '@c_static_callback(5)'),
        'duplicate': original.replace('@c_static_callback(0)', '@c_static_callback(0) @c_static_callback(0)'),
        'negative-index': original.replace('@c_static_callback(0)', '@c_static_callback(-1)'),
        'function-variable': original.replace('    Record output =', '    auto saved = transform\n    Record output =').replace('applyRecord(transform, input)', 'applyRecord(saved, input)'),
        'fallible-function': original.replace('    input.number = widget.value', '    assert input.number == 0\n    input.number = widget.value'),
        'wrong-result': original.replace('c_callback func(Widget, Record) as Record', 'c_callback func(Widget, Record) as c_int'),
        'wrong-input': original.replace('c_callback func(Widget, Record)', 'c_callback func(Widget, c_int)'),
        'not-a-static-address': original.replace('applyPointer(identity)', 'applyPointer(func(Widget? widget) as Widget?\n        return widget\n    end)'),
    }
    for name, content in negatives.items():
        case = out/(name+'.xl'); case.write_text(content, encoding='utf-8')
        target = out/(name+'.c')
        run(name, [driver, case, 'cgen-default', target], False)
        assert not target.exists(), target
    if not args.source_build:
        return  # Exploratory functional run; no independent IR acceptance claim.
    build = json.loads(args.source_build.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and Path(build['Driver']).resolve() == driver
    for row in build['Objects']:
        assert hashlib.sha256(Path(row['Path']).read_bytes()).hexdigest() == row['Sha256'].lower()
    objects = [r['Path'] for r in build['Objects'] if Path(r['Source']).name not in
               ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    systems = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else \
        ['-Wl,--export-dynamic', '-pthread', '-ldl', '-lm', '-Wl,-rpath,'+str(runtime.parent)]
    for opt in ('O0', 'O2'):
        exe = out/('IR-'+opt+suffix)
        arguments = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-'+opt,
                     '-I'+str(runtime_directory(root)), *includes, root/'test/cases/foreign_decl_callback_ir_contract.c',
                     *objects, runtime, *systems, '-o', exe]
        rsp = out/('IR-'+opt+'.rsp')
        rsp.write_text('\n'.join('"'+str(v).replace('\\', '/')+'"' for v in arguments)+'\n', encoding='utf-8')
        run('IR-'+opt+'-build', ['gcc', '@'+str(rsp)])
        run('IR-'+opt+'-run', [exe, source])


if __name__ == '__main__':
    main()
