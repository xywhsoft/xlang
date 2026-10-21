"""Build and execute a modified TCC component from the shipped compiler sources."""
from __future__ import annotations
import argparse
import ctypes as c
from ctypes import wintypes as w
from datetime import datetime, timezone
import difflib
import hashlib
import json
import os
from pathlib import Path
import shutil
import sys
import time
import uuid

from native_desktop import NativeDesktopProcess
from package_portable import PackageError, load_json, plain, require, safe_name, sha, verify_payload

PROBE_NAME = 'tcc_xlang_relink_probe_v1'
PROBE_VALUE = 0x584C4931


def worker(dll: Path, output: Path) -> None:
    library = c.CDLL(str(dll.resolve(strict=True)))
    probe = getattr(library, PROBE_NAME)
    probe.argtypes, probe.restype = [], c.c_int
    require(probe() == PROBE_VALUE, 'The modified TCC component did not execute')
    class Scalar(c.Structure):
        _fields_ = [(name, c.c_int) for name in ('version', 'pointer', 'long', 'longDouble', 'longDoubleAlign')]
    query = library.tcc_xlang_scalar_target
    query.argtypes, query.restype = [c.POINTER(Scalar)], c.c_int
    target = Scalar()
    require(query(c.byref(target)) == 1, 'The real TCC target query is missing')
    kernel = c.WinDLL('kernel32', use_last_error=True)
    module_path = kernel.GetModuleFileNameW
    module_path.argtypes, module_path.restype = [w.HMODULE, w.LPWSTR, w.DWORD], w.DWORD
    name = c.create_unicode_buffer(32768)
    count = module_path(library._handle, name, len(name))
    require(0 < count < len(name) and Path(name.value).resolve() == dll.resolve(), 'A different DLL was loaded')
    output.write_text(json.dumps(dict(status='modified-tcc-component-executed', dll=str(dll.resolve()),
        dllSha256=sha(dll), actualLoadedModule=name.value, symbol=PROBE_NAME, value=PROBE_VALUE,
        scalar={key: getattr(target, key) for key, _ in Scalar._fields_}), indent=2) + '\n', encoding='utf8')


def check(args) -> Path:
    require(os.name == 'nt', 'This gate requires Windows x64')
    package = Path(args.package).resolve(strict=True)
    gcc = Path(args.gcc).resolve(strict=True)
    fixture = Path(args.fixture).resolve(strict=True)
    script = Path(__file__).resolve(strict=True)
    output = Path(args.output).absolute()
    require(not output.exists() and not output.resolve().is_relative_to(package), 'Use a new output directory outside the package')
    # The gate never changes the package, source provider or live toolchain.
    for parent in (output.parent, *output.parent.parents):
        if parent.exists(): plain(parent, True)
    output.mkdir(parents=True)
    prior = load_json(args.resume_evidence) if args.resume_evidence else None
    source = Path(prior['source']).resolve(strict=True) if prior else output / 'source'
    if not prior: source.mkdir()
    records = []
    protected = {str(script): sha(script), str(fixture): sha(fixture), str(gcc): sha(gcc)}
    receipt = output / 'evidence.json'
    result = dict(status='preparing', package=str(package), root=str(output),
        source=str(source), records=records, protected=protected,
        startedUtc=datetime.now(timezone.utc).isoformat(), formalRelease=False,
        technicalModifiedTccRelinkVerified=False, relinkReviewComplete=False,
        legalDistributionReviewComplete=False, productionCompilerReplaced=False,
        defaultIdeCompilerReplaced=False, cleanWindowsVerified=False)
    def publish():
        temp = receipt.with_suffix('.tmp')
        temp.write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
        os.replace(temp, receipt)
    publish()
    user = c.WinDLL('user32', use_last_error=True)
    user.CreateDesktopW.restype = w.HANDLE
    user.CreateDesktopW.argtypes = [w.LPCWSTR, w.LPCWSTR, c.c_void_p, w.DWORD, w.DWORD, c.c_void_p]
    user.CloseDesktop.argtypes = [w.HANDLE]
    env = dict(os.environ)
    for key in tuple(env):
        if key.upper().startswith('XL6_'): env.pop(key)
    env['PATH'] = str(gcc.parent) + os.pathsep + env.get('PATH', '')
    env['PYTHONUTF8'] = '1'
    def run(label, argv, *, cwd=None, timeout=240):
        argv = [str(value) for value in argv]
        desktop_name = 'XlangCompilerRelink-' + uuid.uuid4().hex
        desktop = user.CreateDesktopW(desktop_name, None, None, 0, 0x10000000, None)
        require(bool(desktop), 'Cannot create the owned diagnostic desktop')
        started = time.monotonic()
        log = output / (label + '.log')
        row = dict(label=label, arguments=argv, cwd=str(cwd or output), status='running',
            neverSwitchedDesktop=True, log=str(log))
        records.append(row)
        child = None
        publish()
        try:
            with log.open('xb') as stream:
                child = NativeDesktopProcess(argv, stream, stream, desktop_name, cwd=str(cwd or output), env=env)
                row['pid'] = child.pid
                while child.poll() is None:
                    row.update(elapsedSeconds=round(time.monotonic() - started, 3),
                        lastConfirmedLiveUtc=datetime.now(timezone.utc).isoformat())
                    publish()
                    if time.monotonic() - started > timeout:
                        child.kill()
                        row['deadlineExceeded'] = True
                    time.sleep(.5)
                row['exitCode'] = child.wait()
        finally:
            if child: child.close()
            require(bool(user.CloseDesktop(desktop)), 'Cannot release the diagnostic desktop')
            row.update(elapsedSeconds=round(time.monotonic() - started, 3), logSha256=sha(log))
            row['status'] = 'passed' if row.get('exitCode') == 0 else 'failed'
            publish()
        require(row['exitCode'] == 0, f'{label} exited {row["exitCode"]}; inspect {log}')
        print(label, row['exitCode'], flush=True)
        return log
    try:
        manifest = verify_payload(package)
        for row in manifest['files']:
            protected[str(package / row['path'])] = row['sha256']
        protected[str(package / 'package-manifest.json')] = sha(package / 'package-manifest.json')
        materials = load_json(package / 'sources/xlang/SOURCE-MATERIALS.json')
        require(materials.get('sourceFiles') == 1521 and materials.get('rootBuildScripts') == 11,
            'This gate expects the published 1532-input compiler source contract')
        if prior:
            prior_root = Path(args.resume_evidence).resolve(strict=True).parent
            require(source == (prior_root / 'source').resolve(), 'Resume source is outside the original proof')
            require(prior.get('package') == str(package) and prior.get('packageManifestSha256') == sha(package / 'package-manifest.json'), 'Resume package differs')
            require(prior.get('protectedInputsUnchanged') is True and prior.get('status') == 'failed', 'The original incomplete proof is not usable')
            original_script = prior_root / 'check_compiler_relink-v1.py'
            require(original_script.is_file() and sha(original_script) == prior['protected'][str(script)], 'Original checker snapshot differs')
            for path, digest in prior['protected'].items():
                require((path == str(script)) or (Path(path).is_file() and sha(path) == digest), 'An original protected input changed')
            inventory_path = Path(prior['sourceInventory'])
            require(inventory_path.parent == prior_root and sha(inventory_path) == prior['sourceInventorySha256'], 'Resume source inventory differs')
            old_inputs = load_json(inventory_path)
            effective = old_inputs['effectiveInputs']
            require(len(effective) == 1532 and all(Path(path).resolve().is_relative_to(source) and sha(path) == digest for path, digest in effective.items()), 'Resume build inputs differ')
            rebuilt = prior['rebuiltArtifacts']
            require(set(rebuilt) == {'xl.exe','xl.dll','xlc.dll'} and all(sha(source/'release'/name) == digest for name, digest in rebuilt.items()), 'Resume compiler output differs')
            original_passed = prior['records'][:3]
            require([r['label'] for r in original_passed] == ['modified-source-cold-build','xl.dll-modified-tcc','xlc.dll-modified-tcc'] and all(r['exitCode'] == 0 and sha(r['log']) == r['logSha256'] for r in original_passed), 'The actual build or modified DLL probe did not pass')
            for name in ('xl.dll','xlc.dll'):
                probe_path = prior_root/(name+'-probe.json')
                record = load_json(probe_path)
                require(record['value'] == PROBE_VALUE and record['dllSha256'] == rebuilt[name], 'The actual modified TCC result differs')
                protected[str(probe_path)] = sha(probe_path)
            protected[str(original_script)] = sha(original_script)
            protected[str(Path(args.resume_evidence).resolve())] = sha(args.resume_evidence)
            protected[str(inventory_path)] = sha(inventory_path)
            protected.update(effective)
            protected.update({str(source/'release'/name): digest for name, digest in rebuilt.items()})
            result.update(status='running', resumedFrom=str(Path(args.resume_evidence).resolve()), originalSuccessfulCommands=original_passed,
                sourceInputs=1532, retainedExtensionFiles=2085, retainedExtensionsRebuilt=False, sourceInventory=str(inventory_path),
                sourceInventorySha256=sha(inventory_path), packageManifestSha256=sha(package/'package-manifest.json'), rebuiltArtifacts=rebuilt,
                modifiedSource=prior['modifiedSource'], modificationBeforeSha256=prior['modificationBeforeSha256'], modificationAfterSha256=prior['modificationAfterSha256'],
                originalWholeProofFailureRetained=True, compilerRebuildRepeated=False)
            publish()
            print(output, flush=True)
        else:
            inventory = materials['files']
            require(len(inventory) == 1532, 'Incomplete packaged compiler source inventory')
            inputs, names = [], set()
            for row in inventory:
                name = safe_name(row['path'])
                require(name.casefold() not in names, 'Duplicate compiler source path')
                names.add(name.casefold())
                original = package / 'sources/xlang' / name
                plain(original)
                require(sha(original) == row['sha256'], 'Packaged source differs from its materials record')
                target = source / name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(original, target)
                require(sha(target) == row['sha256'], 'Source copy differs')
                inputs.append(dict(path=name, original=str(original), copy=str(target), sha256=row['sha256']))
            require(all(not (source / 'release' / name).exists() for name in ('xl.exe', 'xl.dll', 'xlc.dll'))
                and not (source / 'build').exists(), 'A prebuilt compiler or object directory was copied')
            extensions = []
            prefix = 'toolchains/xlang/extlibs/'
            for row in manifest['files']:
                if row['path'].startswith(prefix):
                    name = row['path'][len(prefix):]
                    target = source / 'release/extlibs' / name
                    target.parent.mkdir(parents=True, exist_ok=True)
                    original = package / row['path']
                    shutil.copy2(original, target)
                    require(sha(target) == row['sha256'], 'Retained extension copy differs')
                    extensions.append(dict(path='release/extlibs/' + name, original=str(original), copy=str(target), sha256=row['sha256']))
            require(len(extensions) == 2085, 'Retained extension inventory differs')
            edited = source / 'lib/tcc/libtcc.c'
            original_bytes = edited.read_bytes()
            require(PROBE_NAME.encode() not in original_bytes, 'The relink probe already exists')
            changed = original_bytes + ('\nLIBTCCAPI int ' + PROBE_NAME + '(void)\n{\n    return 0x584C4931;\n}\n').encode()
            before, after = output / 'tcc-before.c', output / 'tcc-after.c'
            before.write_bytes(original_bytes)
            after.write_bytes(changed)
            edited.write_bytes(changed)
            require(edited.read_bytes() == changed, 'TCC change did not persist')
            (output / 'tcc-relink.patch').write_text(''.join(difflib.unified_diff(
                original_bytes.decode('utf8').splitlines(keepends=True), changed.decode('utf8').splitlines(keepends=True),
                'before/lib/tcc/libtcc.c', 'after/lib/tcc/libtcc.c')), encoding='utf8')
            effective = {row['copy']: row['sha256'] for row in inputs}
            effective[str(edited)] = sha(edited)
            inventory_path = output / 'source-inputs.json'
            inventory_path.write_text(json.dumps(dict(files=inputs, retainedExtensions=extensions,
                modification=dict(path='lib/tcc/libtcc.c', before=str(before), beforeSha256=sha(before),
                    after=str(after), afterSha256=sha(after), symbol=PROBE_NAME, expectedValue=PROBE_VALUE),
                effectiveInputs=effective, inheritedExtensionsRebuilt=False), indent=2) + '\n', encoding='utf8')
            result.update(status='running', packageManifestSha256=sha(package / 'package-manifest.json'),
                sourceInventory=str(inventory_path), sourceInventorySha256=sha(inventory_path),
                sourceInputs=len(inputs), retainedExtensionFiles=len(extensions), retainedExtensionsRebuilt=False,
                modifiedSource='lib/tcc/libtcc.c', modificationBeforeSha256=sha(before), modificationAfterSha256=sha(after),
                sourceOnlyBuildPrepared=True, buildEntry='build.bat')
            publish()
            print(output, flush=True)
            run('modified-source-cold-build', [os.environ.get('COMSPEC', r'C:\Windows\System32\cmd.exe'),
                '/d', '/c', 'build.bat'], cwd=source, timeout=2400)
            require(all(sha(p) == digest for p, digest in effective.items()), 'Effective source inputs changed during build')
            rebuilt = {name: sha(source / 'release' / name) for name in ('xl.exe', 'xl.dll', 'xlc.dll')}
            for name in ('xl.dll', 'xlc.dll'):
                require(rebuilt[name] != materials['runtimeArtifacts'][name], 'The modified DLL did not change')
                probe_output = output / (name + '-probe.json')
                run(name + '-modified-tcc', [sys.executable, '-X', 'utf8', script, '--probe-dll',
                    source / 'release' / name, '--probe-output', probe_output])
                require(load_json(probe_output)['value'] == PROBE_VALUE, 'The new symbol did not execute')
        project_root = output / '工程 源码😀'
        project_root.mkdir()
        entry, native, project, program = (project_root / n for n in ('main.xl', 'native.c', 'app.xlp', 'program.exe'))
        entry.write_text('export "xl_add" func add(c_int a, c_int b) as c_int\n return a+b\nend\n'
            'extern "relink_answer" func relinkAnswer() as c_int\nfunc main(...) as int\n return relinkAnswer()\nend\n', encoding='utf8')
        native.write_text('#include <stdio.h>\n#include <stdarg.h>\n#include <string.h>\n'
            'static int format(char* out, size_t size, const char* fmt, ...) {\n va_list args; va_start(args,fmt);\n'
            ' int (*function)(char*,size_t,const char*,va_list)=vsnprintf;\n int result=function(out,size,fmt,args);\n'
            ' va_end(args); return result;\n}\nint relink_answer(void) {\n char buffer[64];\n'
            ' if(format(buffer,sizeof(buffer),"%s-%d","relink",42)!=9 || strcmp(buffer,"relink-42")) return 4;\n'
            ' if(xl_add(19,23)!=42) return 5;\n puts("modified TCC relink and C/xlang passed"); return 0;\n}\n', encoding='utf8')
        project.write_text(json.dumps(dict(schema=1, name='Modified TCC relink', kind='exe', entry='main.xl',
            files=['main.xl','native.c'], cSources=['native.c'])), encoding='utf8')
        compiler = source / 'release/xl.exe'
        result['rebuiltArtifacts'] = rebuilt
        publish()
        baseline_log = run('published-native-environment', [package / 'toolchains/xlang/xl.exe', 'c-bridge',
            project, '--json', '--c-environment'], cwd=project_root)
        rebuilt_log = run('relinked-native-environment', [compiler, 'c-bridge', project, '--json', '--c-environment'], cwd=project_root)
        for path in (baseline_log, rebuilt_log): require(path.stat().st_size <= 64*1024*1024, 'C environment response exceeds its 64 MiB contract')
        baseline_query, rebuilt_query = (json.loads(p.read_text(encoding='utf-8-sig')) for p in (baseline_log, rebuilt_log))
        require(baseline_query['ok'] and rebuilt_query['ok'], 'A real environment query failed')
        baseline_env, rebuilt_env = (q['cBridge']['nativeEnvironment'] for q in (baseline_query, rebuilt_query))
        require(baseline_env['scalarTarget'] == rebuilt_env['scalarTarget'] and
            baseline_env['recordLayout'] == rebuilt_env['recordLayout'], 'Native target profiles changed')
        require(baseline_query['cBridge']['declarations'] == rebuilt_query['cBridge']['declarations'], 'ABI declarations changed')
        run('relinked-mixed-build', [compiler, 'build', project, '-o', program], cwd=project_root)
        executed = run('relinked-mixed-execute', [program], cwd=project_root)
        require('modified TCC relink and C/xlang passed' in executed.read_text(encoding='utf8'), 'The actual mixed program did not execute')
        extension_checker = script.parent / 'check_c_extensions.py'
        protected[str(extension_checker)] = sha(extension_checker)
        run('relinked-real-ide-extensions', [sys.executable, '-X', 'utf8', extension_checker,
            '--compiler', compiler, '--server', package / 'toolchains/clangd/bin/clangd.exe',
            '--fixture', fixture, '--output', output / 'ide-extensions'], timeout=600)
        result.update(status='modified-shipped-tcc-source-relink-and-real-ide-bridge-passed',
            technicalModifiedTccRelinkVerified=True, unchangedScalarTarget=rebuilt_env['scalarTarget'],
            unchangedRecordLayout=rebuilt_env['recordLayout'], abiDeclarationsUnchanged=True)
    except Exception as error:
        result.update(status='failed', error=str(error))
        raise
    finally:
        changes = [p for p, digest in protected.items() if not Path(p).is_file() or sha(p) != digest]
        result.update(protectedChanges=changes, protectedInputsUnchanged=not changes,
            finishedUtc=datetime.now(timezone.utc).isoformat())
        if changes: result['status'] = 'failed-protected-input-changes'
        publish()
    require(not result['protectedChanges'], 'Protected original inputs changed')
    print(json.dumps(dict(status=result['status'], actualCommands=len(records)+len(result.get('originalSuccessfulCommands',[])), evidence=str(receipt))), flush=True)
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--gcc', type=Path)
    parser.add_argument('--fixture', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--resume-evidence', type=Path)
    parser.add_argument('--probe-dll', type=Path)
    parser.add_argument('--probe-output', type=Path)
    args = parser.parse_args()
    if args.probe_dll:
        require(args.probe_output is not None, 'The worker output path is required')
        worker(args.probe_dll, args.probe_output)
    else:
        require(all((args.package, args.gcc, args.fixture, args.output)), 'Package, GCC, fixture and output are required')
        check(args)
