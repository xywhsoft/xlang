"""Update six compiler units in a fresh Linux O2 producer/VFS, preserving all other objects.

This is an explicit partial producer rebuild. It verifies the parent's source
fingerprint, compiler identity, object hashes and original profile flags. The
only header change permitted is the new unused-unless-called ownership-helper
inline predicate; no record layout changes are admitted. Parent producer,
objects, manifests and reports remain immutable.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,re,shutil,subprocess,sys,time
from xlir_public_build_inputs import verify_public_build
from precompile_builtin import source_fingerprint
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('source-root','cohort-root','base','parent','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();source_root,cohort,base,parent,out=[v.resolve() for v in (a.source_root,a.cohort_root,a.base,a.parent,a.out)]
    assert os.name!='nt';cc=shutil.which('gcc');assert cc
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    old=json.loads((parent/'precompiled/manifest.json').read_text());old_producer=parent/'producer'
    assert old['target']=='linux-x86_64' and old['sources']==source_fingerprint(old_producer)
    assert old['nativeCompiler']==sha(cc)
    for row in old['objects']:assert sha(row['source'])==row['sha256'],row['source']
    units={'src/api/xl_api.c','src/runtime/xl6_foreign_view.c','src/sem/xl6_sem.c','src/ir/xl6_ir.c',
        'src/ir/xl6_ir_lower.c','src/cgen/xl6_ir_cgen.c'}
    allowed=units|{'src/cgen/xl6_ir_cgen_foreign_view.c','src/cgen/xl6_ir_cgen_native_dependencies.c',
        'src/ir/xl6_ir_foreign_parameter.c','src/type/xl6_foreign_call_contract.h'}
    changed={path.relative_to(source_root).as_posix() for path in (source_root/'src').rglob('*') if path.is_file() and
        path.suffix in ('.c','.h','.inc') and (old_producer/path.relative_to(source_root)).is_file() and
        path.read_bytes().replace(b'\r\n',b'\n')!=(old_producer/path.relative_to(source_root)).read_bytes().replace(b'\r\n',b'\n')}
    assert changed and changed<=allowed,sorted(changed-allowed)
    contract='src/type/xl6_foreign_call_contract.h'
    before=(old_producer/contract).read_text();after=(source_root/contract).read_text()
    stripped=re.sub(r'/\* Ownership helpers have one exact raw owner input.*?^}\n\n','',after,flags=re.S|re.M)
    assert stripped==before,'Only the exact additional static-inline ownership predicate may change'
    out.mkdir(parents=True,exist_ok=False);producer=out/'producer';shutil.copytree(old_producer,producer)
    shutil.copy2(__file__,out/'producer-tool.py')
    delta=[]
    for name in sorted(changed):
        shutil.copy2(source_root/name,producer/name);delta.append(dict(path=name,parentSha256=sha(old_producer/name),sha256=sha(producer/name)))
    precompiled=out/'precompiled';precompiled.mkdir();updated=copy.deepcopy(old)
    updates=[];rows=[];report=dict(scope=__doc__,complete=False,passed=False,delta=delta,rows=rows,updatedObjects=updates,
        parentManifest=str(parent/'precompiled/manifest.json'),parentManifestSha256=sha(parent/'precompiled/manifest.json'),
        originalFlags=old['flags'],originalCompilerSha256=old['nativeCompiler'],originalSystemHeaders=old['nativeSystemHeaders'],
        recordLayoutChanged=False,fullProducerRecompiled=False,sourceRoot=str(source_root),toolSha256=sha(out/'producer-tool.py'))
    def save():(out/'provider.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,cwd=producer,timeout=600):
        start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=cwd,capture_output=True,timeout=timeout)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,
            seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log;return r.stdout
    for row in updated['objects']:
        if row.get('product')=='xrt':continue
        _,profile,name=row['virtual'].split('/',2);name=name[:-2]
        if name not in units:continue
        flags=old['flags']+(['-DXL_BUILD_DLL','-DLIBTCC_AS_DLL'] if profile.startswith('shared') else ['-DXL_STATIC_EMBED'])
        if profile.endswith('runtime'):flags+=['-DXL_RUNTIME_ONLY_DLL','-DXL_RUNTIME_ONLY_API']
        target=precompiled/profile/(name+'.o');target.parent.mkdir(parents=True,exist_ok=True)
        original=dict(row);run(profile+'-'+Path(name).stem,[cc,*flags,'-c',name,'-o',target])
        row.update(source=str(target),sha256=sha(target),bytes=target.stat().st_size)
        updates.append(dict(virtual=row['virtual'],original=original,updated=dict(row),flags=flags))
    assert units=={item['virtual'].split('/',2)[2][:-2] for item in updates}
    updated['sources']=source_fingerprint(producer);updated['parentSignature']=old['signature']
    updated['partialProducerRebuild']=True;updated['overlayUnits']=sorted(units)
    updated['signature']=hashlib.sha256(json.dumps(dict(parent=old['signature'],sources=updated['sources'],updates=updates),sort_keys=True).encode()).hexdigest()
    manifest=precompiled/'manifest.json';manifest.write_text(json.dumps(updated,indent=2)+'\n')
    report.update(sourceFingerprint=updated['sources'],recompiledObjectCount=len(updates),
        reusedObjectCount=len(old['objects'])-len(updates),precompiledManifestSha256=sha(manifest));save()
    packer=parent/'lzma-pack';dist=out/'build/linux/tcc-dist';shutil.copytree(parent/'build/linux/tcc-dist',dist)
    native_xrt=Path(next(row['source'] for row in updated['objects'] if row.get('product')=='xrt' and row['virtual'].startswith('__xl_objects/static-native/')))
    pack=[sys.executable,producer/'tools/gen_tcc_vfs_resources.py','--root',producer,'--tcc-dist',dist,
        '--xrt-obj',native_xrt,'--precompiled',manifest,'--pack-tool',packer,'--packing-order','family']
    for family in ('runtime','xlc'):
        target=out/('tcc_builtin_resources_runtime.c' if family=='runtime' else 'tcc_builtin_resources.c')
        run(family+'-resources',[*pack,'--manifest','tools/tcc_vfs_manifest_linux_'+family+'.txt','--output',target])
    provider=out/'release/libxlc.so';provider.parent.mkdir();tcc=producer/'lib/tcc';lzma=producer/'lib/lzma'
    run('provider-build',[cc,'-std=c11','-O2','-fPIC','-shared','-Wl,-Bsymbolic','-DLIBTCC_AS_DLL','-I'+str(tcc),'-I'+str(lzma),
        tcc/'libtcc.c',tcc/'tcctools.c',tcc/'tcc_builtin_vfs.c',lzma/'LzmaDec.c',out/'tcc_builtin_resources.c','-ldl','-lm','-o',provider])
    build,pinned,_=verify_public_build(cohort,base)
    shutil.copytree(base/'include',out/'include');added=[];sdk=Path(build['SdkDirectory'])/'include'
    for original in sdk.rglob('*'):
        if not original.is_file() or original.suffix not in ('.h','.inc','.def'):continue
        target=out/'include'/original.relative_to(sdk);target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists():assert sha(target)==sha(original)
        else:shutil.copy2(original,target);added.append(dict(Path=str(target),Source=str(original),Sha256=sha(target)))
    driver=out/'public_source'
    run('driver-link',[cc,*[row['Path'] for row in build['Objects']],provider,'-Wl,-rpath,'+str(provider.parent),
        '-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',driver])
    staged=copy.deepcopy(build)
    for row in staged['Headers']:
        path=Path(row['Path'])
        if path.is_relative_to(base/'include'):row['Path']=str(out/'include'/path.relative_to(base/'include'))
    staged['Headers']+=added
    staged.update(Driver=str(driver),DriverSha256=sha(driver),RuntimeLibrary=str(provider),TccCore=str(provider),TccCoreSha256=sha(provider),
        NativeRoot=str(out),ParentSourceBuild=str(base/'build.json'),ParentSourceBuildSha256=sha(base/'build.json'),
        ProviderStageMode='full',ProviderStageScope=__doc__,PartialProducerRebuild=True,EmbeddedCompilerSourceRoot=str(source_root))
    staged['Inputs'] += [dict(Path=str(path),Sha256=sha(path)) for path in (manifest,provider,out/'producer-tool.py')]
    (out/'build.json').write_text(json.dumps(staged,indent=2)+'\n')
    report.update(complete=True,passed=source_fingerprint(old_producer)==old['sources'] and source_fingerprint(producer)==updated['sources'] and
        all(sha(row['source'])==row['sha256'] for row in old['objects']) and
        all(sha(item['updated']['source'])==item['updated']['sha256'] for item in updates),
        mode='full',provider=str(provider),providerSha256=sha(provider),shippingAcceptance=False,
        parentSourceBuild=str(base/'build.json'),sourceCohort=str(out/'build.json'));save();assert report['passed']
if __name__=='__main__':main()
