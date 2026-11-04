"""Exercise real package Frame declarations with no window, GPU or network."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys,time

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','cohort-root','public-directory','packages','out'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--case',choices=('callbacks','terminal','scalar','merge','guard'),default='callbacks')
    a=p.parse_args();root,cohort,public,packages,out=[v.resolve() for v in (a.root,a.cohort_root,a.public_directory,a.packages,a.out)]
    sys.path.insert(0,str(root/'tools'))
    from xlir_public_build_inputs import verify_public_build
    from compiler_response import command_with_response
    build,pinned,_=verify_public_build(cohort,public);cc=shutil.which('gcc');assert cc
    sdk=Path(build.get('RuntimeDirectory',build.get('SdkDirectory','')))
    runtime=sdk/'xl.dll' if os.name=='nt' else Path(build['RuntimeLibrary'])
    native=packages/'_xge_native';engine=native/'native'/('xge.dll' if os.name=='nt' else 'libxge.so')
    fixture=root/('test/cases/foreign_xge3_binding_'+a.case+'.xl')
    package_sources=[packages/'xge/src/callbacks.xl'] if a.case=='callbacks' else [packages/'xui/src'/name for name in ('records.xl','constants.xl','managed.xl','cell.xl')] if a.case in ('terminal','merge','guard') else []
    sources=[native/'src/bootstrap.xl',*package_sources,fixture]
    pinned.update([Path(__file__).resolve(),Path(cc),runtime,*sources,*[q for q in native.rglob('*') if q.is_file()]])
    sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest();inputs={str(q):sha(q) for q in pinned}
    out.mkdir(parents=True,exist_ok=False)
    source=out/'combined.xl'
    # unit is a normal direct function in the exact package function source.
    unit=next(line for line in (packages/'xge/src/functions.xl').read_text().splitlines() if 'func unit(' in line) if a.case=='callbacks' else ''
    if a.case=='callbacks':inputs[str(packages/'xge/src/functions.xl')]=sha(packages/'xge/src/functions.xl')
    source.write_text('\n'.join(q.read_text(encoding='utf8') for q in sources[:-1])+'\n'+unit+'\n'+fixture.read_text(encoding='utf8'),encoding='utf8')
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(cohort));report=dict(scope=__doc__,case=a.case,complete=False,passed=False,inputs=inputs,rows=[],sourceCohort=str(public/'build.json'),networkTests=False,gpuTests=False,desktopTests=False)
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name);shutil.copy2(engine,out/engine.name)
    else:env['LD_LIBRARY_PATH']=str(runtime.parent)+':'+str(engine.parent)+':'+env.get('LD_LIBRARY_PATH','')
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command):
        command=list(map(str,command));actual=command_with_response(command,cc,out/(name+'.rsp')) if command[0]==cc else command
        started=time.monotonic();r=subprocess.run(actual,capture_output=True,env=env,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,exitCode=r.returncode,seconds=round(time.monotonic()-started,3),logSha256=sha(log)));save()
        print(name,r.returncode,flush=True);assert not r.returncode,log
    generated=out/'generated.c';driver=public/('public_source.exe' if os.name=='nt' else 'public_source')
    run('cgen',[driver,source,'cgen-default',generated])
    objects=[q['Path'] for q in build['Objects'] if Path(q['Source']).stem not in ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    fixture_overlay=out/'fixture-inputs/test/cases';fixture_overlay.mkdir(parents=True)
    sidecars=[]
    if a.case in ('scalar','merge'):
        scalar_header=root/('test/cases/foreign_xge3_binding_'+a.case+'.h');shutil.copy2(scalar_header,fixture_overlay/scalar_header.name)
        sidecars=[root/('test/cases/foreign_xge3_binding_'+a.case+'.c')]
        inputs[str(scalar_header)]=sha(scalar_header);inputs.update((str(q),sha(q)) for q in sidecars)
    include=['-I'+str(q) for q in (out/'fixture-inputs',cohort,sdk,sdk/'include',public/'include',native/'include')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent),'-Wl,-rpath,'+str(engine.parent)]
    warnings=['-Wno-'+v for v in ('unused-function','unused-variable','unused-label','unused-const-variable','unused-but-set-variable','cast-function-type')]
    for level in ('O0','O2'):
        program=out/(level+('.exe' if os.name=='nt' else ''))
        run(level+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror',*warnings,'-DXL_STATIC_EMBED','-'+level,*include,generated,*sidecars,native/'src/runtime.c',cohort/'test/cases/foreign_decl_module_host.c',*objects,runtime,engine,*systems,'-o',program])
        run(level+'-run',[program])
    report.update(complete=True,passed=all(sha(q)==h for q,h in inputs.items()),inputsUnchanged=all(sha(q)==h for q,h in inputs.items()));save();assert report['passed']
if __name__=='__main__':main()
