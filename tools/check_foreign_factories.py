"""Offline declaration-driven C status/output factories and const record inputs."""
from pathlib import Path
from compiler_response import command_with_response
from xlir_public_build_inputs import verify_public_build
from runtime_layout import runtime_directory
import argparse, hashlib, json, os, shutil, subprocess, time


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('root', 'driver', 'source-build', 'out'):
        p.add_argument('--'+key, type=Path, required=True)
    a = p.parse_args()
    root, driver, out = a.root.resolve(), a.driver.resolve(), a.out.resolve()
    source_build, source_inputs, _ = verify_public_build(root, driver.parent)
    assert Path(source_build['Driver']).resolve() == driver
    source_objects = [x['Path'] for x in source_build['Objects'] if Path(x['Source']).name not in
                      ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    source_systems = ['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm']
    out.mkdir(parents=True, exist_ok=False)
    source = root/'test/cases/foreign_decl_factory.xl'
    provider = root/'test/cases/foreign_decl_factory_native.c'
    env = dict(os.environ, XL6_SOURCE_NATIVE_ROOT=str(root))
    rows = []

    def run(name, command, accepted=True, diagnostic=None):
        if str(command[0]) == 'gcc' and not any(str(x).startswith('@') for x in command[1:]):
            command = command_with_response(command, 'gcc', out/(name+'.rsp'))
        command = list(map(str, command)); started = time.monotonic()
        result = subprocess.run(command, cwd=root, env=env, capture_output=True, timeout=240)
        log = out/(name+'.log'); log.write_bytes(result.stdout+result.stderr)
        rows.append(dict(name=name, command=command, exitCode=result.returncode, accepted=accepted,
                         log=str(log), sha256=hashlib.sha256(log.read_bytes()).hexdigest(),
                         seconds=round(time.monotonic()-started, 3)))
        (out/'stages.json').write_text(json.dumps(rows, indent=2)+'\n', encoding='utf-8')
        print(name, result.returncode, flush=True)
        assert (result.returncode == 0) == accepted, log
        if diagnostic:
            assert diagnostic in log.read_text(encoding='utf-8-sig', errors='replace'), log

    generated = out/'factory.c'
    run('cgen', [driver, source, 'cgen-default', generated])
    text = generated.read_text(encoding='utf-8-sig')
    assert 'XLIR_NATIVE_ASSERT(XLIR_NATIVE_IS_TYPE(&foreign_factory_create, int (*)(int, foreign_factory_widget**, const foreign_factory_config*)), "declared C function ABI");' in text
    assert '.e0 = foreign_factory_create(' in text and 'foreign_factory_create_token(' in text
    runtime = runtime_directory(root)/('xl.dll' if os.name == 'nt' else 'libxl.so')
    if os.name == 'nt': shutil.copy2(runtime, out/runtime.name)
    includes = ['-I'+str(root), '-I'+str(runtime_directory(root)/'include'), '-I'+str(driver.parent/'include')]
    warnings = ['-Wno-unused-function', '-Wno-unused-variable', '-Wno-unused-label',
                '-Wno-unused-const-variable', '-Wno-unused-but-set-variable']
    ext = '.exe' if os.name == 'nt' else '.out'
    for opt in ('O0', 'O2'):
        executable = out/('factory-'+opt+ext)
        run(opt+'-build', ['gcc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', *warnings, '-'+opt,
            *includes, generated, provider, root/'test/cases/foreign_decl_module_host.c', *source_objects, runtime, *source_systems, '-o', executable])
        run(opt+'-run', [executable])
    for mode in ('run', 'module', 'exe'):
        prefix = out/('native-'+mode)
        run('tcc-'+mode, [driver, source, mode, prefix, provider])
        if mode == 'exe': run('tcc-standalone', [prefix.with_suffix('.exe') if os.name == 'nt' else prefix])
    original = source.read_text(encoding='utf-8-sig')
    native_text = provider.read_text(encoding='utf-8-sig').replace(
        '#include "foreign_decl_factory.h"', '#include "test/cases/foreign_decl_factory.h"')
    for name, status in (('positive-status', 1), ('negative-status', -7), ('minimum-status', -2147483648)):
        case, native = out/(name+'.xl'), out/(name+'_provider.c')
        case.write_text(original.replace('@factory(1, 0,', '@factory(1, '+str(status)+','), encoding='utf-8')
        native.write_text(native_text.replace('return mode == 1 ? -9 : 0;',
            'return mode == 1 ? -9 : '+str(status)+';'), encoding='utf-8')
        run(name, [driver, case, 'run', out/name, native])
    declarations = original.split('func main(', 1)[0]
    package = out/'factory_api.xl'
    package.write_text('\n'.join(('export '+line) if line.startswith('extern ') and
        'func dispose(' not in line else line for line in declarations.splitlines())+'\n', encoding='utf-8')
    imported = out/'imported.xl'
    imported.write_text('''import "./factory_api"
func main(...) as int
    auto config = factory_api.Config()
    config.value = 71
    assert factory_api.readConfig(config) == 71
    auto widget = factory_api.create(0, config)
    assert widget.value == 71
    // The provider's release method is intentionally private. Use an exact
    // declared C alias in the consumer to release its explicit caller owner.
    release(widget)
    bool caught = false
    try
        auto partial = factory_api.create(1, config)
    catch error
        caught = true
    end
    assert caught and factory_api.live() == 0
    return 0
end
extern "foreign_factory_destroy" @no_raise @nonblocking func release(factory_api.Widget widget)
''', encoding='utf-8')
    run('relative-module', [driver, imported, 'run', out/'imported', provider])
    for name, content in {
        'bad-ref-index': declarations.replace('@c_ref(0)', '@c_ref(3)'),
        'scalar-ref': declarations.replace('@c_ref(1)', '@c_ref(0)'),
        'duplicate-ref': declarations.replace('@c_ref(0)', '@c_ref(0) @c_ref(0)'),
        'bad-output-index': declarations.replace('@factory(1,', '@factory(4,'),
        'duplicate-factory': declarations.replace('@factory(1, 0, dispose)', '@factory(1, 0, dispose) @factory(1, 0, dispose)'),
        'unknown-rollback': declarations.replace('@factory(1, 0, dispose)', '@factory(1, 0, unknown)'),
        'missing-callee-effect': declarations.replace('"foreign_factory_create" @no_raise', '"foreign_factory_create"'),
        'pure-factory': declarations.replace('"foreign_factory_create" @no_raise', '"foreign_factory_create" @pure @no_raise'),
        'fallible-rollback': declarations.replace('"foreign_factory_destroy" @no_raise', '"foreign_factory_destroy"'),
        'wrong-rollback-type': declarations.replace('func dispose(Widget widget)', 'func dispose(Config widget)'),
        'wrong-rollback-result': declarations.replace('func dispose(Widget widget)', 'func dispose(Widget widget) as c_int'),
        'nullable-result': declarations.replace('as Widget\n', 'as Widget?\n'),
        'record-result': declarations.replace('as Widget\n', 'as Config\n'),
        'status-overflow': declarations.replace('@factory(1, 0,', '@factory(1, 2147483648,'),
        'unknown-contract': declarations.replace('@c_ref(0)', '@unimplemented(0)'),
    }.items():
        case, target = out/(name+'.xl'), out/(name+'.c')
        case.write_text(content, encoding='utf-8')
        run(name, [driver, case, 'cgen-default', target], False)
        assert not target.exists(), target
    build = json.loads(a.source_build.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and Path(build['Driver']).resolve() == driver
    for row in build['Objects']:
        assert hashlib.sha256(Path(row['Path']).read_bytes()).hexdigest() == row['Sha256'].lower()
    objects = [r['Path'] for r in build['Objects'] if Path(r['Source']).name not in
               ('xlir_public_source_driver.c', 'xlir_public_source_linux_driver.c')]
    systems = ['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if os.name == 'nt' else \
        ['-Wl,--export-dynamic', '-pthread', '-ldl', '-lm', '-Wl,-rpath,'+str(runtime.parent)]
    for opt in ('O0', 'O2'):
        executable = out/('IR-'+opt+ext)
        arguments = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED', '-'+opt,
                     '-I'+str(runtime_directory(root)), *includes, root/'test/cases/foreign_decl_factory_ir_contract.c',
                     *objects, runtime, *systems, '-o', executable]
        rsp = out/('IR-'+opt+'.rsp')
        rsp.write_text('\n'.join('"'+str(v).replace('\\', '/')+'"' for v in arguments)+'\n', encoding='utf-8')
        run('IR-'+opt+'-build', ['gcc', '@'+str(rsp)])
        run('IR-'+opt+'-run', [executable, source])


    verify_public_build(root, driver.parent)


if __name__ == '__main__':
    main()
