"""Real GCC AMD64 COFF archives consumed by the modified TinyCC.

The primary gates exercise data, callbacks in both directions, double and
aggregate ABI, native COMDAT selection, constructors/destructors, live/dead
undefined symbols and invalid object bounds. GPU/network operations are absent.
"""
from pathlib import Path
import argparse, hashlib, json, os, shutil, struct, subprocess, time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('root', 'tcc', 'distribution', 'out'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--implementation-root',type=Path,help='Read lib/tcc from this previously frozen implementation root; fixtures still come from --root')
    parser.add_argument('--emit-pe',action='store_true',help='Explicitly emit/inspect TinyCC PE; default only object + direct JIT')
    parser.add_argument('--build-jit-provider',action='store_true',help='Build isolated GCC O2 no-VFS libtcc DLL from the same snapshot')
    parser.add_argument('--execute-pe', action='store_true', help='Explicitly execute emitted TinyCC PE; default only inspects it')
    args = parser.parse_args()
    args.emit_pe=args.emit_pe or args.execute_pe
    root, tcc, distribution, out = (v.resolve() for v in
        (args.root, args.tcc, args.distribution, args.out))
    assert os.name == 'nt' and not out.exists()
    assert not out.is_relative_to(root / 'release')
    out.mkdir(parents=True)
    implementation=(args.implementation_root or root).resolve()
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    sources = [root / ('test/cases/tcc_coff_' + name + '.c') for name in
        ('native', 'client', 'duplicate', 'api')]
    sources.append(root/'test/cases/tcc_coff_cpp_comdat.cpp')
    report = dict(complete=False, passed=False, scope=__doc__,
        inputs={str(p): sha(p) for p in [tcc, *sources]}, rows=[])

    def save():
        (out / 'results.json').write_text(json.dumps(report, indent=2) + '\n')

    def run(name, command, success=True, required=True):
        start = time.monotonic()
        try:
            result = subprocess.run(list(map(str, command)), cwd=out,
                capture_output=True, timeout=180)
        except OSError as error:
            report['rows'].append(dict(name=name, command=list(map(str, command)),
                exitCode=None, expectedSuccess=success, error=str(error),
                winerror=getattr(error, 'winerror', None)))
            report['blockedBySecurityProduct'] = getattr(error, 'winerror', None) == 225
            save()
            raise
        log = out / (name + '.log')
        log.write_bytes(result.stdout + result.stderr)
        report['rows'].append(dict(name=name, command=list(map(str, command)),
            exitCode=result.returncode, expectedSuccess=success,
            seconds=round(time.monotonic() - start, 3), logSha256=sha(log),passed=(result.returncode==0)==success))
        save()
        print(name, result.returncode, flush=True)
        if required:assert (result.returncode == 0) == success, log
        return result

    native, client, duplicate, api, cpp = sources
    # Freeze the production implementation independently of the optional CLI.
    snapshot=out/'source';frozen=[]
    for path in sorted((implementation/'lib/tcc').rglob('*')):
        if not path.is_file() or '__pycache__' in path.parts:continue
        target=snapshot/path.relative_to(implementation);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
        assert sha(path)==sha(target),'Source drift during copy'
        frozen.append(dict(path=path.relative_to(implementation).as_posix(),sha256=sha(target)))
    for path in sources:
        target=snapshot/path.relative_to(root);target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(path,target)
        frozen.append(dict(path=path.relative_to(root).as_posix(),sha256=sha(target)))
    report['productionSourceInputs']=frozen
    report['implementationRoot']=str(implementation)
    native,client,duplicate,api,cpp=[snapshot/p.relative_to(root)for p in sources]
    for name, source in [('native', native), ('duplicate', duplicate)]:
        run('gcc-' + name, ['gcc', '-std=c11', '-O2', '-ffunction-sections',
            '-fdata-sections', '-c', source, '-o', out / (name + '.o')])
    for suffix,defines in [('a',[]),('b',['-DCOFF_CPP_SECOND=1'])]:
        run('g++-cpp-'+suffix,['g++','-std=c++17','-O2','-fno-exceptions','-fno-rtti','-ffunction-sections','-fdata-sections',*defines,'-c',cpp,'-o',out/('cpp-'+suffix+'.o')])
    # An unselected member has a real unsatisfied native dependency.
    inactive = out / 'inactive.c'
    inactive.write_text('extern int coff_missing_member(void); int coff_inactive(void){return coff_missing_member();}\n')
    run('gcc-inactive', ['gcc', '-O2', '-c', inactive, '-o', out / 'inactive.o'])
    archive = out / 'libcoff.a'
    run('archive', ['ar', 'rcs', archive, out / 'native.o', out / 'duplicate.o',out/'cpp-a.o',out/'cpp-b.o', out / 'inactive.o'])
    expected = ['COFF_CONSTRUCTOR', 'COFF_MAIN_OK', 'COFF_DESTRUCTOR']
    gcc_program = out / 'gcc-reference.exe'
    # This PE ld keeps the dead provider's reference through its native metadata.
    # Resolve it for the correctness reference with a fail-fast cold provider;
    # the TinyCC gates below use the unchanged unresolved archive without it.
    reference_only = out / 'gcc-reference-only.c'
    reference_only.write_text('#include <stdlib.h>\nint coff_unavailable_dead_dependency(void){abort();}\n')
    run('gcc-reference-build', ['gcc', '-std=c11', '-O2', '-ffunction-sections',
        '-fdata-sections', client, archive, reference_only, '-Wl,--gc-sections', '-o', gcc_program])
    assert run('gcc-reference-run', [gcc_program]).stdout.decode().splitlines() == expected
    base = [tcc, '-B' + str(distribution), '-ffunction-sections', '-fdata-sections']
    program = out / 'tinycc.exe'
    if args.emit_pe:
        rejected=run('tcc-no-gc-rejects-dead-undefined', [*base, client, archive, '-o', out / 'no-gc.exe'], False)
        assert b'coff_unavailable_dead_dependency'in rejected.stderr
    run('tcc-client-object',[*base,'-c',client,'-o',out/'client.o'])
    if args.emit_pe:run('tcc-exe-build', [*base, '-Wl,--gc-sections', client, archive, '-o', program])
    report['peExecution']=dict(emitRequested=args.emit_pe,requested=args.execute_pe,executed=False,qualification='Not qualified by default; explicit PE emission/execution only; prior blocked artifacts stay untouched')
    if args.execute_pe:
        assert run('tcc-exe-run', [program]).stdout.decode().splitlines() == expected
        report['peExecution']['executed']=True
    definition=out/'coff-alias.def';definition.write_text('LIBRARY msvcrt.dll\nEXPORTS\ncoff_abs=labs\ncoff_huge=_HUGE DATA\n')
    for opt in ('O0','O2'):
        dest=out/opt;dest.mkdir();harness=dest/'coff-api-host.exe'
        run(opt+'-api-build',['gcc','-std=c11','-'+opt,'-DTCC_DISABLE_BUILTIN_VFS','-I'+str(snapshot/'lib/tcc'),api,snapshot/'lib/tcc/tcc_utf8_io.c','-lws2_32','-o',harness])
        text=run(opt+'-api-jit',[harness,distribution,client,archive,definition,dest],required=False).stdout.decode()
        import re
        result=re.search(r'TCC_COFF_API_CHECKS=(\d+) FAILURES=(\d+)',text)
        assert result,'API JIT host did not report a check count'
        report['rows'][-1]['checks']=int(result[1]);report['rows'][-1]['failures']=int(result[2])
        report['rows'][-1]['nativeLifecycleOutput']=[line for line in text.splitlines()if line.startswith('COFF_')]
        if not int(result[2]):assert report['rows'][-1]['nativeLifecycleOutput']==expected
    live = out / 'live.c'
    live.write_text('extern int coff_unreachable(void); int main(void){return coff_unreachable();}\n')
    if args.emit_pe:
        rejected=run('tcc-live-undefined-rejected', [*base, '-Wl,--gc-sections', live, archive,'-o',out/'live.exe'],False)
        assert b'coff_unavailable_dead_dependency'in rejected.stderr
    original = bytearray((out / 'native.o').read_bytes())
    invalid = [('short-header', original[:18]), ('short-file', original[:60])]
    changed = bytearray(original)
    struct.pack_into('<I', changed, 8, 0xffffffff)
    invalid.append(('symbol-offset-overflow', changed))
    changed = bytearray(original)
    struct.pack_into('<I', changed, 12, 0xffffffff)
    invalid.append(('symbol-count-overflow', changed))
    for name, raw in invalid:
        path = out / (name + '.o')
        path.write_bytes(raw)
        result = run('reject-' + name, [*base, client, path, '-o', out / (name + '.exe')], False)
        assert result.returncode == 1, 'Invalid object caused a crash: ' + name
    if args.emit_pe:
        headers = run('image-sections', ['objdump', '-h', program])
        imports = run('image-imports', ['objdump', '-p', program])
        assert b'xge3_host' not in imports.stdout and b'xge.dll' not in imports.stdout
    if args.build_jit_provider:
        provider=out/'libtcc-coff-jit.dll'
        run('jit-provider-build',['gcc','-O2','-shared','-DONE_SOURCE','-DLIBTCC_AS_DLL','-DTCC_DISABLE_BUILTIN_VFS','-I'+str(snapshot/'lib/tcc'),snapshot/'lib/tcc/libtcc.c',snapshot/'lib/tcc/tcc_utf8_io.c','-lws2_32','-Wl,--out-implib,'+str(out/'libtcc-coff-jit.a'),'-o',provider])
        exports=run('jit-provider-symbols',['nm','-g','--defined-only',provider]).stdout.decode()
        assert 'tcc_new'in exports and 'tcc_relocate'in exports and 'tcc_delete'in exports
        assert not re.search(r'\b(?:xrt[A-Z]|xl6|xl[A-Z])\w*\s*$',exports,re.M),'Unexpected resident runtime in Only-TCC provider'
        report['jitProvider']=dict(path=str(provider),sha256=sha(provider),bytes=provider.stat().st_size,profile='GCC O2 isolated JIT-only, no built-in VFS or embedded resource; not a release provider',sourceManifest='source-freeze.json')
    report['frozenInputsUnchanged']=all(sha(snapshot/v['path'])==v['sha256']for v in frozen)
    report['sourceStillMatchesWorkspace']=all(sha(root/v['path'])==v['sha256']for v in frozen)
    report['jitQualification']='Direct ONE_SOURCE production libtcc API in GCC-built host; separate client then archive; explicit init/fini contract.'
    report.update(complete=True, passed=all(v.get('passed',False)for v in report['rows'])and report['frozenInputsUnchanged']and all(sha(p) == value for p, value in report['inputs'].items()),
        programSha256=sha(program)if args.emit_pe else None, programBytes=program.stat().st_size if args.emit_pe else None,
        referenceBytes=gcc_program.stat().st_size, nativeArchiveSha256=sha(archive))
    (out/'source-freeze.json').write_text(json.dumps(dict(scope='Exact libtcc/COFF qualifier source snapshot; PE TLS successor may be separate.',inputs=frozen,allFrozenInputsUnchanged=report['frozenInputsUnchanged']),indent=2)+'\n')
    report['sourceFreezeSha256']=sha(out/'source-freeze.json')
    report['qualificationScope']='GCC-built CPU API JIT + true GCC COFF archives + invalid objects/import metadata; PE emit/inspection/execution only when explicitly requested'
    save()
    assert report['passed']


if __name__ == '__main__':
    main()
