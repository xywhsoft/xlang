"""Qualify ordinary/group source selection with a complete O2/TCC/XRT/VFS build.

CPU-only JIT, standalone and shared artifacts with freshly staged private SDK
headers; inspect ownership-domain initializers and immutable VFS object bytes. This verifies component isolation,
not byte-for-byte machine-code pruning or language callback-group pack support.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time
from full_compiler_qualification import verify_full_compiler


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'compiler', 'compiler-report', 'out'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    root, compiler, build, out = [v.resolve() for v in
                                 (a.root, a.compiler, a.compiler_report, a.out)]
    metadata = verify_full_compiler(build, compiler)
    snapshot = Path(metadata['source'])
    win = os.name == 'nt'
    sha = lambda v: hashlib.sha256(Path(v).read_bytes()).hexdigest()
    runtime = compiler.parent / ('xlc.dll' if win else 'libxlc.so')
    manifest = (snapshot / 'build/tmp/precompiled/manifest.json' if win else
                build.parent / 'build/precompiled/manifest.json')
    objects = json.loads(manifest.read_text())['objects']
    selected = [v for v in objects if v['virtual'] in {
        '__xl_objects/static-compiler/src/runtime/xl6_native_callback.c.o',
        '__xl_objects/static-compiler/src/runtime/xl6_native_callback_group.c.o'}]
    assert len(selected) == 2
    files = [compiler, runtime, build, manifest, Path(__file__),
             Path(__file__).with_name('full_compiler_qualification.py')]
    files += [root / ('test/cases/foreign_callback_component_' + label + '.xl')
              for label in ('ordinary', 'group')]
    files += [Path(v['source']) for v in selected]
    objdump = Path(shutil.which('objdump')).resolve()
    files.append(objdump)
    inputs = {str(v): sha(v) for v in files}
    out.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith('XL6_') and
           k != 'XLANG_PACKAGE_PATH'}
    env.update(TMP=str(out), TEMP=str(out), TMPDIR=str(out), PYTHONUTF8='1',
               PYTHONDONTWRITEBYTECODE='1')
    if not win:
        env['LD_LIBRARY_PATH'] = str(compiler.parent) + ':' + env.get('LD_LIBRARY_PATH', '')
    report = dict(complete=False, passed=False, scope=__doc__, inputs=inputs,
                  rows=[], programs=[], fullCompilerBuild=True,
                  shippingDelivery=False, networkTests=False,
                  compilerPackSupport=False, machineCodePruningMeasured=False)

    def save():
        temp = out / 'results.json.tmp'
        temp.write_text(json.dumps(report, indent=2) + '\n')
        os.replace(temp, out / 'results.json')

    def run(name, command, contains=None):
        command = list(map(str, command))
        start = time.monotonic()
        r = subprocess.run(command, cwd=out, env=env, capture_output=True, timeout=360)
        log = out / (name + '.log')
        raw = r.stdout + r.stderr
        log.write_bytes(raw)
        report['rows'].append(dict(name=name, command=command, exitCode=r.returncode,
                                   seconds=time.monotonic() - start, logSha256=sha(log)))
        save()
        print(name, r.returncode, flush=True)
        assert not r.returncode, log
        if contains is not None:
            assert contains in raw, log
        return raw

    def imports(name, binary):
        raw = run(name + '-imports', [objdump, '-p', binary]).lower()
        names = (re.findall(rb'dll name:\s*(\S+)', raw) if win else
                 re.findall(rb'\bneeded\s+(\S+)', raw))
        assert not {b'xl.dll', b'xlc.dll', b'libxl.so', b'libxlc.so',
                    b'libtcc.dll', b'libtcc.so'}.intersection(names)
        report['programs'].append(dict(path=str(binary), bytes=binary.stat().st_size,
                                       sha256=sha(binary), imports=[v.decode() for v in names]))

    try:
        stage = snapshot / 'tools/stage_runtime_headers.py'
        headers = out / 'runtime-sdk'
        for path in (stage, snapshot / 'tools/gen_tcc_vfs_resources.py',
                     snapshot / 'tools/tcc_vfs_manifest_linux_xlc.txt'):
            inputs[str(path)] = sha(path)
        run('stage-headers', [sys.executable, '-B', stage, '--root', snapshot, '--out', headers])
        inputs.update((str(v), sha(v)) for v in headers.rglob('*') if v.is_file())
        # CLI defaults search its adjacent SDK before additional include paths.
        # Assemble a matched, isolated execution tree instead of modifying the
        # immutable build's inherited SDK or silently shadowing that default.
        original = compiler
        execution = out / 'runtime'
        execution.mkdir()
        sdk = original.parent / 'include'
        inputs.update((str(v), sha(v)) for v in sdk.rglob('*') if v.is_file())
        shutil.copytree(sdk, execution / 'include')
        shutil.copytree(headers / 'include', execution / 'include', dirs_exist_ok=True)
        names = ('xl.exe', 'xl.dll', 'xlc.dll') if win else ('xl', 'libxl.so', 'libxlc.so')
        for name in names:
            inputs[str(original.parent / name)] = sha(original.parent / name)
            shutil.copy2(original.parent / name, execution / name)
        inputs.update((str(v), sha(v)) for v in execution.rglob('*') if v.is_file())
        compiler = execution / original.name
        verify_full_compiler(build, compiler)
        report['executionCompiler'] = str(compiler)
        report['privateHeadersStagedFrom'] = str(snapshot)
        if not win:
            env['LD_LIBRARY_PATH'] = str(execution) + ':' + env.get('LD_LIBRARY_PATH', '')
        # Read immutable catalog bytes, not a disk/dynamic substitute. The
        # producer manifest and delivered DLL must agree on both changed units.
        extract = out / 'read_objects.py'
        extract.write_text(
            'import ctypes,hashlib,json,sys\n'
            'lib=ctypes.CDLL(sys.argv[1])\n'
            'read=lib.tcc_vfs_read_builtin\n'
            'read.argtypes=[ctypes.c_char_p,ctypes.c_void_p,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]\n'
            'read.restype=ctypes.c_int\n'
            'rows=json.loads(sys.argv[2])\n'
            'for row in rows:\n'
            ' size=ctypes.c_size_t()\n'
            ' assert read(row["virtual"].encode(),None,0,ctypes.byref(size))==1\n'
            ' data=ctypes.create_string_buffer(size.value)\n'
            ' assert read(row["virtual"].encode(),data,size.value,ctypes.byref(size))==1\n'
            ' assert hashlib.sha256(data.raw).hexdigest()==row["sha256"]\n'
            ' print(row["virtual"],size.value,row["sha256"])\n')
        inputs[str(extract)] = sha(extract)
        run('vfs-objects', [sys.executable, '-B', extract, runtime,
                            json.dumps(selected)])
        report['embeddedObjects'] = selected
        include = ['--inc-path', root, '--inc-path', compiler.parent / 'include']
        domain = 'xl6OwnershipDomainNativeCallbackGroupV1Get'
        for label in ('ordinary', 'group'):
            fixture = root / ('test/cases/foreign_callback_component_' + label + '.xl')
            marker = (label.upper() + '_CALLBACK_COMPONENT_OK').encode()
            run(label + '-jit', [compiler, 'run', fixture, *include], marker)
            generated = out / (label + '.c')
            run(label + '-cgen', [compiler, 'cgen', fixture, '--emit', generated, *include])
            text = generated.read_text(encoding='utf-8-sig')
            getter = ('xl6NativeCallbackGroupRuntimeV1Get' if label == 'group' else
                      'xl6NativeCallbackRuntimeV1Get')
            assert getter + '()' in text
            inputs[str(generated)] = sha(generated)
            binary = out / (label + ('.exe' if win else ''))
            raw = run(label + '-build', [compiler, 'build', fixture, '-o', binary,
                                        '--save-temps', *include])
            match = re.search(rb'temporary files kept: ([^\r\n]+)', raw)
            assert match, raw
            temporary = Path(match[1].decode('utf-8')).resolve()
            assert temporary.is_relative_to(out), temporary
            native_source = temporary / 'module.c'
            emitted = native_source.read_text(encoding='utf-8-sig')
            assert (domain + '()' in emitted) == (label == 'group')
            inputs[str(native_source)] = sha(native_source)
            run(label + '-run', [binary], marker)
            imports(label, binary)
            shared = out / (label + '-shared.xl')
            shared.write_text(fixture.read_text().replace(
                'func main(...) as int', 'export "component_check" func check() as int'))
            library = out / (label + ('.dll' if win else '.so'))
            run(label + '-shared-build', [compiler, 'build', shared, '--shared', '-o', library, *include])
            host = out / (label + '-host.py')
            host.write_text('import ctypes,sys\nlib=ctypes.CDLL(sys.argv[1])\n'
                            'lib.component_check.restype=ctypes.c_int\n'
                            'assert lib.component_check()==0 and lib.component_check()==0\n'
                            'lib.xl_script_module_unit_checked.restype=ctypes.c_int\n'
                            'assert lib.xl_script_module_unit_checked()==1\n')
            run(label + '-shared-run', [sys.executable, '-B', host, library], marker)
            imports(label + '-shared', library)
        verify_full_compiler(build, compiler)
        report.update(complete=True, inputsUnchanged=all(sha(v) == h for v, h in inputs.items()))
        report['passed'] = report['inputsUnchanged']
        assert report['passed']
    except Exception as error:
        report.update(complete=True, passed=False, error=str(error))
        raise
    finally:
        save()


if __name__ == '__main__':
    main()
