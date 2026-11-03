"""Stage a verified source compiler with a current, independently built TinyCC VFS.

Compiler/runtime objects and driver bytes are reused unchanged from a completed
source cohort. Only the dynamically loaded TinyCC provider changes; this DLL
contains no resident xlang/XRT machine-code implementation (target objects
remain serialized VFS resources). This is isolated acceptance material,
not a replacement of release products or a new full compiler compilation.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,shutil,subprocess,sys,time
from compiler_response import command_with_response

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('base','precompiled','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[1]
    base,precompiled,out=[v.resolve() for v in (a.base,a.precompiled,a.out)]
    assert os.name=='nt';cc=shutil.which('gcc');nm=shutil.which('nm');assert cc and nm
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    original=json.loads((base/'build.json').read_text(encoding='utf-8-sig'))
    assert original['Complete'] and original['InputsUnchanged'] and original['SourceXrt'] and not original['RuntimeOnly']
    for row in original['Inputs']+original['Headers']+original['Objects']:assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']
    assert sha(original['Driver']).lower()==original['DriverSha256'].lower()
    manifest=json.loads((precompiled/'manifest.json').read_text())
    assert manifest['target']=='win64'
    out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    files=[Path(__file__).resolve(),root/'tools/compiler_response.py',base/'build.json',precompiled/'manifest.json',
        root/'tools/gen_tcc_vfs_resources.py',root/'tools/precompile_builtin.py',root/'tools/runtime_layout.py',Path(cc),Path(nm)]
    files += [Path(row['source']) for row in manifest['objects']]
    inputs={str(path):sha(path) for path in files}
    report=dict(complete=False,passed=False,scope=__doc__,sourceBuild=str(base/'build.json'),
        sourceBuildSha256=sha(base/'build.json'),precompiledManifest=str(precompiled/'manifest.json'),inputs=inputs,rows=[])
    env=dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'))
    def save(): (out/'provider.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        actual=command_with_response(command,cc,out/(name+'.rsp'))
        start=time.monotonic();r=subprocess.run(actual,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),actualCommand=actual,exitCode=r.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert r.returncode==0,log;return r.stdout
    packer=out/'pack.exe'
    run('packer',[cc,'-std=c11','-O2','-DZ7_ST','-Ilib/lzma','tools/tcc_vfs_lzma_pack.c',
        'lib/lzma/LzmaEnc.c','lib/lzma/LzFind.c','lib/lzma/CpuArch.c','-o',packer])
    common=[sys.executable,root/'tools/gen_tcc_vfs_resources.py','--root',root,
        '--tcc-dist',root/'res/tcc/win64/x86_64','--xrt-obj',precompiled/'static-native.o',
        '--pack-tool',packer,'--precompiled',precompiled/'manifest.json','--packing-order','family']
    runtime_resource=out/'tcc_builtin_resources_runtime.c'
    compiler_resource=out/'tcc_builtin_resources.c'
    run('runtime-resource',[*common,'--manifest','tools/tcc_vfs_manifest_win64_runtime.txt','--output',runtime_resource])
    run('compiler-resource',[*common,'--manifest','tools/tcc_vfs_manifest_win64_xlc.txt','--output',compiler_resource])
    provider=out/'xlc.dll'
    run('provider-build',[cc,'-std=c11','-O2','-shared','-DLIBTCC_AS_DLL','-DCONFIG_TCC_STATIC',
        '-D_CRT_SECURE_NO_WARNINGS','-Ilib/tcc','-Ilib/lzma','lib/tcc/libtcc.c','lib/tcc/tcctools.c',
        'lib/tcc/tcc_builtin_vfs.c','lib/tcc/tcc_utf8_io.c','lib/lzma/LzmaDec.c',compiler_resource,
        '-lws2_32','-o',provider])
    symbols=run('provider-symbols',[nm,'-g','--defined-only',provider]).decode(errors='replace').splitlines()
    exported={line.split()[-1] for line in symbols if line.split()}
    assert 'tcc_new' in exported and not any(name.startswith(('xrt','xl6','xlcCreate','xlCreate')) for name in exported)
    shutil.copy2(base/'public_source.exe',out/'public_source.exe');shutil.copy2(base/'xl.dll',out/'xl.dll')
    shutil.copytree(base/'include',out/'include')
    staged=copy.deepcopy(original)
    staged.update(Driver=str(out/'public_source.exe'),DriverSha256=sha(out/'public_source.exe'),
        TccCore=str(provider),TccCoreSha256=sha(provider),
        ParentSourceBuild=str(base/'build.json'),ParentSourceBuildSha256=sha(base/'build.json'),
        TccProviderRole='TinyCC and current VFS only; target objects are resources, no resident xlang/XRT machine code',
        Scope=__doc__)
    for row in staged['Headers']:
        path=Path(row['Path'])
        if path.is_relative_to(base/'include'):
            row['Path']=str(out/'include'/path.relative_to(base/'include'))
            assert sha(row['Path']).lower()==row['Sha256'].lower()
    staged['Inputs'] += [dict(Path=path,Sha256=value) for path,value in inputs.items()]
    (out/'build.json').write_text(json.dumps(staged,indent=2)+'\n')
    run('source-driver-aot',[out/'public_source.exe',root/'test/cases/cgen_basic.xl','exe',out/'smoke'])
    run('source-aot-run',[out/'smoke.exe'])
    report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
    report['parentCohortUnchanged']=all(sha(row['Path']).lower()==row['Sha256'].lower()
        for row in original['Inputs']+original['Headers']+original['Objects'])
    report['driverUnchanged']=sha(out/'public_source.exe').lower()==original['DriverSha256'].lower()
    report['providerSha256']=sha(provider);report['sourceCohort']=str(out/'build.json')
    report['complete']=True;report['passed']=report['inputsUnchanged'] and report['driverUnchanged'] and report['parentCohortUnchanged'];save()
    assert report['passed']
if __name__=='__main__':main()
