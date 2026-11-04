"""Check authoritative C record layouts and execution without a UI backend."""
from pathlib import Path
from compiler_response import command_with_response
from xlir_public_build_inputs import verify_public_build
from runtime_layout import runtime_directory
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--driver', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    root, driver, out = args.root.resolve(), args.driver.resolve(), args.out.resolve()
    source_build, source_inputs, _ = verify_public_build(root, driver.parent)
    assert Path(source_build['Driver']).resolve() == driver
    source_objects = [x['Path'] for x in source_build['Objects'] if Path(x['Source']).name not in
                      ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    source_systems = ['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm']
    out.mkdir(parents=True, exist_ok=False)
    env = {**os.environ, 'XL6_SOURCE_NATIVE_ROOT': str(root)}
    rows = []

    def run(name, command, *, accepted=True, diagnostic=None):
        if str(command[0]) == 'gcc' and not any(str(x).startswith('@') for x in command[1:]):
            command = command_with_response(command, 'gcc', out/(name+'.rsp'))
        start = time.monotonic()
        result = subprocess.run(list(map(str, command)), cwd=root, env=env,
                                capture_output=True, timeout=180)
        log = out / (name + '.log')
        log.write_bytes(result.stdout + result.stderr)
        rows.append(dict(name=name, exitCode=result.returncode, accepted=accepted,
                         seconds=round(time.monotonic()-start, 3), command=list(map(str, command)),
                         log=str(log), sha256=hashlib.sha256(log.read_bytes()).hexdigest()))
        (out / 'stages.json').write_text(json.dumps(rows, indent=2)+'\n', encoding='utf-8')
        assert (result.returncode == 0) == accepted, (name, log)
        if diagnostic:
            assert diagnostic in log.read_text(encoding='utf-8-sig', errors='replace'), (name, log)
        print(name, result.returncode, flush=True)

    source = root / 'test/cases/foreign_decl_record.xl'
    provider = root / 'test/cases/foreign_decl_record_native.c'
    generated = out / 'record.c'
    run('record-cgen', [driver, source, 'cgen-default', generated])
    text = generated.read_text(encoding='utf-8-sig')
    assert 'XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(&foreign_decl_shift, foreign_decl_rect (*)(foreign_decl_rect, int, int)), "declared C function ABI");' in text
    assert 'XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(&foreign_decl_scale, foreign_decl_transform (*)(foreign_decl_transform, double)), "declared C function ABI");' in text
    assert 'C record field offset' in text and 'C record field type' in text
    assert 'xlir_xvalue_object_get_literal' not in text
    runtime = runtime_directory(root) / ('xl.dll' if os.name == 'nt' else 'libxl.so')
    if os.name == 'nt':
        shutil.copy2(runtime, out / runtime.name)
    includes = ['-I'+str(root), '-I'+str(runtime_directory(root)/'include'), '-I'+str(driver.parent/'include')]
    for optimization in ('O0', 'O2'):
        executable = out / ('record-'+optimization+('.exe' if os.name == 'nt' else ''))
        run(optimization+'-build', ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror',
            '-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-label',
            '-Wno-unused-const-variable', '-Wno-unused-but-set-variable', '-DXL_STATIC_EMBED', '-'+optimization,
            *includes, generated, provider, root/'test/cases/foreign_decl_module_host.c',
            *source_objects, runtime, *source_systems, '-o', executable])
        run(optimization+'-run', [executable])
    for mode in ('run', 'module', 'exe'):
        prefix = out / ('native-'+mode)
        run('tcc-'+mode, [driver, source, mode, prefix, provider])
        if mode == 'exe':
            run('tcc-standalone', [prefix.with_suffix('.exe') if os.name == 'nt' else prefix])
    original = source.read_text(encoding='utf-8-sig')
    negatives = {
        'missing-header': original.replace(' @header("test/cases/foreign_decl_record.h")', ''),
        'duplicate-header': original.replace('@header("test/cases/foreign_decl_record.h")',
                            '@header("test/cases/foreign_decl_record.h") @header("other.h")'),
        'bad-constructor': original.replace('Rect rect = Rect()', 'Rect rect = Rect(1)'),
        'default-field': original.replace('    c_int x', '    c_int x = 1'),
        'resource-field': original.replace('    c_int x', '    string x'),
        'recursive-field': original.replace('    c_int x', '    Rect x'),
    }
    for name, content in negatives.items():
        case = out / (name+'.xl'); case.write_text(content, encoding='utf-8')
        target = out / (name+'.c')
        run(name, [driver, case, 'cgen-default', target], accepted=False)
        assert not target.exists(), target
    # These mistakes have plausible widths. Only the independent C header,
    # rather than our language declaration, can certify their actual ABI.
    wrong_layouts = {
        'wrong-field-type': original.replace('    c_int x', '    c_float x'),
        'wrong-field-order': original.replace('    c_int x\n    c_int y', '    c_int y\n    c_int x'),
        'wrong-field-count': original.replace('    c_int x', '    c_int extra\n    c_int x'),
    }
    for name, content in wrong_layouts.items():
        content = content.split('func roundTrip(', 1)[0] + '''func main(...) as int
    Rect value = Rect()
    return 0
end
'''
        case = out / (name+'.xl'); case.write_text(content, encoding='utf-8')
        target = out / (name+'.c')
        run(name+'-cgen', [driver, case, 'cgen-default', target])
        run(name+'-C-ABI', ['gcc', '-std=c11', '-c', *includes, target,
            '-o', out/(name+'.o')], accepted=False, diagnostic='C record')
    print('C record layout, value semantics, nested records and C ABI rejection passed.')


    verify_public_build(root, driver.parent)


if __name__ == '__main__':
    main()
