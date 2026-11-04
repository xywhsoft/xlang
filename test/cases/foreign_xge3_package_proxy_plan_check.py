"""Actual pinned native Proxy/Font participants in the verified resident plan.

Reuses the accepted lifecycle objects and exact shared XRT implementation
object. Compiles the immutable native component into the same CPU program.
This verifies real component/plan behavior, not full compiler or GUI delivery.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','native-source','plan-evidence','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--plan-source',type=Path,help='Verified immutable planner source; resolves historical workspace inputs by exact SHA')
    p.add_argument('--case',choices=('proxy','draw'),default='proxy')
    a=p.parse_args();root,native,parent,out=[v.resolve() for v in (a.root,a.native_source,a.plan_evidence,a.out)]
    reportPath=parent/'results.json';evidence=json.loads(reportPath.read_text());assert evidence['passed'] and evidence['complete']
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    # The native component has its own immutable source manifest, rather than
    # depending on the concurrently developed engine worktree.
    manifest=native/'manifest.json';nm=json.loads(manifest.read_text())
    assert all(sha(native/row['path'])==row['sha256'] for row in nm['files'])
    cc=shutil.which('gcc');assert cc
    case=root/('test/cases/foreign_xge3_package_draw_plan.c' if a.case=='draw' else 'test/cases/foreign_xge3_package_proxy_plan.c');component=native/'src/xui/core/xui_proxy_resource.c'
    files=[Path(__file__).resolve(),Path(cc),case,reportPath,manifest,*[native/row['path'] for row in nm['files']]]
    resolved={};remapped={};planSource=a.plan_source.resolve() if a.plan_source else None
    if planSource:
        sourceManifest=planSource/'manifest.json';sourceFiles=json.loads(sourceManifest.read_text())
        assert all(sha(planSource/row['path'])==row['sha256'] for row in sourceFiles)
        files.extend([sourceManifest,*[planSource/row['path'] for row in sourceFiles]])
    for path,value in evidence['inputs'].items():
        original=Path(path);actual=original
        if not actual.is_file() or sha(actual).lower()!=value.lower():
            assert planSource and original.is_relative_to(root),path
            actual=planSource/original.relative_to(root)
            assert actual.is_file() and sha(actual).lower()==value.lower(),path
            remapped[path]=str(actual)
        resolved[path]=actual;files.append(actual)
    inputs={str(v):sha(v) for v in files}
    for path,value in evidence['inputs'].items():assert sha(resolved[path]).lower()==value.lower(),path
    out.mkdir(parents=True,exist_ok=False)
    result=dict(complete=False,passed=False,scope=__doc__,case=a.case,inputs=inputs,rows=[],parentPlanEvidence=str(reportPath),historicalInputPathRemaps=remapped,nativeSource=str(native),fullCompilerRebuild=False,gpuTests=False,networkTests=False)
    def save():(out/'results.json').write_text(json.dumps(result,indent=2)+'\n')
    for opt in ('O0','O2'):
        original=next(row['command'] for row in evidence['rows'] if row['name']=='xlir_ownership_checked_drop_runtime.'+opt+'.build')
        objects=[Path(value) for value in original if str(value).endswith('.o')]
        systems=[value for value in original if str(value).startswith('-l') or str(value).startswith('-Wl,')]
        for value in objects:inputs[str(value)]=sha(value)
        program=out/(opt+('.exe' if os.name=='nt' else ''))
        includes=([ '-I'+str(planSource)] if planSource else [])+[value for value in original if str(value).startswith('-I')]
        includes+=['-I'+str(v) for v in (root,native,native/'include',native/'extlibs/xrt',native/'src/xui/core')]
        command=[cc,'-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-cast-function-type','-'+opt,*includes,case,component,*objects,*systems,'-o',program]
        if os.name!='nt':command+=['-pthread','-ldl','-lm']
        for name,args in ((opt+'-build',command),(opt+'-run',[program])):
            start=time.monotonic();r=subprocess.run(list(map(str,args)),cwd=root,capture_output=True,timeout=240)
            log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
            result['rows'].append(dict(name=name,command=list(map(str,args)),exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
            print(name,r.returncode,flush=True);assert not r.returncode,log
    result['inputsUnchanged']=all(sha(v)==value for v,value in inputs.items());assert result['inputsUnchanged']
    result.update(complete=True,passed=True);save()
if __name__=='__main__':main()
