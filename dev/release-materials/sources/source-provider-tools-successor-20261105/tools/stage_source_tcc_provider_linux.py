"""Stage a Linux source compiler with an independently built matching TinyCC provider.

An explicit compiler overlay is verified against its complete source
parent; remaining compiler objects are reused. The driver is relinked to the new provider. A fresh
producer tree receives explicit native TCC configuration and platform-neutral
public SDK overlays; the frozen source cohort and shipping release are never
modified. Full mode creates GCC -O2 precompiled objects and matching runtime/
compiler VFS catalogs. JIT mode uses an empty catalog and external SDK and
does not qualify embedded-VFS, standalone AOT or nested compilation.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,re,shutil,subprocess,sys,time

def refresh_sdk_headers(root,base,build,overlay,destination,sha,inputs):
    """Recreate every recorded alias from authoritative successor sources."""
    original=next(Path(row['Source']).parents[2] for row in build['Objects']
        if Path(row['Source']).name=='xlir_public_source_linux_driver.c')
    old_headers=Path(overlay['RuntimeHeaders'])
    records=copy.deepcopy(overlay.get('Headers',build['Headers']));families={};recorded=set()
    for row in records:
        path=Path(row['Path']);source=Path(row['Source'])
        relative=next((path.relative_to(folder) for folder in (old_headers,base/'include') if path.is_relative_to(folder)),None)
        assert relative is not None,path
        if source.is_relative_to(original):source=root/source.relative_to(original)
        assert source.is_relative_to(root) and source.is_file(),source
        target=destination/relative;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,target)
        row.update(Source=str(source),Path=str(target),Sha256=sha(source));inputs[str(source)]=sha(source)
        recorded.add(target);families.setdefault((source.parent,relative.parent),set()).add(source.suffix)
    for (folder,relative),suffixes in families.items():
        for source in sorted(folder.iterdir()):
            target=destination/relative/source.name
            if not source.is_file() or source.suffix not in suffixes or target in recorded:continue
            shutil.copy2(source,target);inputs[str(source)]=sha(source);recorded.add(target)
            records.append(dict(Source=str(source),Path=str(target),Sha256=sha(source)))
    return records

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','base','native-root','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--compiler-overlay',type=Path,required=True)
    p.add_argument('--inherit-precompiled',type=Path,required=True)
    p.add_argument('--mode',choices=('jit','full'),default='full')
    p.add_argument('--reuse-provider-stage',type=Path,help='Reuse an interrupted stage whose precompile/catalog/provider steps all completed; keep its report unchanged')
    p.add_argument('--prior-tool',type=Path,help='Archived exact tool input required when resuming a prior stage')
    a=p.parse_args();root,base,native,out=[v.resolve() for v in (a.root,a.base,a.native_root,a.out)]
    assert os.name!='nt';sys.path.insert(0,str(root/'tools'))
    from foreign_validation_build import load_build
    selected=load_build(None,a.compiler_overlay)
    assert selected['cohort']==root
    build=selected['base']; overlay=json.loads(a.compiler_overlay.read_text())
    pinned=set(map(Path,selected['inputs']));pinned.add(a.compiler_overlay.resolve())
    pinned.update(path for path in root.rglob('*') if path.is_file())
    cc=shutil.which('gcc');nm=shutil.which('nm');assert cc and nm
    assert not out.exists() and not out.is_relative_to(root/'release')
    out.mkdir(parents=True);producer=out/'producer';sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    configuration=native/'build/linux/tcc-source/config.h';predefs=native/'build/linux/tcc-source/tccdefs_.h'
    distribution=native/'build/linux/tcc-dist';sdk=Path(overlay['SdkDirectory'])
    pinned.update(path for path in sdk.rglob('*') if path.is_file())
    pinned.update(path for path in a.inherit_precompiled.rglob('*') if path.is_file())
    pinned.update([Path(__file__).resolve(),Path(cc),Path(nm),configuration,predefs,
        *[path for path in distribution.rglob('*') if path.is_file()]])
    inputs={str(path):sha(path) for path in pinned}
    report=dict(scope=__doc__,mode=a.mode,complete=False,passed=False,inputs=inputs,rows=[],
        parentSourceBuild=str(selected['basePath']),parentDriverSha256=overlay['DriverSha256'],overlays=[],
        compilerOverlay=str(a.compiler_overlay.resolve()),fullCompilerRebuild=False,
        cacheScope='Normal precompile_builtin keys; inherited cache does not bypass conservative global header invalidation')
    def save():(out/'provider.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,cwd=None,env=None,timeout=7200):
        start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,timeout=timeout)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log;return r.stdout
    if a.reuse_provider_stage:
        previous=a.reuse_provider_stage.resolve();prior=json.loads((previous/'provider.json').read_text())
        assert a.mode=='full' and prior['mode']=='full' and not prior['complete']
        assert [row['name'] for row in prior['rows']]==['tcc-producer-build','precompile','packer','runtime-resources','xlc-resources','provider-build','provider-symbols']
        assert all(row['exitCode']==0 for row in prior['rows'])
        assert a.prior_tool and a.prior_tool.is_file()
        archived_sha=sha(a.prior_tool);assert archived_sha in prior['inputs'].values()
        for path,digest in prior['inputs'].items():
            if Path(path).resolve()==Path(__file__).resolve():
                assert digest==archived_sha
            else:assert sha(path)==digest,path
        for row in prior['rows']:assert sha(previous/(row['name']+'.log'))==row['logSha256']
        producer=Path(prior['producer']);precompiled=previous/'precompiled'
        sys.path.insert(0,str(root/'tools'));from precompile_builtin import source_fingerprint
        manifest=json.loads((precompiled/'manifest.json').read_text())
        assert manifest['sources']==source_fingerprint(producer)
        for row in manifest['objects']:assert sha(row['source'])==row['sha256']
        (out/'build/linux').mkdir(parents=True);dist=out/'build/linux/tcc-dist'
        shutil.copytree(previous/'build/linux/tcc-dist',dist)
        for name in ('tcc_builtin_resources_runtime.c','tcc_builtin_resources.c'):
            resource=previous/name;stamp=json.loads(Path(str(resource)+'.stamp.json').read_text())
            assert sha(resource)==stamp['generatedSha256']
            for row in stamp['resources']:assert sha(row['source'])==row['sha256']
        provider=out/'release/linux_x64/libxlc.so';provider.parent.mkdir(parents=True)
        shutil.copy2(previous/'release/linux_x64/libxlc.so',provider)
        shutil.copy2(previous/'precompile.log',out/'precompile.log')
        for path in [a.prior_tool,previous/'provider.json',precompiled/'manifest.json',previous/'release/linux_x64/libxlc.so']:
            inputs[str(path)]=sha(path)
        report.update(inheritedProviderStage=str(previous),inheritedProviderStageSha256=sha(previous/'provider.json'),
            inheritedCompletedRows=prior['rows'],producer=str(producer),producerSdk=prior['producerSdk'],
            precompiledManifest=str(precompiled/'manifest.json'),precompiledManifestSha256=sha(precompiled/'manifest.json'),
            nativeConfigurationSha256=prior['nativeConfigurationSha256'],triplet=prior['triplet'])
        exports=run('provider-symbols',[nm,'-D','--defined-only',provider]).decode().splitlines()
        names={line.split()[-1] for line in exports if line.split()}
        assert 'tcc_new' in names and not any(n.startswith(('xrt','xl6','xlcCreate','xlCreate')) for n in names)
    else:
        # Producer configuration changes are explicit and never applied to root.
        def link_copy(original,target):
            try:os.link(original,target)
            except OSError:shutil.copy2(original,target)
        shutil.copytree(root,producer,copy_function=link_copy)
        for original,target in ((configuration,producer/'lib/tcc/config.h'),(predefs,producer/'lib/tcc/tccdefs_.h')):
            before=sha(target) if target.exists() else None
            target.unlink(missing_ok=True);shutil.copy2(original,target)
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
            shutil.copytree(a.inherit_precompiled,precompiled)
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
        (out/'release/linux_x64').mkdir(parents=True);provider=out/'release/linux_x64/libxlc.so'
        run('provider-build',[cc,*common,'-fPIC','-shared','-Wl,-Bsymbolic','-DLIBTCC_AS_DLL',
            source/'libtcc.c',source/'tcctools.c',source/'tcc_builtin_vfs.c',lzma/'LzmaDec.c',catalog,'-ldl','-lm','-o',provider])
        exports=run('provider-symbols',[nm,'-D','--defined-only',provider]).decode().splitlines()
        names={line.split()[-1] for line in exports if line.split()}
        assert 'tcc_new' in names and not any(n.startswith(('xrt','xl6','xlcCreate','xlCreate')) for n in names)
    shutil.copytree(Path(overlay['RuntimeHeaders']),out/'include')
    driver=out/'public_source'
    replacements={row['ParentObject']:row['Path'] for row in overlay['Objects']}
    linked=[replacements.get(row['Path'],row['Path']) for row in build['Objects']]
    linked.extend(row['Path'] for row in overlay.get('ExtraObjects',[]))
    # Only the test driver changes its NativeRoot provider location to the
    # current platform directory. Core objects and the frozen source stay put.
    entry=next(row for row in build['Objects'] if Path(row['Source']).name=='xlir_public_source_linux_driver.c')
    entry_source=root/'test/cases/xlir_public_source_linux_driver.c'
    derived=out/entry_source.name
    text=entry_source.read_text();assert '%s/release/libxlc.so' in text
    derived.write_text(text.replace('%s/release/libxlc.so','%s/release/linux_x64/libxlc.so'))
    if 'Flags' in entry:flags=entry['Flags']
    else:
        row=next(row for row in build['Rows'] if '-c' in row.get('Command',[]) and entry['Source'] in row['Command'])
        flags=row['Command'][1:row['Command'].index('-c')]
    old_root=Path(entry['Source']).parents[2]
    flags=[v.replace(str(old_root),str(root)) for v in flags]
    entry_object=out/'source_driver.o'
    run('platform-driver',[cc,*flags,'-c',derived,'-o',entry_object],root)
    linked=[str(entry_object) if v==replacements.get(entry['Path'],entry['Path']) else v for v in linked]
    run('driver-relink',[cc,*linked,'-L'+str(provider.parent),'-lxlc',
        '-Wl,-rpath,'+str(provider.parent),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',driver])
    staged=copy.deepcopy(overlay)
    previous=next((r for r in staged['Objects'] if r['ParentObject']==entry['Path']),None)
    if previous:staged['Objects'].remove(previous)
    staged['Objects'].append(dict(Source=str(derived),Path=str(entry_object),ParentObject=entry['Path'],Sha256=sha(entry_object),Flags=flags))
    staged.update(Driver=str(driver),DriverSha256=sha(driver),TccCore=str(provider),TccCoreSha256=sha(provider),
        RuntimeLibrary=str(provider),NativeRoot=str(out),ParentSourceBuild=str(base/'build.json'),
        ParentSourceBuildSha256=sha(base/'build.json'),TccProviderRole='Only TinyCC and '+a.mode+' VFS; no resident xlang/XRT implementation',
        ProviderStageScope=__doc__,ProviderStageMode=a.mode)
    staged['Headers']=refresh_sdk_headers(root,base,build,overlay,out/'include',sha,inputs)
    staged['ProviderInputs']=[dict(Path=path,Sha256=value) for path,value in inputs.items()]
    staged['fullRebuild']=False
    staged['providerSerializedObjectCache']='Normal inherited cache; inspect precompile.log for actual reuse'
    staged['includeOverlayRoot']=str(out/'include');staged['RuntimeHeaders']=str(out/'include')
    (out/'overlay.json').write_text(json.dumps(staged,indent=2)+'\n')
    (out/'build.json').write_text(json.dumps(staged,indent=2)+'\n')
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(out),LD_LIBRARY_PATH=str(provider.parent)+':'+os.environ.get('LD_LIBRARY_PATH',''))
    run('source-driver-jit',[driver,root/'test/cases/cgen_basic.xl','run',out/'smoke-jit'],root,env)
    if a.mode=='full':
        run('source-driver-aot',[driver,root/'test/cases/cgen_basic.xl','exe',out/'smoke-aot'],root,env)
        run('source-aot-run',[out/'smoke-aot'],root,env)
    if a.mode=='full':
        precompile_log=(out/'precompile.log').read_text()
        counts=re.search(r'precompiled (\d+) objects \((\d+) unchanged\)',precompile_log)
        assert counts
        report['precompiledObjectCount']=int(counts.group(1));report['precompiledObjectsReused']=int(counts.group(2))
        report['precompiledObjectsRebuilt']=int(counts.group(1))-int(counts.group(2))
    report.update(complete=True,passed=all(sha(path)==value for path,value in inputs.items()),
        provider=str(provider),providerSha256=sha(provider),sourceCohort=str(out/'build.json'),
        compilerObjectsRebuilt=0,testDriverObjectsRebuilt=1,driverRelinked=True,shippingAcceptance=False,
        standaloneSmokePassed=a.mode=='full',nestedCompilationAccepted=False)
    save();assert report['passed']
if __name__=='__main__':main()
