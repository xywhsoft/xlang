"""Qualify owned callback-group declarations/IR/Cgen over a whole source cohort.

CPU-only direct/bound/protocol callbacks, shared userdata, nullable members,
failure/clear reentry, exact scalar captures, OOM and warm allocation checks.
The actual shipping compiler/VFS and HostSession remain separate gates.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time
from foreign_validation_build import load_build
from compiler_response import command_with_response


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'build', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    root, out = a.root.resolve(), a.out.resolve()
    b = load_build(build=a.build)
    cohort, runtime = b['cohort'], b['runtime']
    win = os.name == 'nt'
    sha = lambda v: hashlib.sha256(Path(v).read_bytes()).hexdigest()
    tools = {v: Path(shutil.which(v)).resolve() for v in ('gcc', 'g++', 'objdump')}
    files = [Path(__file__), Path(__file__).with_name('foreign_validation_build.py'),
             Path(__file__).with_name('compiler_response.py'), *tools.values()]
    case = root / 'test/cases/foreign_pack_owned_callback.xl'
    native = case.with_name('foreign_pack_owned_callback_native.c')
    scalar = root / 'test/cases/closure_native_scalar_cells.xl'
    driver_source = case.with_name('foreign_pack_owned_callback_driver.c')
    checker = case.with_name('foreign_pack_owned_callback_ir.c')
    host_source = case.with_name('foreign_pack_owned_callback_host.c')
    files += [case, native, case.with_suffix('.h'), scalar, driver_source, checker, host_source]
    regressions = {'foreign_pack': 'foreign_pack_native.c',
                   'foreign_sync_callback': 'foreign_sync_callback_native.c',
                   'foreign_nested_pack_null': 'foreign_nested_pack_null_native.c',
                   'foreign_pack_pod': 'foreign_pack_pod_native.c',
                   'foreign_pack_callback': 'foreign_pack_callback.c'}
    for stem, provider in regressions.items():
        files += [root / 'test/cases' / (stem + suffix) for suffix in ('.xl', '.h')]
        files.append(root / 'test/cases' / provider)
    inputs = dict(b['inputs'])
    inputs.update((str(v), sha(v)) for v in files)
    freeze_path = cohort.parent / 'freeze.json'
    freeze = json.loads(freeze_path.read_text())
    assert all(sha(root / n) == digest for n, digest in freeze['overrides'].items())
    inputs[str(freeze_path)] = sha(freeze_path)
    out.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith('XL6_') and k != 'XLANG_PACKAGE_PATH'}
    env.update(TMP=str(out), TEMP=str(out), TMPDIR=str(out), PYTHONDONTWRITEBYTECODE='1', PYTHONUTF8='1')
    key = 'PATH' if win else 'LD_LIBRARY_PATH'
    env[key] = str(runtime.parent) + os.pathsep + env.get(key, '')
    if win:
        shutil.copy2(runtime, out / runtime.name)
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs, rows=[],
                  fullSourceBuild=True, objectOverrides=[], fullCompilerVfsBuild=False,
                  shippingDelivery=False, networkTests=False)

    def save():
        temp = out / 'results.json.tmp'
        temp.write_text(json.dumps(report, indent=2) + '\n')
        os.replace(temp, out / 'results.json')

    def run(name, command, reject=False, contains=None):
        command = list(map(str, command))
        actual = command_with_response(command, str(tools['gcc']), out / (name + '.rsp'))
        start = time.monotonic()
        result = subprocess.run(actual, cwd=cohort, env=env, capture_output=True, timeout=360)
        raw = result.stdout + result.stderr
        log = out / (name + '.log')
        log.write_bytes(raw)
        report['rows'].append(dict(name=name, command=command, exitCode=result.returncode,
                                   expectedFailure=reject, seconds=time.monotonic() - start, logSha256=sha(log)))
        save()
        print(name, result.returncode, flush=True)
        assert bool(result.returncode) == reject, log
        if contains is not None:
            assert contains in raw, log
        return raw

    try:
        sdk = Path(b['base'].get('RuntimeDirectory', b['base'].get('SdkDirectory', '')))
        headers = b['basePath'].parent / 'include'
        core = (b['basePath'].parent / 'xlc.dll' if win else
                cohort.parent / 'native/release/linux_x64/libxlc.so')
        dist = cohort / 'res/tcc/win64/x86_64' if win else cohort.parent / 'native/build/linux/tcc-dist'
        if not win and not dist.exists():
            producer = cohort.parent.parent / 'compiler-callback-group-linux-full-r1/build/tcc-dist'
            shutil.copytree(producer, dist)
        inputs[str(core)] = sha(core)
        inputs.update((str(v), sha(v)) for v in dist.rglob('*') if v.is_file())
        inc = ['-I' + str(v) for v in (cohort, root, headers, sdk, sdk / 'include')]
        flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
                 '-DXL_STATIC_EMBED', *inc]
        system = (['-lws2_32', '-liphlpapi', '-lshell32', '-lole32', '-ladvapi32'] if win else
                  ['-Wl,--export-dynamic', '-pthread', '-ldl', '-lm', '-Wl,-rpath,' + str(runtime.parent)])
        driver = out / ('driver.exe' if win else 'driver')
        run('driver-build', [tools['gcc'], *flags, '-O0', driver_source, *b['objects'], runtime, *system, '-o', driver])
        for opt in ('O0', 'O2'):
            check = out / ('ir-' + opt + ('.exe' if win else ''))
            run('ir-' + opt + '-build', [tools['gcc'], *flags, '-' + opt, checker,
                                        *b['objects'], runtime, *system, '-o', check])
            run('ir-' + opt + '-run', [check, case], contains=b'PACK_OWNED_CALLBACK_IR_OK')

        def invoke(name, source, provider, mode, target, reject=False, contains=None):
            return run(name, [driver, source, provider, mode, target, core, dist, headers, root], reject, contains)

        for label, fixture, provider, marker in (
            ('group', case, native, b'PACK_OWNED_CALLBACK_OK'),
            ('scalar', scalar, '-', b'NATIVE_SCALAR_CELLS_OK')):
            generated = out / (label + '.c')
            invoke(label + '-emit', fixture, provider, 'emit', generated)
            invoke(label + '-jit', fixture, provider, 'run', out / (label + '-jit'), contains=marker)
            header = Path(str(generated) + '.h')
            for std, tool, language in (('c11', 'gcc', 'c'), ('c++11', 'g++', 'c++'), ('c++17', 'g++', 'c++')):
                run(label + '-header-' + std, [tools[tool], '-std=' + std, '-Wall', '-Wextra', '-Werror',
                                               '-DXL_STATIC_EMBED', *inc, '-x', language, '-fsyntax-only', header])
            binary = out / (label + ('.exe' if win else ''))
            invoke(label + '-exe-build', fixture, provider, 'exe', binary)
            run(label + '-exe-run', [binary], contains=marker)
            shared = out / (label + ('.dll' if win else '.so'))
            shared_source = out / (label + '-shared.xl')
            shared_source.write_text(fixture.read_text().replace('func main(...) as int',
                                     'export "pack_check" func check() as int'))
            inputs[str(shared_source)] = sha(shared_source)
            invoke(label + '-shared-build', shared_source, provider, 'dll', shared)
            host = out / (label + '-shared-host.py')
            host.write_text('import ctypes,sys\nlib=ctypes.CDLL(sys.argv[1])\n'
                            'lib.xl_script_module_init.argtypes=[ctypes.c_void_p]\n'
                            'assert lib.xl_script_module_init(None)==1\n'
                            'f=lib.pack_check\nf.restype=ctypes.c_int64\n'
                            'assert f()==0 and f()==0\n'
                            'assert lib.xl_script_module_unit_checked()==1\n')
            run(label + '-shared-run', [sys.executable, '-B', host, shared], contains=marker)
            for target in (binary, shared):
                imports = run(target.name + '-imports', [tools['objdump'], '-p', target]).lower()
                names = (re.findall(rb'dll name:\s*(\S+)', imports) if win else
                         re.findall(rb'\bneeded\s+(\S+)', imports))
                assert not {b'xl.dll', b'xlc.dll', b'libxl.so', b'libxlc.so', b'libtcc.dll', b'libtcc.so'}.intersection(names)
            for opt in ('O0', 'O2'):
                program = out / (label + '-' + opt + ('.exe' if win else ''))
                warnings = ['-Wno-' + v for v in ('unused-function', 'unused-variable', 'unused-label',
                            'unused-const-variable', 'unused-but-set-variable', 'cast-function-type')]
                providers = [native] if label == 'group' else []
                run(label + '-' + opt + '-build', [tools['gcc'], *flags, *warnings, '-' + opt, generated,
                    *providers, host_source, runtime, *system, '-o', program])
                raw = run(label + '-' + opt + '-run', [program], contains=marker)
                if label == 'group':
                    assert b'PACK_GROUP_OOM_OK hotAlloc=0' in raw
        for stem, provider in regressions.items():
            # This fixture has its automatically discovered same-name .c;
            # unlike *_native.c providers it must not be added a second time.
            native_source = '-' if stem == 'foreign_pack_callback' else root / 'test/cases' / provider
            invoke('legacy-' + stem, root / 'test/cases' / (stem + '.xl'), native_source,
                   'run', out / stem)
        native_header = case.with_suffix('.h').read_text()
        for label, old, new in (
            ('value-order', 'int (*value)(int,void*)', 'int (*value)(void*,int)'),
            ('notice-order', 'void (*notice)(void*,int)', 'void (*notice)(int,void*)'),
            ('receiver-constness', 'int (*value)(const struct GpReceiver*,int)',
             'int (*value)(struct GpReceiver*,int)'),
            ('userdata-type', 'void* user;', 'const void* user;'),
            ('owner-argument', 'int gp_install(const GpDesc*,const GpOwner*);',
             'int gp_install(const GpDesc*,const void*);')):
            assert old in native_header
            folder = out / label
            changed = folder / 'test/cases/foreign_pack_owned_callback.h'
            changed.parent.mkdir(parents=True)
            changed.write_text(native_header.replace(old, new, 1))
            inputs[str(changed)] = sha(changed)
            for standard, tool, language in (('c11', 'gcc', 'c'), ('c++11', 'g++', 'c++')):
                run('abi-reject-' + label + '-' + standard,
                    [tools[tool], '-std=' + standard, '-Wall', '-Wextra', '-Werror', '-DXL_STATIC_EMBED',
                     '-I' + str(folder), *inc, '-x', language, '-fsyntax-only', out / 'group.c'], reject=True)
        original = case.read_text()
        attribute = '@c_pack_callback_group(0,"user",1)'
        bad = {
            'missing-group': original.replace(attribute, '', 1),
            'duplicate-group': original.replace(attribute, attribute + ' ' + attribute, 1),
            'owner-overlap': original.replace(attribute, '@c_pack_callback_group(0,"user",0)', 1),
            'owner-out-of-range': original.replace(attribute, '@c_pack_callback_group(0,"user",64)', 1),
            'owner-unreachable': original.replace(attribute, '@c_pack_callback_group(0,"user",63)', 1),
            'group-without-pack': original.replace('@c_pack(0,"GpDesc",0,3)', '', 1),
            'userdata-expression': original.replace(attribute, '@c_pack_callback_group(0,"user->field",1)', 1),
            'userdata-collision': original.replace(attribute, '@c_pack_callback_group(0,"value",1)', 1),
            'userdata-parent': original.replace(attribute, '@c_pack_callback_group(0,"nested.user",1)', 1),
            'not-retained': original.replace('@c_owned_callback(0,-1,"GpOwner",-111)', '@c_sync_callback(0,-1,-111)', 1),
            'different-owner': original.replace('@c_owned_callback(0,-1,"GpOwner",-111)', '@c_owned_callback(0,-1,"OtherOwner",-111)', 1),
            'individual-userdata': original.replace('@c_pack_field(0,"notice",1,"owned_callback")', '@c_pack_field(0,"notice",1,"callback_user")', 1),
            'scalar-receiver': original.replace('@c_owned_callback(0,-1,"GpOwner",-111)',
                                                '@c_owned_callback(0,-1,"GpOwner",-111,"receiver")', 1),
            'unpacked-receiver': original + '\nextern "gp_unpacked" @no_raise '
                '@c_owned_callback(0,-1,"GpOwner",-111,"receiver") func unpacked(ReceiverValue? value)\n',
        }
        for label, text in bad.items():
            assert text != original
            source = out / (label + '.xl')
            source.write_text(text)
            inputs[str(source)] = sha(source)
            generated = out / (label + '.c')
            invoke('reject-' + label, source, '-', 'emit', generated, reject=True)
            assert not generated.exists() and not Path(str(generated) + '.h').exists()
        report.update(complete=True, inputsUnchanged=all(sha(v) == digest for v, digest in inputs.items()))
        report['passed'] = report['inputsUnchanged']
        assert report['passed']
    except Exception as error:
        report.update(complete=True, passed=False, error=str(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
