"""Exercise declared C representations with the actual source compiler.

The C provider is independent of the language declarations. Its authoritative
header makes a wrong receiver, argument or result type a C compilation error.
These tests do not open a window, create a socket or use a security protocol.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--driver', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source', type=Path, help='Explicit fixture; headers/provider remain under --root')
    args = parser.parse_args()
    root, driver, out = args.root.resolve(), args.driver.resolve(), args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    env = {**os.environ, 'XL6_SOURCE_NATIVE_ROOT': str(root)}
    rows = []

    def run(name, command, *, accepted=True, diagnostic=None):
        start = time.monotonic()
        process = subprocess.run(list(map(str, command)), cwd=root, env=env,
                                 capture_output=True, timeout=180)
        log = out / (name + '.log')
        log.write_bytes(process.stdout + process.stderr)
        rows.append(dict(name=name, exitCode=process.returncode, accepted=accepted,
                         seconds=round(time.monotonic()-start, 3), command=list(map(str, command)),
                         log=str(log), sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
        (out / 'stages.json').write_text(json.dumps(rows, indent=2) + '\n', encoding='utf-8')
        assert (process.returncode == 0) == accepted, (name, log)
        if diagnostic:
            assert diagnostic in log.read_text(encoding='utf-8-sig', errors='replace'), (name, log)
        print(name, process.returncode, flush=True)

    source = args.source.resolve() if args.source else root / 'test/cases/foreign_decl_handle.xl'
    generated = out / 'handle.c'
    run('handle-cgen', [driver, source, 'cgen-default', generated])
    text = generated.read_text(encoding='utf-8-sig')
    assert '#include <test/cases/foreign_decl_handle.h>' in text
    assert 'extern int foreign_decl_add_child(foreign_decl_widget, foreign_decl_widget);' in text
    assert 'extern foreign_decl_widget foreign_decl_identity(foreign_decl_widget);' in text
    assert 'xlir_xvalue_object_get_literal_raw_pointer(' not in text
    assert 'xlir_native_handle_take_s' not in text
    runtime = root / 'release' / ('xl.dll' if os.name == 'nt' else 'libxl.so')
    if os.name == 'nt':
        import shutil
        shutil.copy2(runtime, out / runtime.name)
    for optimization in ('O0', 'O2'):
        executable = out / ('handle-' + optimization + ('.exe' if os.name == 'nt' else ''))
        command = ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                   '-Wno-unused-variable', '-Wno-unused-label', '-Wno-unused-const-variable',
                   '-Wno-unused-but-set-variable', '-' + optimization, '-I' + str(root),
                   '-I' + str(root / 'release/include'), '-I' + str(driver.parent / 'include'),
                   generated, root / 'test/cases/foreign_decl_handle_native.c',
                   root / 'test/cases/foreign_decl_module_host.c', runtime, '-o', executable]
        run('handle-' + optimization + '-build', command)
        run('handle-' + optimization + '-run', [executable])
    provider = root / 'test/cases/foreign_decl_handle_native.c'
    for mode in ('run', 'module', 'exe'):
        prefix = out / ('native-' + mode)
        run('handle-tcc-' + mode, [driver, source, mode, prefix, provider])
        if mode == 'exe':
            run('handle-tcc-standalone', [prefix.with_suffix('.exe') if os.name == 'nt' else prefix])

    base = source.read_text(encoding='utf-8-sig')
    negatives = {
        'alias-non-pointer': (base.replace('WidgetAliasChain(widgetAt(0))', 'WidgetAliasChain(17)'),
                              'native-backed class handle binding argument must be point'),
        'missing-header': (base.replace('@header("test/cases/foreign_decl_handle.h") ', ''),
                           'external class requires'),
        'missing-lifetime': (base.replace(' @lifetime("caller")', ''), 'external class requires'),
        'bad-header': (base.replace('test/cases/foreign_decl_handle.h', '../escape.h'),
                       'invalid external class'),
        'bad-c-name': (base.replace('"foreign_decl_widget" class', '"bad;type" class'),
                       'invalid external class'),
        'duplicate-header': (base.replace('@lifetime("caller")',
                            '@lifetime("caller") @header("other.h")'), 'invalid external class'),
        'unknown-lifetime': (base.replace('@lifetime("caller")', '@lifetime("arbitrary")'),
                             'invalid external class'),
        'ordinary-field': (base.replace('    native point handle',
                          '    native point handle\n    int count'),
                           'caller-owned external class admits only'),
        'destructor': (base.replace('    native point handle',
                       '    native point handle\n    extern "unrelated_free" destroy()'),
                        'caller-owned external class admits only'),
        'ordinary-subclass': (base + '\nclass LanguageChild extends Widget\nend\n',
                              'ordinary class cannot inherit an external C view'),
        'virtual-dispatch': (base.replace('extern "foreign_decl_add_child"',
                                         'virtual extern "foreign_decl_add_child"'),
                              'external C view does not add a language virtual dispatch table'),
        'scalar-field': (base.replace('native point handle', 'native int handle'),
                         'external class needs one native point handle field'),
        'private-pointer-binding': (base.replace('native point handle', 'private native point handle'),
                                    'private class member is not accessible'),
    }
    for name, (text, diagnostic) in negatives.items():
        case = out / (name + '.xl'); case.write_text(text, encoding='utf-8')
        target = out / (name + '.c')
        run(name, [driver, case, 'cgen-default', target], accepted=False, diagnostic=diagnostic)
        assert not target.exists(), target
    # Width equality alone must not certify a pointer: uintptr_t is exactly
    # pointer-sized on the supported targets but remains an arithmetic type.
    scalar = out / 'not-a-pointer.xl'
    scalar.write_text('''extern "uintptr_t" class Wrong @header("stdint.h") @lifetime("caller")
    native point handle
end
extern "foreign_wrong_accept" @no_raise func consume(Wrong value)
point raw = null
Wrong value = Wrong(raw)
consume(value)
''', encoding='utf-8')
    rejected = out / 'not-a-pointer.c'
    run('not-a-pointer-cgen', [driver, scalar, 'cgen-default', rejected])
    run('not-a-pointer-c-abi', ['gcc', '-std=c11', '-O0', '-c',
        '-I' + str(root / 'release/include'), '-I' + str(driver.parent / 'include'),
        rejected, '-o', out / 'not-a-pointer.o'], accepted=False,
        diagnostic='external handle must be a C pointer type')
    print('External handle representation, authoritative C ABI, O0/O2 and refusal checks passed.')


if __name__ == '__main__':
    main()
