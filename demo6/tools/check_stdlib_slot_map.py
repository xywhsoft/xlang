"""Public SlotMap core calls: source JIT, strict standalone C and checked faults.
Optional --snapshots adds public independent keys/values and snapshot consumers.
Not paired-iterator/frame/persistence or pruned shipping acceptance.
"""
from pathlib import Path
import argparse,fnmatch,hashlib,json,os,re,shutil,subprocess
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def stage_private_sdk(root):
    """Expand tracked private-source VFS entries for an unbundled source driver.

    This is a physical test SDK, not a regenerated shipping VFS. Compiler-import
    resource blobs use a separate build output and are deliberately not staged.
    Existing files must already match; never overwrite another SDK generation.
    """
    manifest=root/'tools'/('tcc_vfs_manifest_win64_xlc.txt' if os.name=='nt' else 'tcc_vfs_manifest_linux_xlc.txt')
    private=(root/'__xl_compiler').resolve(); rows=[]; skipped=[]; seen=set()
    for line in manifest.read_text(encoding='utf-8-sig').splitlines():
        if not line.strip() or line.lstrip().startswith('#'): continue
        fields=line.split('|'); assert len(fields) in (3,4),line
        kind,prefix,source=fields[:3]
        if not prefix.startswith('__xl_compiler/'): continue
        if not source.startswith('%XL6_ROOT%'):
            assert source.startswith('%XL6_VFS_OUTPUT_DIR%'),line
            skipped.append(prefix); continue
        source_path=(root/source[len('%XL6_ROOT%'):].replace('\\','/').lstrip('/')).resolve()
        assert source_path.is_relative_to(root) and source_path.exists(),source_path
        patterns=fields[3].split(';') if len(fields)==4 else ['*']
        assert kind in ('file','dir'),line
        candidates=[source_path] if kind=='file' else sorted(p for p in source_path.rglob('*')
            if p.is_file() and any(fnmatch.fnmatchcase(p.name,pattern) for pattern in patterns))
        for origin in candidates:
            assert origin.resolve().is_relative_to(root),origin
            relative=Path(prefix)/(origin.relative_to(source_path) if kind=='dir' else Path())
            target=(root/relative).resolve()
            assert target.is_relative_to(private) and target!=private and target not in seen,target
            seen.add(target); target.parent.mkdir(parents=True,exist_ok=True)
            if target.exists(): assert sha(target)==sha(origin),target
            else: shutil.copy2(origin,target)
            assert sha(target)==sha(origin)
            rows.append(dict(Source=str(origin),Path=str(target),Sha256=sha(target)))
    assert rows and skipped
    return manifest,rows,skipped
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'): parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--snapshots',action='store_true')
    parser.add_argument('--pruned',action='store_true',help='Execute independently linked snapshot plans and public AOT')
    a=parser.parse_args(); root=a.root.resolve(); public=a.public_directory.resolve(); out=a.out.resolve()
    assert not a.pruned or a.snapshots
    build=json.loads((public/'build.json').read_text(encoding='utf-8-sig')); driver=Path(build['Driver'])
    cc=shutil.which('gcc'); assert cc and not out.exists() and build['SourceXrt'] and not build['RuntimeOnly']
    runtime=root/'release/xl.dll' if os.name=='nt' else Path(build['NativeRoot'])/'release/libxl.so'
    source=root/('test/cases/stdlib_slot_map_snapshots_language.xl' if a.snapshots else 'test/cases/stdlib_slot_map.xl')
    host=root/('test/cases/stdlib_slot_map_snapshot_language_host.c' if a.snapshots else 'test/cases/stdlib_slot_map_host.c')
    contract=root/'test/cases/xlir_slot_map_contract.c'; memory=root/'test/support/module_product_memory.c'
    errors=root/'test/cases/stdlib_slot_map_errors.xl'
    artifact=root/'test/cases/xlir_artifact_runtime_contract.c'; lean=root/'test/cases/stdlib_slot_map_values_prune.xl'
    files={Path(__file__).resolve(),public/'build.json',driver,Path(cc),runtime,source,errors,host,contract,memory}
    nm=shutil.which('nm'); assert nm or not a.pruned
    if a.pruned: files.update({artifact,lean,Path(nm)})
    sdk=[]; skipped=[]
    if a.pruned:
        manifest,sdk,skipped=stage_private_sdk(root); files.add(manifest)
        files.update(Path(row[key]) for row in sdk for key in ('Source','Path'))
    for row in build['Inputs']+build['Headers']+build['Objects']:
        assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]; out.mkdir(); rows=[]; products=[]; prefixes=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=products,Prefixes=prefixes,
        LanguageSlotMapCoreCalls=True,LanguageSlotMapSnapshots=a.snapshots,LanguageSlotMapFullCutover=False,Full53Acceptance=False,
        PrunedShippingArtifact=False,PrunedSourceExecutables=False,PublicAotExecutable=False,Scope=__doc__)
    if a.pruned: report['PhysicalPrivateSourceSdk']=sdk; report['UnstagedCompilerImportResources']=skipped
    env=dict(os.environ)
    if os.name!='nt': env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    else: shutil.copy2(runtime,out/'xl.dll')
    def run(name,cmd):
        p=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=600)
        log=out/(name+'.log'); log.write_bytes(p.stdout+p.stderr)
        rows.append(dict(Name=name,Exit=p.returncode,Log=str(log),LogSha256=sha(log)))
        assert p.returncode==0,(name,(p.stdout+p.stderr)[-7000:])
        return (p.stdout+p.stderr).decode(errors='replace')
    try:
        generated=out/'slot-map.c'; repeat=out/'repeat.c'
        run('generate',[driver,source,'cgen-default',generated]); run('repeat',[driver,source,'cgen-default',repeat]); assert sha(generated)==sha(repeat)
        body=generated.read_text(encoding='utf-8-sig')
        assert re.search(r'xl6SlotMapInsertV1\(v\d+,&slot_map_layout,&v\d+,&v\d+\)',body)
        assert re.search(r'xl6SlotMapReadRequiredV1\(v\d+,&slot_map_layout,v\d+,&v\d+\)',body)
        if a.snapshots:
            assert re.search(r'xl6SlotMapKeysV1\(v\d+,&slot_map_layout,0x[0-9A-F]+ULL\)',body)
            assert re.search(r'xl6SlotMapValuesV1\(v\d+,&slot_map_layout,',body)
        else:
            assert 'xl6SlotMapKeyBoxV1(' not in body and 'xl6SlotMapKeyUnboxV1(' not in body
        assert '#include <__xl_runtime/xl6_slot_map.h>' in body
        run('public-jit',[driver,source,'run',out/'jit'])
        run('checked-errors',[driver,errors,'run',out/'errors-jit'])
        negatives={
            'key-type':'auto map = SlotMap::<int>()\nmap.contains(1)\n',
            'value-type':'auto map = SlotMap::<int>()\nmap.insert("bad")\n',
            'arity':'auto map = SlotMap::<int>()\nauto key = map.insert(1)\nmap.set(key)\n',
            'closure-domain':'auto map = SlotMap::<func(int) as int>()\n'}
        report['RejectedSourceContracts']=[]
        for label,body in negatives.items():
            fixture=out/(label+'.xl'); fixture.write_text(body,encoding='utf-8')
            p=subprocess.run([str(driver),str(fixture),'cgen-default',str(out/(label+'.c'))],cwd=root,env=env,capture_output=True,timeout=120)
            log=out/(label+'.log'); log.write_bytes(p.stdout+p.stderr)
            rows.append(dict(Name=label,Exit=p.returncode,Log=str(log),LogSha256=sha(log)))
            assert p.returncode and b'failed:' in p.stdout+p.stderr
            report['RejectedSourceContracts'].append(label)
        for mode in ('ir-typed','ir-lowered'):
            target=out/(mode+'.json'); run(mode,[driver,source,mode,target]); products.append(dict(Path=str(target),Sha256=sha(target)))
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in ('xlir_public_source_driver.o','xlir_public_source_linux_driver.o')]
        flags=['-std=c11','-Wall','-Wextra','-Werror','-I.','-Irelease','-Irelease/include','-Isrc/runtime','-Ilib/xruntime','-Ilib/xrt','-I'+str(public/'include'),'-DXL_STATIC_EMBED',
            '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable',
            '-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
        links=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
        if os.name!='nt': flags+=['-D_GNU_SOURCE']
        ext='.exe' if os.name=='nt' else ''
        proof=out/('contract'+ext); run('contract-build',[cc,*flags,'-Ilib/tcc','-O0',contract,*objects,runtime,*links,'-o',proof])
        text=run('contract',[proof,source]); match=re.search(r'slot-map calls=(\d+) mutations=(\d+)',text); assert match
        report['ContractCalls'],report['RefusedMutations']=map(int,match.groups())
        memory_object=out/'memory.o'; run('memory-build',[cc,*flags,'-O0','-c',memory,'-o',memory_object])
        for opt in ('O0','O2'):
            exe=out/(opt+ext); run(opt+'-build',[cc,*flags,'-'+opt,'-include',generated,host,memory_object,*objects,runtime,*links,'-o',exe])
            run(opt+'-lifetimes',[exe]); products.append(dict(Path=str(exe),Sha256=sha(exe)))
            fault=out/(opt+'-fault'+ext)
            calls=('xl6SlotMapKeysV1','xl6SlotMapValuesV1','xl6SlotMapKeyUnboxV1') if a.snapshots else ('xl6SlotMapInsertV1','xl6SlotMapReadRequiredV1','xl6SlotMapSizeV1')
            wraps=['-Wl,--wrap='+s for s in calls]
            run(opt+'-fault-build',[cc,*flags,'-'+opt,'-DXL6_SLOT_MAP_TEST_FAULTS','-include',generated,host,memory_object,*objects,runtime,*links,*wraps,'-o',fault])
            products.append(dict(Path=str(fault),Sha256=sha(fault)))
            for lane in (('keys','values','project','oom') if a.snapshots else ('insert','get','size','oom')):
                for at in range(512):
                    text=run(opt+'-'+lane+'-'+str(at),[fault,lane,at]); assert 'prefix='+lane+' at='+str(at)+' hit=' in text
                    if 'hit=0' in text: break
                else: raise AssertionError('Unfinished prefix '+lane)
                prefixes.append(dict(Optimization=opt,Lane=lane,Runs=at+1,Complete=True))
        if a.pruned:
            report['PrunedPlans']=[]
            planner=out/('artifact'+ext)
            run('artifact-build',[cc,*flags,'-Ilib/tcc','-O0',artifact,*objects,runtime,*links,'-o',planner])
            products.append(dict(Path=str(planner),Sha256=sha(planner)))
            for tag,fixture in (('full',source),('values',lean)):
                implementation=out/(tag+'-runtime.c')
                text=run(tag+'-plan',[planner,'--emit-native',fixture.as_posix(),implementation])
                plans=[json.loads(line) for line in text.splitlines() if line.startswith('{"Fixture":')]; assert len(plans)==1
                plan=plans[0]; assert '#define XRT_MODULE_ALL' not in implementation.read_text() and '#define XRUNTIME_MODULE_ALL' not in implementation.read_text()
                raw=implementation.read_text(); implementation.write_text('#define XRT_MODULE_MEMORY_DEBUG\n'+raw)
                code=generated if tag=='full' else out/'values.c'
                if tag=='values': run('values-generate',[driver,fixture,'cgen-default',code])
                report['PrunedPlans'].append(dict(Fixture=tag,Sources=plan['Sources'],NativeModules=plan['NativeModules']))
                for opt in ('O0','O2'):
                    providers=[]
                    for index,provider in enumerate([implementation,*[root/s for s in plan['Sources']],memory]):
                        obj=out/(tag+'-'+opt+'-'+str(index)+'.o')
                        run(tag+'-'+opt+'-provider-'+str(index),[cc,*flags,'-'+opt,'-c',provider,'-o',obj]); providers.append(obj)
                        products.append(dict(Path=str(obj),Sha256=sha(obj)))
                    binary=out/(tag+'-'+opt+'-pruned'+ext)
                    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-pthread','-ldl','-lm']
                    run(tag+'-'+opt+'-pruned-build',[cc,*flags,'-'+opt,'-include',code,host,*providers,*systems,'-o',binary])
                    run(tag+'-'+opt+'-pruned-lifetimes',[binary])
                    symbols=run(tag+'-'+opt+'-symbols',[nm,'--defined-only',binary])
                    if tag=='values':
                        for name in ('xl6SlotMapKeysV1','xl6SlotMapKeyBoxV1','xl6SlotMapKeyHashV1','xl6SequenceSlotMapKeyCreateV1'):
                            assert not re.search(r'\b'+name+r'\b',symbols),name
                    products.append(dict(Path=str(binary),Sha256=sha(binary)))
                products.extend(dict(Path=str(p),Sha256=sha(p)) for p in {implementation,code})
            run('public-module',[driver,source,'module',out/'module'])
            executable=out/'aot.exe'
            run('public-aot-build',[driver,source,'exe',out/'aot' if os.name=='nt' else executable])
            run('public-aot-execute',[executable]); products.append(dict(Path=str(executable),Sha256=sha(executable)))
            report['PrunedSourceExecutables']=report['PublicAotExecutable']=True
        for p in (generated,repeat,proof,memory_object): products.append(dict(Path=str(p),Sha256=sha(p)))
        report['LifetimesPerOptimization']=1000; report['Complete']=True
    finally:
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    assert report['Complete'] and report['InputsUnchanged']; print('public SlotMap core calls verified',flush=True)
if __name__=='__main__': main()
