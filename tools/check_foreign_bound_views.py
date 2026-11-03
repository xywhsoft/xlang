"""Offline typed C-view method values and exact receiver/invocation XLIR."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from xlir_public_build_inputs import verify_public_build
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','public-directory','out'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root,public,out=[v.resolve() for v in (a.root,a.public_directory,a.out)]
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver']);cc=shutil.which('gcc');assert cc
    source=root/'test/cases/foreign_bound_view.xl';provider=root/'test/cases/foreign_bound_view_native.c';contract=root/'test/cases/foreign_bound_view_ir.c'
    positive=sorted((root/'test/cases').glob('extern_owned*bound*.xl'))
    negative=sorted((root/'test/cases').glob('bad_extern_owned_bound*.xl'))
    pinned.update((Path(__file__).resolve(),source,provider,contract,root/'test/cases/foreign_bound_view.h',Path(cc),*positive,*negative))
    pinned.update((root/'test/cases/xlir_bound_dispatch_contract.c',root/'test/cases/xlir_owner_consumption_contract.c'))
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs={str(path):sha(path) for path in pinned};out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(root),TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    report=dict(complete=False,passed=False,scope=__doc__,networkTests=False,sourceBuild=str(public/'build.json'),inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,accepted=True):
        cmd=command_with_response(command,cc,out/(name+'.rsp'));started=time.monotonic()
        r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,exitCode=r.returncode,accepted=accepted,seconds=round(time.monotonic()-started,3),logSha256=sha(log)))
        save();print(name,r.returncode,flush=True)
        assert r.returncode==(0 if accepted else 1),log
        if not accepted:assert b'public source' in r.stderr and b'failed' in r.stderr,log
    generated=out/'bound.c';run('cgen',[driver,source,'cgen-default',generated])
    runtime_dir=Path(build.get('RuntimeDirectory',root/('release/win_x64' if os.name=='nt' else 'release/linux_x64')))
    runtime=runtime_dir/('xl.dll' if os.name=='nt' else 'libxl.so')
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    includes=['-I'+str(root),'-I'+str(runtime_dir/'include'),'-I'+str(public/'include'),'-I'+str(runtime_dir)]
    warnings=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable']
    suffix='.exe' if os.name=='nt' else '.out'
    run('emit-header',[driver,source,'emit',out/'header',provider])
    run('header-C-ABI',[cc,'-std=c11','-Wall','-Wextra','-Werror','-x','c','-fsyntax-only',*includes,out/'header.h'])
    for opt in ('O0','O2'):
        exe=out/('bound-'+opt+suffix)
        run(opt+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,*warnings,*includes,generated,provider,root/'test/cases/foreign_decl_module_host.c',runtime,'-o',exe]);run(opt+'-run',[exe])
    for mode in ('run','module','exe'):
        prefix=out/('tcc-'+mode);run('tcc-'+mode,[driver,source,mode,prefix,provider])
        if mode=='exe':run('standalone',[prefix.with_suffix('.exe') if os.name=='nt' else prefix])
    original=source.read_text(encoding='utf-8-sig')
    bad={
        'wrong-input':original.replace('func(int) as int add = first.add','func(string) as int add = first.add'),
        'wrong-result':original.replace('func(int) as int add = first.add','func(int) as string add = first.add'),
        'nullable-not-narrowed':original.replace('View first = view(c_int(0))','View? first = null'),
        'missing-caller-contract':original.replace(' @lifetime("caller")',''),
        'static-through-instance':original.replace('func add(int amount = 2)','static func add(int amount = 2)')}
    for name,content in bad.items():
        case=out/(name+'.xl');case.write_text(content);target=out/(name+'.c')
        run(name,[driver,case,'cgen-default',target],False);assert not target.exists()
    for case in positive:run('regression-'+case.stem,[driver,case,'cgen-default',out/(case.stem+'.c')])
    for case in negative:
        target=out/(case.stem+'.c');run('reject-'+case.stem,[driver,case,'cgen-default',target],False);assert not target.exists()
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for opt in ('O0','O2'):
        for label,unit,args in (('receiver',contract,[source]),('dispatch',root/'test/cases/xlir_bound_dispatch_contract.c',[])):
            exe=out/(label+'-IR-'+opt+suffix)
            run(label+'-IR-'+opt+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-'+opt,*includes,unit,*objects,runtime,*systems,'-o',exe])
            run(label+'-IR-'+opt+'-run',[exe,*args])
    verify_public_build(root,public);report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
    report.update(complete=True,passed=report['inputsUnchanged'],existingPositiveCases=len(positive),existingNegativeCases=len(negative));save();assert report['passed']
if __name__=='__main__':main()
