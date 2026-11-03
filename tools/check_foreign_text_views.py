"""Offline length-preserving C text views, owned results and sealed XLIR."""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,time
from compiler_response import command_with_response
from xlir_public_build_inputs import verify_public_build

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('root','public-directory','out'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root,public,out=[v.resolve() for v in (a.root,a.public_directory,a.out)]
    build,pinned,_=verify_public_build(root,public);driver=Path(build['Driver']);cc=shutil.which('gcc');assert cc
    source=root/'test/cases/foreign_text_view.xl';provider=root/'test/cases/foreign_text_view_native.c';contract=root/'test/cases/foreign_text_view_ir.c'
    pinned.update((Path(__file__).resolve(),source,provider,contract,root/'test/cases/foreign_text_view.h',Path(cc)))
    sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    inputs={str(path):sha(path) for path in pinned};out.mkdir(parents=True,exist_ok=False);(out/'tmp').mkdir()
    env=dict(os.environ,XL6_SOURCE_NATIVE_ROOT=str(root),TEMP=str(out/'tmp'),TMP=str(out/'tmp'),TMPDIR=str(out/'tmp'))
    report=dict(complete=False,passed=False,scope=__doc__,networkTests=False,sourceBuild=str(public/'build.json'),inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    def run(name,command,accepted=True):
        cmd=command_with_response(command,cc,out/(name+'.rsp'))
        started=time.monotonic();r=subprocess.run(list(map(str,cmd)),cwd=root,env=env,capture_output=True,timeout=240)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,exitCode=r.returncode,accepted=accepted,seconds=round(time.monotonic()-started,3),logSha256=sha(log)))
        save();print(name,r.returncode,flush=True);assert (r.returncode==0)==accepted,log
    generated=out/'text.c';run('cgen',[driver,source,'cgen-default',generated]);text=generated.read_text(encoding='utf-8-sig')
    assert 'extern size_t foreign_text_size(xstrview);' in text and 'extern xstrview foreign_text_echo(xstrview);' in text
    runtime_dir=Path(build.get('RuntimeDirectory',root/('release/win_x64' if os.name=='nt' else 'release/linux_x64')))
    runtime=runtime_dir/('xl.dll' if os.name=='nt' else 'libxl.so')
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    includes=['-I'+str(root),'-I'+str(runtime_dir/'include'),'-I'+str(public/'include'),'-I'+str(runtime_dir)]
    warnings=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable']
    suffix='.exe' if os.name=='nt' else '.out'
    run('emit-header',[driver,source,'emit',out/'header',provider])
    run('header-C-ABI',[cc,'-std=c11','-Wall','-Wextra','-Werror','-x','c','-fsyntax-only',*includes,out/'header.h'])
    for opt in ('O0','O2'):
        exe=out/('text-'+opt+suffix)
        run(opt+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-'+opt,*warnings,*includes,generated,provider,root/'test/cases/foreign_decl_module_host.c',runtime,'-o',exe])
        run(opt+'-run',[exe])
    for mode in ('run','module','exe'):
        prefix=out/('tcc-'+mode);run('tcc-'+mode,[driver,source,mode,prefix,provider])
        if mode=='exe':run('standalone',[prefix.with_suffix('.exe') if os.name=='nt' else prefix])
    original=source.read_text(encoding='utf-8-sig')
    negatives={
        'duplicate':original.replace('@c_text_view(0)','@c_text_view(0) @c_text_view(0)',1),
        'bad-index':original.replace('@c_text_view(0)','@c_text_view(63)',1),
        'negative-index':original.replace('@c_text_view(0)','@c_text_view(-1)',1),
        'conflict':original.replace('@c_text_view(0)','@c_text_view(0) @c_ref(0)',1),
        'nullable-input':original.replace('textSize(string value)','textSize(string? value)'),
        'bytes-input':original.replace('textSize(string value)','textSize(bytes value)'),
        'wrong-result':original.replace('textResult(c_int mode) as string','textResult(c_int mode) as int'),
        'owned-result':original.replace('@c_result("text_view") func textResult','@owned @c_result("text_view") func textResult'),
        'unknown-result':original.replace('@c_result("text_view")','@c_result("unknown")',1),
        'duplicate-result':original.replace('@c_result("text_view")','@c_result("text_view") @c_result("text_view")',1)}
    for name,content in negatives.items():
        case=out/(name+'.xl');case.write_text(content);target=out/(name+'.c')
        run(name,[driver,case,'cgen-default',target],False);assert not target.exists()
    objects=[row['Path'] for row in build['Objects'] if Path(row['Source']).name not in ('xlir_public_source_driver.c','xlir_public_source_linux_driver.c')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if os.name=='nt' else ['-Wl,--export-dynamic','-pthread','-ldl','-lm','-Wl,-rpath,'+str(runtime.parent)]
    for opt in ('O0','O2'):
        exe=out/('IR-'+opt+suffix)
        run('IR-'+opt+'-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-'+opt,*includes,contract,*objects,runtime,*systems,'-o',exe])
        run('IR-'+opt+'-run',[exe,source])
    verify_public_build(root,public);report['inputsUnchanged']=all(sha(path)==value for path,value in inputs.items())
    report['complete']=True;report['passed']=report['inputsUnchanged'];save();assert report['passed']
if __name__=='__main__':main()
