"""Stage a Linux source compiler with an independently built matching TinyCC provider.

The parent complete source cohort is verified and its compiler/runtime objects
are reused unchanged. The driver is relinked to the new provider. A fresh
producer tree receives explicit native TCC configuration and platform-neutral
public SDK overlays; the frozen source cohort and shipping release are never
modified. Full mode creates GCC -O2 precompiled objects and matching runtime/
compiler VFS catalogs. JIT mode uses an empty catalog and external SDK and
does not qualify embedded-VFS, standalone AOT or nested compilation.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,re,shutil,subprocess,sys,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','base','native-root','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--mode',choices=('jit','full'),default='full')
    a=p.parse_args();root,base,native,out=[v.resolve() for v in (a.root,a.base,a.native_root,a.out)]
    assert os.name!='nt';sys.path.insert(0,str(Path(__file__).resolve().parent))
    from xlir_public_build_inputs import verify_public_build
    build,pinned,_=verify_public_build(root,base);cc=shutil.which('gcc');nm=shutil.which('nm');assert cc and nm
    assert not out.exists() and not out.is_relative_to(root/'release')
    out.mkdir(parents=True);producer=out/'producer';sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    configuration=native/'build/linux/tcc-source/config.h';predefs=native/'build/linux/tcc-source/tccdefs_.h'
    distribution=native/'build/linux/tcc-dist';sdk=Path(build['SdkDirectory'])
    public_headers=[path for path in (sdk/'include').rglob('*') if path.is_file() and path.suffix in ('.h','.inc','.def')]
    pinned.update([Path(__file__).resolve(),Path(cc),Path(nm),configuration,predefs,
        *public_headers,*[path for path in distribution.rglob('*') if path.is_file()]])
    inputs={str(path):sha(path) for path in pinned}
    report=dict(scope=__doc__,mode=a.mode,complete=False,passed=False,inputs=inputs,rows=[],
        parentSourceBuild=str(base/'build.json'),parentDriverSha256=build['DriverSha256'],overlays=[])
    def save():(out/'provider.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,cwd=None,env=None,timeout=7200):
        start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,timeout=timeout)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log;return r.stdout
    # Producer configuration changes are explicit and never applied to root.
    shutil.copytree(root,producer)
    for original,target in ((configuration,producer/'lib/tcc/config.h'),(predefs,producer/'lib/tcc/tccdefs_.h')):
        before=sha(target) if target.exists() else None;shutil.copy2(original,target)
        report['overlays'].append(dict(source=str(original),target=str(target.relative_to(producer)),beforeSha256=before,sha256=sha(target)))
    config=producer/'lib/tcc/config.h'
    if '#define CONFIG_TCC_BUILTIN_VFS' not in config.read_text():
        with config.open('a') as f:f.write('\n#define CONFIG_TCC_BUILTIN_VFS 1\n')
    report['nativeConfigurationSha256']=sha(config)
    triplet=re.search(r'^#define\s+CONFIG_TRIPLET\s+"([A-Za-z0-9_.+\-]+)"',config.read_text(),re.M)
    assert triplet;triplet=triplet.group(1);report['triplet']=triplet
    linux_sdk=producer/'release/linux_x64'
    if linux_sdk.exists():
        # Preserve inherited bytes, but never let a stale platform SDK win
        # over the explicitly selected source-cohort declarations/providers.
        linux_sdk.rename(out/'inherited-linux-sdk')
    shutil.copytree(sdk,linux_sdk)
    report['overlays'].append(dict(source=str(sdk),target='release/linux_x64',
        role='Explicit matched declarations/providers, replacing inherited SDK selection'))
    (out/'build/linux').mkdir(parents=True);dist=out/'build/linux/tcc-dist';shutil.copytree(distribution,dist)
    report['producerSdk']=str(linux_sdk);report['producer']=str(producer);save()
    source=producer/'lib/tcc';lzma=producer/'lib/lzma'
    common=['-std=c11','-O2','-I'+str(source),'-I'+str(lzma)]
    empty=producer/'tools/tcc_empty_builtin_resources.c';catalog=empty
    if a.mode=='full':
        tcc=out/'tcc-producer'
        run('tcc-producer-build',[cc,*common,source/'tcc.c',source/'tcc_builtin_vfs.c',lzma/'LzmaDec.c',empty,'-ldl','-lm','-o',tcc])
        precompiled=out/'precompiled'
        run('precompile',[sys.executable,producer/'tools/precompile_builtin.py','--root',producer,'--tcc',tcc,
            '--tcc-dist',dist,'--native-cc',cc,'--triplet',triplet,'--output',precompiled],producer)
        manifest=json.loads((precompiled/'manifest.json').read_text());assert manifest['target']=='linux-x86_64'
        packer=out/'lzma-pack'
        run('packer',[cc,'-std=c11','-O2','-DZ7_ST','-I'+str(lzma),producer/'tools/tcc_vfs_lzma_pack.c',
            lzma/'LzmaEnc.c',lzma/'LzFind.c',lzma/'CpuArch.c','-o',packer])
        pack=[sys.executable,producer/'tools/gen_tcc_vfs_resources.py','--root',producer,'--tcc-dist',dist,
            '--xrt-obj',precompiled/'static-native.o','--precompiled',precompiled/'manifest.json','--pack-tool',packer,'--packing-order','family']
        for family in ('runtime','xlc'):
            destination=out/('tcc_builtin_resources_runtime.c' if family=='runtime' else 'tcc_builtin_resources.c')
            run(family+'-resources',[*pack,'--manifest','tools/tcc_vfs_manifest_linux_'+family+'.txt','--output',destination],producer)
        catalog=out/'tcc_builtin_resources.c';report['precompiledManifest']=str(precompiled/'manifest.json')
        report['precompiledManifestSha256']=sha(precompiled/'manifest.json')
    (out/'release').mkdir();provider=out/'release/libxlc.so'
    run('provider-build',[cc,*common,'-fPIC','-shared','-Wl,-Bsymbolic','-DLIBTCC_AS_DLL',
        source/'libtcc.c',source/'tcctools.c',source/'tcc_builtin_vfs.c',lzma/'LzmaDec.c',catalog,'-ldl','-lm','-o',provider])
    exports=run('provider-symbols',[nm,'-D','--defined-only',provider]).decode().splitlines()
    names={line.split()[-1] for line in exports if line.split()}
    assert 'tcc_new' in names and not any(n.startswith(('xrt','xl6','xlcCreate','xlCreate')) for n in names)
    shutil.copytree(base/'include',out/'include')
    added_headers=[]
    for source_header in public_headers:
        target=out/'include'/source_header.relative_to(sdk/'include');target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists():assert sha(target)==sha(source_header),target
        else:
            shutil.copy2(source_header,target)
            added_headers.append(dict(Path=str(target),Source=str(source_header),Sha256=sha(target)))
    driver=out/'public_source'
    run('driver-relink',[cc,*[row['Path'] for row in build['Objects']],'-L'+str(provider.parent),'-lxlc',
        '-Wl,-rpath,'+str(provider.parent),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',driver])
    staged=copy.deepcopy(build)
    staged.update(Driver=str(driver),DriverSha256=sha(driver),TccCore=str(provider),TccCoreSha256=sha(provider),
        RuntimeLibrary=str(provider),NativeRoot=str(out),ParentSourceBuild=str(base/'build.json'),
        ParentSourceBuildSha256=sha(base/'build.json'),TccProviderRole='Only TinyCC and '+a.mode+' VFS; no resident xlang/XRT implementation',
        ProviderStageScope=__doc__,ProviderStageMode=a.mode)
    for row in staged['Headers']:
        path=Path(row['Path'])
        if path.is_relative_to(base/'include'):row['Path']=str(out/'include'/path.relative_to(base/'include'))
    staged['Headers']+=added_headers
    staged['Inputs'] += [dict(Path=path,Sha256=value) for path,value in inputs.items() if path not in {row['Path'] for row in staged['Inputs']}]
    (out/'build.json').write_text(json.dumps(staged,indent=2)+'\n')
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(out),LD_LIBRARY_PATH=str(provider.parent)+':'+os.environ.get('LD_LIBRARY_PATH',''))
    run('source-driver-jit',[driver,root/'test/cases/cgen_basic.xl','run',out/'smoke-jit'],root,env)
    if a.mode=='full':
        run('source-driver-aot',[driver,root/'test/cases/cgen_basic.xl','exe',out/'smoke-aot'],root,env)
        run('source-aot-run',[out/'smoke-aot'],root,env)
    report.update(complete=True,passed=all(sha(path)==value for path,value in inputs.items()),
        provider=str(provider),providerSha256=sha(provider),sourceCohort=str(out/'build.json'),
        compilerObjectsRebuilt=False,driverRelinked=True,shippingAcceptance=False,
        standaloneSmokePassed=a.mode=='full',nestedCompilationAccepted=False)
    save();assert report['passed']
if __name__=='__main__':main()
