"""CPU-only Cell bootstrap policy and exact native domain integration.

The complete source cohort is verified. An optional explicit runtime overlay
supports development kernels and is recorded separately from formal reuse.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','cohort-root','public-directory','packages','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--runtime-overlay',action='store_true',help='Development only: rebuild five runtime translation units')
    p.add_argument('--case',choices=('cell','leaf','proxy_policy','proxy_affinity'),default='cell')
    p.add_argument('--font',type=Path,help='CPU font input for the explicit affinity reproducer')
    p.add_argument('--affinity-guarded',action='store_true',help='Require worker BUSY and creator-thread snapshot retry')
    p.add_argument('--checked-plan-evidence',type=Path,help='Verified FinishV2 lifecycle objects; explicit partial runtime overlay')
    p.add_argument('--checked-plan-source',type=Path,help='Matching immutable lifecycle declaration/source snapshot')
    a=p.parse_args();root,cohort,public,packages,out=[v.resolve() for v in (a.root,a.cohort_root,a.public_directory,a.packages,a.out)]
    if a.case=='proxy_affinity' and not a.font:p.error('proxy_affinity requires --font')
    if a.affinity_guarded and a.case!='proxy_affinity':p.error('--affinity-guarded requires --case proxy_affinity')
    sys.path.insert(0,str(root/'tools'));from xlir_public_build_inputs import verify_public_build
    from compiler_response import command_with_response
    build,pinned,_=verify_public_build(cohort,public);cc=shutil.which('gcc');assert cc
    sdk=Path(build.get('RuntimeDirectory',build.get('SdkDirectory','')))
    runtime=sdk/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    names=('xl6_code_lease','xl6_foreign_context','xl6_ownership_plan','xl6_ownership_catalog','xl6_foreign_view')
    source=root/'test/cases'/('foreign_xge3_package_'+a.case+'.c');native=packages/'_xge_native'
    engine=native/'native'/('xge.dll' if os.name=='nt' else 'libxge.so')
    pinned.update([Path(__file__).resolve(),Path(cc),runtime,source,*[path for path in native.rglob('*') if path.is_file()]])
    if a.font:pinned.add(a.font.resolve())
    checked=None;checkedSource=None;checkedObjects={};checkedRemaps={}
    if a.checked_plan_evidence or a.checked_plan_source:
        assert a.checked_plan_evidence and a.checked_plan_source and not a.runtime_overlay
        checkedPath=a.checked_plan_evidence.resolve()/'results.json';checked=json.loads(checkedPath.read_text())
        assert checked['complete'] and checked['passed'];pinned.add(checkedPath)
        checkedSource=a.checked_plan_source.resolve();manifest=checkedSource/'manifest.json'
        sha256=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
        assert all(sha256(checkedSource/row['path'])==row['sha256'] for row in json.loads(manifest.read_text()))
        pinned.update([manifest,*[checkedSource/row['path'] for row in json.loads(manifest.read_text())]])
        for path,value in checked['inputs'].items():
            actual=Path(path)
            if not actual.is_file() or sha256(actual).lower()!=value.lower():
                assert actual.is_relative_to(root),path
                actual=checkedSource/actual.relative_to(root)
                assert actual.is_file() and sha256(actual).lower()==value.lower(),path
                checkedRemaps[path]=str(actual)
            pinned.add(actual)
        for level in ('O0','O2'):
            command=next(row['command'] for row in checked['rows'] if row['name']=='xlir_ownership_checked_drop_runtime.'+level+'.build')
            checkedObjects[level]=[Path(v) for v in command if str(v).endswith('.o') and Path(v).name!='xrt_single_header_impl.o']
            pinned.update(checkedObjects[level])
    if a.runtime_overlay:pinned.update([*[root/'src/runtime'/(n+'.c') for n in names],root/'src/api/xl_foreign_error.h',
        *[path for path in (root/'src/runtime').glob('*.h')]])
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();inputs={str(p):sha(p) for p in pinned}
    out.mkdir(parents=True,exist_ok=False)
    if a.runtime_overlay or checked:
        headers=out/'sdk/__xl_runtime';headers.mkdir(parents=True)
        headerRoot=checkedSource or root
        for path in (headerRoot/'src/runtime').glob('*.h'):shutil.copy2(path,headers/path.name)
        shutil.copy2(headerRoot/'src/api/xl_foreign_error.h',headers/'xl_foreign_error.h')
    env=dict(os.environ)
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name);shutil.copy2(engine,out/engine.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+str(engine.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(scope=__doc__,complete=False,passed=False,inputs=inputs,rows=[],runtimeOverlay=list(names) if a.runtime_overlay else [],
        checkedPlanEvidence=str(a.checked_plan_evidence) if checked else None,
        historicalInputPathRemaps=checkedRemaps,
        checkedPlanPartialOverlay=bool(checked),fullCompilerRebuild=False,
        knownFailureReproduced=a.case=='proxy_affinity' and not a.affinity_guarded,
        productionAcceptance=False,
        sourceCohort=str(public/'build.json'),testCase=a.case)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        actual=command_with_response(command,cc,out/(name+'.rsp'));start=time.monotonic()
        r=subprocess.run(list(map(str,actual)),capture_output=True,env=env,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log
    objects=[];overlays={}
    for row in build['Objects']:
        name=Path(row['Source']).stem
        if name in ('xlir_public_source_driver','xlir_public_source_linux_driver'):continue
        if checked and any(value.name==name+'.O0.o' for value in checkedObjects['O0']):continue
        if a.runtime_overlay and name in names:overlays[name]=row;continue
        objects.append(row['Path'])
    include=[*(['-I'+str(checkedSource),'-I'+str(checkedSource/'release/win_x64/include')] if checked else []),
        *(['-I'+str(out/'sdk')] if a.runtime_overlay or checked else []),
        '-I'+str(root if a.runtime_overlay else cohort),'-I'+str(sdk),'-I'+str(sdk/'include'),
        '-I'+str(public/'include'),'-I'+str(native/'include')]
    for name in names if a.runtime_overlay else ():
        row=overlays[name];flags=row.get('Flags',build.get('CompileFlags'));assert flags
        flags=[x for x in flags if not x.startswith('-I')]+include
        dest=out/(name+'.o');run(name+'-build',[cc,*flags,'-c',root/'src/runtime'/(name+'.c'),'-o',dest]);objects.append(dest)
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine.parent)]
    for level in ('O0','O2'):
        program=out/(level+('.exe' if os.name=='nt' else ''))
        run(level+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-'+level,*include,source,native/'src/runtime.c',*objects,*checkedObjects.get(level,()),runtime,engine,*systems,'-o',program])
        run(level+'-run',[program,*([a.font.resolve()] if a.font else []),*(['guarded'] if a.affinity_guarded else [])])
    report.update(complete=True,passed=all(sha(path)==value for path,value in inputs.items()));save();assert report['passed']
if __name__=='__main__':main()
