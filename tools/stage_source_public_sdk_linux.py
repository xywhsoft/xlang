"""Give a verified Linux source/provider stage its complete matched declaration SDK.

Source-build private SDKs alone do not include root-level xl.h/xrt_decl.h.
Merge the explicitly selected public declarations beside a freshly relinked
driver, retaining identical compiler objects and provider bytes. No frozen
cohort, inherited report or shipping artifact is changed.
"""
from pathlib import Path
import argparse,copy,hashlib,json,os,shutil,subprocess,time
from xlir_public_build_inputs import verify_public_build
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','base','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,base,out=[v.resolve() for v in (a.root,a.base,a.out)]
    assert os.name!='nt';build,pinned,_=verify_public_build(root,base)
    original_provider=json.loads((base/'provider.json').read_text())
    assert original_provider['complete'] and original_provider['passed'] and original_provider['mode']=='full'
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();cc=shutil.which('gcc');assert cc
    public=Path(build['SdkDirectory'])/'include'
    files=[path for path in public.rglob('*') if path.is_file() and path.suffix in ('.h','.inc','.def')]
    pinned.update([Path(__file__).resolve(),Path(cc),base/'provider.json',Path(build['TccCore']),*files])
    inputs={str(path):sha(path) for path in pinned};out.mkdir(parents=True,exist_ok=False)
    shutil.copytree(base/'include',out/'include');added=[]
    for source in files:
        target=out/'include'/source.relative_to(public);target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists():assert sha(target)==sha(source),target
        else:shutil.copy2(source,target);added.append(dict(Path=str(target),Source=str(source),Sha256=sha(target)))
    provider=out/'release/libxlc.so';provider.parent.mkdir();shutil.copy2(build['TccCore'],provider)
    shutil.copytree(Path(build['NativeRoot'])/'build/linux/tcc-dist',out/'build/linux/tcc-dist')
    driver=out/'public_source';command=[cc,*[row['Path'] for row in build['Objects']],'-L'+str(provider.parent),'-lxlc',
        '-Wl,-rpath,'+str(provider.parent),'-Wl,--export-dynamic','-pthread','-ldl','-lm','-o',driver]
    start=time.monotonic();r=subprocess.run(command,capture_output=True,timeout=240)
    (out/'driver-relink.log').write_bytes(r.stdout+r.stderr);assert not r.returncode
    staged=copy.deepcopy(build)
    for row in staged['Headers']:
        path=Path(row['Path'])
        if path.is_relative_to(base/'include'):row['Path']=str(out/'include'/path.relative_to(base/'include'))
    staged['Headers']+=added
    staged.update(Driver=str(driver),DriverSha256=sha(driver),RuntimeLibrary=str(provider),TccCore=str(provider),
        TccCoreSha256=sha(provider),NativeRoot=str(out),ParentSourceBuild=str(base/'build.json'),
        ParentSourceBuildSha256=sha(base/'build.json'),ProviderSdkCompletionScope=__doc__)
    staged['Inputs'] += [dict(Path=path,Sha256=value) for path,value in inputs.items() if path not in {row['Path'] for row in staged['Inputs']}]
    (out/'build.json').write_text(json.dumps(staged,indent=2)+'\n')
    report=dict(scope=__doc__,mode='full',complete=True,passed=all(sha(path)==value for path,value in inputs.items()),
        inputs=inputs,parentProvider=str(base/'provider.json'),parentProviderSha256=sha(base/'provider.json'),
        providerSha256=sha(provider),providerUnchanged=sha(provider)==sha(build['TccCore']),
        compilerObjectsRebuilt=False,driverRelinked=True,publicHeadersAdded=len(added),
        driverRelinkSeconds=round(time.monotonic()-start,3),shippingAcceptance=False)
    (out/'provider.json').write_text(json.dumps(report,indent=2)+'\n');assert report['passed'] and report['providerUnchanged']
    print('Matched public SDK staged:',len(added),'declarations; provider and compiler objects unchanged',flush=True)
if __name__=='__main__':main()
