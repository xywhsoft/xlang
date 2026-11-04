"""Freeze and rebuild the six post-r30 foreign-bootstrap compiler units on Linux.

All other compiler objects are exact verified parent bytes. The existing
provider is an independently qualified full-VFS artifact; its historical tool
paths are not rewritten or treated as current source/compiler inputs.
Embedded compiler objects remain at the provider's recorded revision until a
separate producer overlay rebuilds them. This is a declared partial rebuild.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('workspace','root','base','provider','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();workspace,root,base,provider_stage,out=[v.resolve() for v in (a.workspace,a.root,a.base,a.provider,a.out)]
    assert os.name!='nt';build,pinned,_=verify_public_build(root,base);cc=shutil.which('gcc');assert cc
    provider_build=json.loads((provider_stage/'build.json').read_text())
    provider_report=json.loads((provider_stage/'provider.json').read_text());assert provider_report['complete'] and provider_report['passed']
    provider=Path(provider_build['TccCore']);sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    assert sha(provider)==provider_build['TccCoreSha256'].lower()
    units={'src/api/xl_api.c','src/runtime/xl6_foreign_view.c','src/sem/xl6_sem.c','src/ir/xl6_ir.c',
        'src/ir/xl6_ir_lower.c','src/cgen/xl6_ir_cgen.c'}
    allowed=units|{'src/cgen/xl6_ir_cgen_foreign_view.c','src/cgen/xl6_ir_cgen_native_dependencies.c',
        'src/ir/xl6_ir_foreign_parameter.c','src/type/xl6_foreign_call_contract.h'}
    changed={path.relative_to(workspace).as_posix() for path in (workspace/'src').rglob('*') if path.is_file() and
        path.suffix in ('.h','.c','.inc') and (root/path.relative_to(workspace)).is_file() and
        path.read_bytes().replace(b'\r\n',b'\n')!=(root/path.relative_to(workspace)).read_bytes().replace(b'\r\n',b'\n')}
    assert changed and changed<=allowed,sorted(changed-allowed)
    out.mkdir(parents=True,exist_ok=False);snapshot=out/'snapshot';shutil.copytree(root,snapshot)
    delta=[]
    for name in sorted(changed):
        shutil.copy2(workspace/name,snapshot/name);delta.append(dict(path=name,oldSha256=sha(root/name),sha256=sha(snapshot/name)))
    shutil.copytree(provider_stage/'include',out/'include')
    # Compiler-private declaration ABI changed only by an inline predicate.
    private=out/'include/__xl_compiler/src/type/xl6_foreign_call_contract.h'
    if private.exists():shutil.copy2(snapshot/'src/type/xl6_foreign_call_contract.h',private)
    def remap(path):
        path=Path(path)
        return snapshot/path.relative_to(root) if path.is_relative_to(root) else path
    staged=copy.deepcopy(build);rows=[]
    for row in staged['Inputs']:
        path=remap(row['Path']);row['Path']=str(path);row['Sha256']=sha(path)
    for name in sorted(changed):
        path=snapshot/name
        if str(path) not in {row['Path'] for row in staged['Inputs']}:staged['Inputs'].append(dict(Path=str(path),Sha256=sha(path)))
    for row in staged['Headers']:
        original=Path(row['Path']);new=out/'include'/original.relative_to(base/'include')
        assert new.is_file();row['Path']=str(new);row['Sha256']=sha(new)
        if 'Source' in row:row['Source']=str(remap(row['Source']))
    # Include the additional matched public root headers from provider staging.
    for row in provider_build['Headers']:
        path=Path(row['Path'])
        if not path.is_relative_to(provider_stage/'include'):continue
        target=out/'include'/path.relative_to(provider_stage/'include')
        if str(target) not in {item['Path'] for item in staged['Headers']}:
            new=copy.deepcopy(row);new['Path']=str(target);new['Sha256']=sha(target);staged['Headers'].append(new)
    flags=[flag.replace(str(root),str(snapshot)) for flag in build['CompileFlags']]
    compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable','-Wno-unused-label',
        '-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers',
        '-Wno-pointer-sign','-Wno-format-truncation']
    report=dict(scope=__doc__,complete=False,passed=False,delta=delta,rows=rows,units=sorted(units),
        parentSourceBuild=str(base/'build.json'),providerBuild=str(provider_stage/'build.json'))
    def save():(out/'overlay.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        start=time.monotonic();r=subprocess.run(list(map(str,command)),cwd=snapshot,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        rows.append(dict(name=name,command=list(map(str,command)),exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log
    for row in staged['Objects']:
        old=Path(row['Source']);name=old.relative_to(root).as_posix();row['Source']=str(snapshot/name)
        if name in units:
            target=out/(old.stem+'.o');run(old.stem,[cc,*flags,*compat,'-c',snapshot/name,'-o',target]);row['Path']=str(target);row['Sha256']=sha(target)
    staged['Sources']=[str(remap(path)) for path in build['Sources']]
    staged['CompileFlags']=flags;staged['SdkDirectory']=str(remap(build['SdkDirectory']))
    driver=out/'public_source'
    run('driver-link',[cc,*[row['Path'] for row in staged['Objects']],provider,'-Wl,-rpath,'+str(provider.parent),
        '-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',driver])
    staged.update(Driver=str(driver),DriverSha256=sha(driver),RuntimeLibrary=str(provider),TccCore=str(provider),TccCoreSha256=sha(provider),
        NativeRoot=provider_build['NativeRoot'],Scope=__doc__,PartialSourceRebuild=True,SourceOverlayUnits=sorted(units),
        ParentSourceBuild=str(base/'build.json'),ParentSourceBuildSha256=sha(base/'build.json'),
        ProviderArtifactSha256=sha(provider),EmbeddedCompilerRevision='Provider-recorded r30; separate producer overlay required')
    staged['Inputs'] += [dict(Path=str(path),Sha256=sha(path)) for path in (provider,provider_stage/'build.json',provider_stage/'provider.json')]
    (out/'build.json').write_text(json.dumps(staged,indent=2)+'\n')
    report.update(complete=True,passed=all(sha(root/row['path'])==row['oldSha256'] and sha(workspace/row['path'])==row['sha256'] for row in delta),
        driverSha256=sha(driver));save();assert report['passed']
if __name__=='__main__':main()
