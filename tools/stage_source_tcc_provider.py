"""Stage a verified source compiler with a current, independently built TinyCC VFS.

The original full-build invocation is retained. Explicit --overlay verifies its
completed parent and actual replacement objects without claiming a new full
compiler build. Resident-runtime bytes stay unchanged; an explicit matched-link
mode relinks the stock driver without recompiling any objects. This DLL
contains no resident xlang/XRT machine-code implementation (target objects
remain serialized VFS resources). This is isolated acceptance material,
not a replacement of release products or a new full compiler compilation.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,shutil,subprocess,sys,time
import re
from compiler_response import command_with_response
from foreign_validation_build import load_build
from precompile_builtin import source_fingerprint
from runtime_layout import source_directories


def source_identity(root,cohort,sha):
    """An explicit producer may update SDK aliases, never compiler sources."""
    result={}
    for folder in ('src','lib/tcc','lib/lzma','lib/xrt','lib/xruntime'):
        trees=[{path.relative_to(selected).as_posix():sha(path)
            for path in (selected/folder).rglob('*') if path.is_file()
            and path.suffix in ('.c','.h','.inc','.def')} for selected in (root,cohort)]
        assert trees[0]==trees[1],folder+' differs from the selected compiler cohort'
        result.update(trees[0])
    return result


def pe_tcc_imports(text):
    """Inspect actual PE import ownership; do not assume linker search order."""
    current=None;owners={}
    for line in text.splitlines():
        module=re.search(r'DLL Name:\s*(\S+)',line)
        if module:current=module.group(1).lower();continue
        imported=re.match(r'^\s*[0-9a-fA-F]+\s+(?:<none>\s+)?[0-9a-fA-F]+\s+([_A-Za-z][_A-Za-z0-9@$?.]*)\s*$',line)
        if current and imported and re.match(r'^_?tcc_',imported.group(1)):
            name=imported.group(1);assert name not in owners,name
            owners[name]=current
    return owners

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('base','precompiled','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--overlay',type=Path,help='Explicit partial compiler overlay over --base')
    p.add_argument('--root',type=Path,help='Frozen source or matching producer tree for VFS inputs')
    p.add_argument('--producer-tool',type=Path,help='Actual TinyCC executable used by the precompiled manifest')
    p.add_argument('--skip-pe-smoke',action='store_true',help='Use JIT smoke only; emit/run no TinyCC PE and leave PE/AOT unqualified')
    p.add_argument('--match-provider-imports',action='store_true',
        help='Relink the stock adjacent-xlc.dll SourceDriver from verified objects and require matching TCC PE imports')
    a=p.parse_args()
    base,precompiled,out=[v.resolve() for v in (a.base,a.precompiled,a.out)]
    assert os.name=='nt';cc=shutil.which('gcc');nm=shutil.which('nm');assert cc and nm
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    original=json.loads((base/'build.json').read_text(encoding='utf-8-sig'))
    assert original['Complete'] and original['InputsUnchanged'] and original['SourceXrt'] and not original['RuntimeOnly']
    for row in original['Inputs']+original['Headers']+original['Objects']:assert sha(row['Path']).lower()==row['Sha256'].lower(),row['Path']
    assert sha(original['Driver']).lower()==original['DriverSha256'].lower()
    parent=load_build(base/'build.json',None)
    selected=load_build(None,a.overlay) if a.overlay else parent
    description=json.loads(a.overlay.read_text(encoding='utf-8-sig')) if a.overlay else original
    if a.overlay:
        assert selected['basePath'].resolve()==(base/'build.json').resolve()
        assert 'Complete' not in description and 'InputsUnchanged' not in description,'Overlay cannot claim a complete compiler rebuild'
        known={row['Path'] for row in original['Objects']}
        assert all(row['ParentObject'] in known for row in description['Objects'])
        assert selected['runtime'].name.lower()!='xlc.dll','Resident runtime must remain distinct from Only-TCC'
    root=(a.root or (selected['cohort'] if a.overlay else Path(__file__).resolve().parents[1])).resolve()
    assert not out.is_relative_to(root/'release')
    identity=source_identity(root,selected['cohort'],sha) if a.overlay else {}
    driver_rows=[row for row in original['Objects'] if Path(row['Source']).name=='xlir_public_source_driver.c']
    if a.match_provider_imports:
        assert len(driver_rows)==1,'Matched import mode is limited to the stock Windows SourceDriver'
        actual_driver=next((row for row in description.get('Objects',[])if row.get('ParentObject')==driver_rows[0]['Path']),driver_rows[0])
        driver_source=Path(actual_driver['Source'])
        expected_driver=selected['cohort']/'test/cases/xlir_public_source_driver.c'
        assert driver_source.name==expected_driver.name
        assert driver_source.read_bytes().replace(b'\r\n',b'\n')==expected_driver.read_bytes().replace(b'\r\n',b'\n'),'Stock driver source differs beyond canonical LF'
    manifest=json.loads((precompiled/'manifest.json').read_text())
    assert manifest['target']=='win64'
    assert manifest['sources']==source_fingerprint(root),'Precompiled objects mismatch VFS source inputs'
    for row in manifest['objects']:assert sha(row['source'])==row['sha256'],row['source']
    if a.producer_tool:assert sha(a.producer_tool)==manifest['tcc'],'Producer executable mismatch'
    out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    files=[Path(__file__).resolve(),root/'tools/compiler_response.py',base/'build.json',precompiled/'manifest.json',
        root/'tools/gen_tcc_vfs_resources.py',root/'tools/precompile_builtin.py',root/'tools/runtime_layout.py',Path(cc),Path(nm)]
    files += [Path(row['source']) for row in manifest['objects']]
    files += [Path(__file__).with_name('foreign_validation_build.py')]
    files += [Path(row['Source']) for row in description.get('Objects',[])]
    files += [root/name for name in identity]
    files += [path for folder in source_directories(root) for path in folder.rglob('*')
        if path.is_file() and path.suffix in ('.c','.h','.inc','.def')]
    files += [path for path in (root/'res/tcc/win64/x86_64').rglob('*') if path.is_file()]
    files += [root/'tools/tcc_vfs_manifest_win64_runtime.txt',root/'tools/tcc_vfs_manifest_win64_xlc.txt']
    if a.overlay:files.append(a.overlay.resolve())
    if a.producer_tool:files.append(a.producer_tool.resolve())
    if a.match_provider_imports:files.extend((driver_source,expected_driver))
    inputs=dict(parent['inputs']);inputs.update(selected['inputs'])
    inputs.update((str(path),sha(path)) for path in files)
    report=dict(complete=False,passed=False,scope=__doc__,sourceBuild=str(base/'build.json'),
        sourceBuildSha256=sha(base/'build.json'),precompiledManifest=str(precompiled/'manifest.json'),inputs=inputs,rows=[],
        compilerOverlay=str(a.overlay.resolve()) if a.overlay else None,fullSourceCompilerParent=selected['fullSourceRebuild'],
        compilerObjectsRebuiltByThisTool=False,sourceRoot=str(selected['cohort']),vfsProducerRoot=str(root),
        precompiledObjectCount=len(manifest['objects']),peSmokeSkipped=a.skip_pe_smoke,peAotQualified=False,shippingDelivery=False)
    if a.skip_pe_smoke:report['scope']+=' This invocation qualifies provider/VFS and JIT only; PE/AOT emission and execution are explicitly skipped.'
    if a.producer_tool:report['precompilerTool']=dict(path=str(a.producer_tool.resolve()),sha256=sha(a.producer_tool),reused=True)
    env=dict(os.environ,TEMP=str(out/'tmp'),TMP=str(out/'tmp'),PYTHONDONTWRITEBYTECODE='1')
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
    shutil.copy2(selected['runtime'],out/selected['runtime'].name)
    if a.match_provider_imports:
        replacements={row['ParentObject']:row for row in description.get('Objects',[])} if a.overlay else {}
        linked=[replacements.get(row['Path'],row)['Path'] for row in original['Objects']]
        linked.extend(row['Path']for row in description.get('ExtraObjects',[]))
        assert len(linked)==len(set(linked))
        # Direct DLL inputs are last-wins in the observed MinGW linker. The
        # resulting IAT below is authoritative; order alone is not the proof.
        run('driver-provider-relink',[cc,*linked,str(out/selected['runtime'].name),str(provider),
            '-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32','-o',out/'public_source.exe'])
    else:shutil.copy2(selected['driver'],out/'public_source.exe')
    objdump=shutil.which('objdump');assert objdump
    inputs[str(Path(objdump))]=sha(objdump)
    imports=run('driver-provider-imports',[objdump,'-p',out/'public_source.exe']).decode(errors='replace')
    tcc_owners=pe_tcc_imports(imports)
    matched=bool(tcc_owners)and tcc_owners.get('tcc_delete')==provider.name.lower()and all(name==provider.name.lower()for name in tcc_owners.values())
    if a.match_provider_imports:assert matched,tcc_owners
    binding=dict(requested=a.match_provider_imports,verified=matched,tccImports=tcc_owners,provider=str(provider),
        scope='Stock SourceDriver fixes the API DLL to adjacent xlc.dll; this is not a guarantee for arbitrary custom tccDllPath/provider swapping')
    if a.match_provider_imports:
        binding['source']=str(driver_source);binding['sourceSha256']=sha(driver_source)
        binding['cohortSource']=str(expected_driver);binding['cohortSourceSha256']=sha(expected_driver)
        binding['canonicalLFSame']=True
    report['providerImportBinding']=binding
    # The compiler successor may carry new private/runtime headers and included
    # frontend fragments absent from its older full parent. Stage the actual
    # selected SDK, then refresh source-backed aliases from the frozen tree.
    selected_headers=Path(description.get('RuntimeHeaders',base/'include')) if a.overlay else base/'include'
    assert selected_headers.is_dir(),selected_headers
    shutil.copytree(selected_headers,out/'include')
    staged=copy.deepcopy(description)
    staged.update(Driver=str(out/'public_source.exe'),DriverSha256=sha(out/'public_source.exe'),
        RuntimeLibrary=str(out/selected['runtime'].name),RuntimeLibrarySha256=sha(out/selected['runtime'].name),
        TccCore=str(provider),TccCoreSha256=sha(provider),
        ParentSourceBuild=str(base/'build.json'),ParentSourceBuildSha256=sha(base/'build.json'),
        TccProviderRole='TinyCC and current VFS only; target objects are resources, no resident xlang/XRT machine code',
        Scope=__doc__)
    staged.update(DriverRelinked=a.match_provider_imports,DriverParentSha256=sha(selected['driver']),ProviderImportBinding=binding)
    if a.match_provider_imports:
        staged['linkResponse']=str(out/'driver-provider-relink.rsp')
        staged['linkResponseSha256']=sha(out/'driver-provider-relink.rsp')
    staged.update(peSmokeSkipped=a.skip_pe_smoke,peAotQualified=False,shippingDelivery=False)
    staged['Headers']=copy.deepcopy(description.get('Headers',original['Headers']))
    alias_families={}
    for row in staged['Headers']:
        path=Path(row['Path'])
        if a.overlay:
            source=Path(row['Source'])
            if source.is_relative_to(parent['cohort']):source=selected['cohort']/source.relative_to(parent['cohort'])
            assert source.is_file(),source
            relative=next((path.relative_to(folder) for folder in (selected_headers,base/'include') if path.is_relative_to(folder)),None)
            assert relative is not None,path
            shutil.copy2(source,out/'include'/relative)
            row.update(Source=str(source),Sha256=sha(source))
            row['Path']=str(out/'include'/relative)
            alias_families.setdefault((source.parent,relative.parent),set()).add(source.suffix)
            inputs[str(source)]=sha(source)
        if path.is_relative_to(base/'include'):
            row['Path']=str(out/'include'/path.relative_to(base/'include'))
            assert sha(row['Path']).lower()==row['Sha256'].lower()
    if a.overlay:
        recorded={Path(row['Path']).resolve() for row in staged['Headers']}
        for (folder,relative),suffixes in alias_families.items():
            for source in sorted(folder.iterdir()):
                target=out/'include'/relative/source.name
                if not source.is_file() or source.suffix not in suffixes or target.resolve() in recorded:continue
                shutil.copy2(source,target);inputs[str(source)]=sha(source);recorded.add(target.resolve())
                staged['Headers'].append(dict(Source=str(source),Path=str(target),Sha256=sha(source)))
        # New resident headers are guaranteed even when no predecessor alias
        # family existed for them. Never manufacture old flat release aliases.
        runtime_alias=out/'include/__xl_runtime';runtime_alias.mkdir(exist_ok=True)
        for source in sorted((selected['cohort']/'src/runtime').glob('*.h')):
            target=runtime_alias/source.name;shutil.copy2(source,target);inputs[str(source)]=sha(source)
            if target.resolve() not in recorded:
                recorded.add(target.resolve());staged['Headers'].append(dict(Source=str(source),Path=str(target),Sha256=sha(source)))
    if a.overlay:
        staged.update(fullRebuild=False,complete=True,passed=True,
            ParentCompilerOverlay=str(a.overlay.resolve()),ParentCompilerOverlaySha256=sha(a.overlay),
            RuntimeHeaders=str(out/'include'),includeOverlayRoot=str(out/'include'),
            ProviderInputs=[dict(Path=path,Sha256=value) for path,value in inputs.items()])
        assert 'Complete' not in staged and 'InputsUnchanged' not in staged
        (out/'overlay.json').write_text(json.dumps(staged,indent=2)+'\n')
    else:staged['Inputs'] += [dict(Path=path,Sha256=value) for path,value in inputs.items()]
    (out/'build.json').write_text(json.dumps(staged,indent=2)+'\n')
    if a.skip_pe_smoke:
        run('source-driver-jit',[out/'public_source.exe',selected['cohort']/'test/cases/cgen_basic.xl','run',out/'smoke-jit'])
        assert not(out/'smoke.exe').exists(),'JIT-only smoke unexpectedly emitted PE'
        report['jitSmokeQualified']=True
    else:
        run('source-driver-aot',[out/'public_source.exe',selected['cohort']/'test/cases/cgen_basic.xl','exe',out/'smoke'])
        run('source-aot-run',[out/'smoke.exe'])
        report['peAotQualified']=True
        staged['peAotQualified']=True
        (out/'build.json').write_text(json.dumps(staged,indent=2)+'\n')
        if a.overlay:(out/'overlay.json').write_text(json.dumps(staged,indent=2)+'\n')
    report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
    report['parentCohortUnchanged']=all(sha(row['Path']).lower()==row['Sha256'].lower()
        for row in original['Inputs']+original['Headers']+original['Objects'])
    report['driverUnchanged']=sha(out/'public_source.exe')==sha(selected['driver'])
    report['driverRelinked']=a.match_provider_imports
    report['residentRuntimeUnchanged']=sha(out/selected['runtime'].name)==sha(selected['runtime'])
    report['headers']=staged['Headers']
    report['providerSha256']=sha(provider);report['sourceCohort']=str(out/'overlay.json' if a.overlay else out/'build.json')
    report['sourceCohortMode']='overlay' if a.overlay else 'full'
    report['complete']=True;report['passed']=all(report[key] for key in
        ('inputsUnchanged','residentRuntimeUnchanged','parentCohortUnchanged'))and (report['driverUnchanged']or(a.match_provider_imports and matched));save()
    assert report['passed']
if __name__=='__main__':main()
