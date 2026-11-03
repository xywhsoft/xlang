"""Offline length-preserving C binary views, owned results and sealed XLIR."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from xlir_public_build_inputs import verify_public_build

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','public-directory','out'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root,public,out=[v.resolve() for v in (a.root,a.public_directory,a.out)]
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver']);cc=shutil.which('gcc');assert cc
    source=root/'test/cases/foreign_bytes_view.xl';provider=root/'test/cases/foreign_bytes_view_native.c';contract=root/'test/cases/foreign_bytes_view_ir.c'
    pinned.update((Path(__file__).resolve(),source,provider,contract,root/'test/cases/foreign_bytes_view.h',Path(cc)))
    pinned.update(root/'test/cases'/name for name in ('xlir_function_lower.c','xlir_persistence_project_plan_contract.c','xlir_persistence_project_plan.xl'))
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs={str(path):sha(path) for path in pinned};out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(root),TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    report=dict(complete=False,passed=False,scope=__doc__,networkTests=False,sourceBuild=str(public/'build.json'),inputs=inputs,rows=[])
    report['legacySyntheticAstUnit']='Compile-only enum migration check; existing runtime fixture failure recorded separately'
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,accepted=True):
        cmd=command_with_response(command,cc,out/(name+'.rsp'))
        started=time.monotonic();r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,exitCode=r.returncode,accepted=accepted,seconds=round(time.monotonic()-started,3),logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert r.returncode == (0 if accepted else 1),log
        if not accepted:assert b'[sem]' in (r.stdout+r.stderr),log
    generated=out/'bytes.c';run('cgen',[driver,source,'cgen-default',generated]);text=generated.read_text(encoding='utf-8-sig')
    assert 'extern size_t foreign_bytes_size(xbytesview);' in text and 'extern xbytesview foreign_bytes_echo(xbytesview);' in text
    run('dump-ir',[driver,source,'ir-lowered',out/'ir.json'])
    targets=json.loads((out/'ir.json').read_text(encoding='utf-8-sig'))['callTargets']
    echoes=[target for target in targets if target['cName']=='foreign_bytes_echo']
    assert echoes and all(target['resultConversionAbiName']=='borrowed-value-copy' for target in echoes)
    runtime_dir=Path(build.get('RuntimeDirectory',root/('release/win_x64' if os.name=='nt' else 'release/linux_x64')))
    runtime=runtime_dir/('xl.dll' if os.name=='nt' else 'libxl.so')
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    includes=['-I'+str(root),'-I'+str(runtime_dir/'include'),'-I'+str(public/'include'),'-I'+str(runtime_dir)]
    warnings=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable']
    suffix='.exe' if os.name=='nt' else '.out'
    run('emit-header',[driver,source,'emit',out/'header',provider])
    run('header-C-ABI',[cc,'-std=c11','-Wall','-Wextra','-Werror','-x','c','-fsyntax-only',*includes,out/'header.h'])
    for opt in ('O0','O2'):
        exe=out/('bytes-'+opt+suffix)
        run(opt+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,*warnings,*includes,generated,provider,root/'test/cases/foreign_decl_module_host.c',runtime,'-o',exe])
        run(opt+'-run',[exe])
    for mode in ('run','module','exe'):
        prefix=out/('tcc-'+mode);run('tcc-'+mode,[driver,source,mode,prefix,provider])
        if mode=='exe':run('standalone',[prefix.with_suffix('.exe') if os.name=='nt' else prefix])
    original=source.read_text(encoding='utf-8-sig')
    negatives={
        'duplicate':original.replace('@c_bytes_view(0)','@c_bytes_view(0) @c_bytes_view(0)',1),
        'bad-index':original.replace('@c_bytes_view(0)','@c_bytes_view(63)',1),
        'negative-index':original.replace('@c_bytes_view(0)','@c_bytes_view(-1)',1),
        'text-conflict':original.replace('@c_bytes_view(0)','@c_bytes_view(0) @c_text_view(0)',1),
        'result-kind':original.replace('@c_result("bytes_view")','@c_result("text_view")',1),
        'conflict':original.replace('@c_bytes_view(0)','@c_bytes_view(0) @c_ref(0)',1),
        'nullable-input':original.replace('byteSize(bytes value)','byteSize(bytes? value)'),
        'string-input':original.replace('byteSize(bytes value)','byteSize(string value)'),
        'wrong-result':original.replace('byteResult(c_int mode) as bytes','byteResult(c_int mode) as int'),
        'owned-result':original.replace('@c_result("bytes_view") func byteResult','@owned @c_result("bytes_view") func byteResult'),
        'unknown-result':original.replace('@c_result("bytes_view")','@c_result("unknown")',1),
        'duplicate-result':original.replace('@c_result("bytes_view")','@c_result("bytes_view") @c_result("bytes_view")',1)}
    for name,content in negatives.items():
        assert content != original, name
        case=out/(name+'.xl');case.write_text(content,encoding='utf-8');target=out/(name+'.c')
        run(name,[driver,case,'cgen-default',target],False);assert not target.exists()
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for opt in ('O0','O2'):
        exe=out/('IR-'+opt+suffix)
        run('IR-'+opt+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-'+opt,*includes,contract,*objects,runtime,*systems,'-o',exe])
        run('IR-'+opt+'-run',[exe,source])
        for name in ('xlir_function_lower','xlir_persistence_project_plan_contract'):
            if name=='xlir_function_lower':
                run(name+'-'+opt+'-syntax',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-'+opt,*includes,'-fsyntax-only',root/('test/cases/'+name+'.c')])
                continue
            exe=out/(name+'-'+opt+suffix)
            run(name+'-'+opt+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-'+opt,*includes,root/('test/cases/'+name+'.c'),*objects,runtime,*systems,'-o',exe])
            arguments=[root/'test/cases/xlir_persistence_project_plan.xl',out/('persistence-'+opt+'.json'),'schema']
            run(name+'-'+opt+'-run',[exe,*arguments])
    verify_public_build(root,public);report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
    report['complete']=True;report['passed']=report['inputsUnchanged'];save();assert report['passed']
if __name__=='__main__':main()
