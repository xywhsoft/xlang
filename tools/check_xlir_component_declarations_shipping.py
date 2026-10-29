"""Resident declarations selected by component ownership, on actual products.

Use only public API/CLI and unmodified packed headers, no source-object
replacement. Does not certify the complete five-group release by itself.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from gen_tcc_vfs_resources import load_resources

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();assert not out.exists();out.mkdir(parents=True)
    build=json.loads(a.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    controls=Path(__file__).resolve().parents[1];windows=os.name=='nt';cc=Path(shutil.which('gcc')).resolve()
    consumer=root/'src/cgen/xl6_ir_cgen_needs_call_targets.c'
    current=controls/'src/cgen/xl6_ir_cgen_needs_call_targets.c'
    text=consumer.read_text(encoding='utf8')
    assert text==current.read_text(encoding='utf8')
    assert 'strcmp(target->cName' not in text and 'owner == XL6_ARTIFACT_DICT_METHODS' in text
    names=('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so')
    original=[root/'release'/name for name in names];recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    for path in original:assert recorded[path]==sha(path)
    isolated=out/'isolated';isolated.mkdir();work=out/'empty-work';work.mkdir();inputs=out/'inputs';inputs.mkdir()
    for path in original:shutil.copy2(path,isolated/path.name)
    source=controls/'test/cases/xlir_shipping_api_host.c';host=isolated/('api-host'+('.exe' if windows else ''))
    cases=[controls/'test/cases'/name for name in ('xlir_component_declarations.xl','xlir_component_declarations_absent.xl')]
    manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
    directory=root/('build/tmp' if windows else 'build/linux');dist=root/'res/tcc/win64/x86_64' if windows else directory/'tcc-dist'
    resources=load_resources(manifest,root,dist,directory/'xrt.o',directory)
    sdk=out/'strict-sdk';declarations=[row for row in resources if row.virtual.startswith('include/__xl_runtime/')]
    for row in declarations:
        destination=sdk/Path(row.virtual).relative_to('include');destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(row.source,destination)
    pinned={Path(__file__).resolve(),Path(sys.executable),Path(load_resources.__code__.co_filename).resolve(),
        cc,a.build_report.resolve(),source,manifest,consumer,current,*original,*cases,*[row.source for row in declarations]}
    r=dict(Complete=False,Inputs={str(q):sha(q) for q in sorted(pinned)},Rows=[],Failures=[],
        ActualShippingLibraries=True,SourceObjectOverrides=False,FullFiveGroupsAccepted=False)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
        ('CPATH','C_INCLUDE_PATH','LIBRARY_PATH','LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT')}
    env.update(TMPDIR=str(out),TEMP=str(out),TMP=str(out))
    def save():(out/'results.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf8')
    def run(name,args,expected=0):
        args=list(map(str,args));log=out/(name+'.log');start=time.monotonic()
        with log.open('wb') as stream:code=subprocess.run(args,cwd=work,env=env,stdout=stream,stderr=subprocess.STDOUT,timeout=300).returncode
        r['Rows'].append(dict(Name=name,Command=args,Exit=code,Expected=expected,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-start));save();print(name,code,flush=True)
        assert code==expected,str(log);return log.read_text(encoding='utf8',errors='replace')
    # The compiler is a complete runtime-capable library. Bind the API host to
    # its context/VFS cohort, not a runtime-only library that also exports the
    # common context names and may preempt them under ELF symbol interposition.
    libraries=[isolated/names[2]]
    run('public-host-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'release/include'),source,*libraries,
        *([] if windows else ['-Wl,-rpath,$ORIGIN']),'-o',host])
    flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter',
        '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-unused-const-variable','-Wno-missing-field-initializers','-I'+str(sdk)]
    for index,case in enumerate(cases):
        try:
            copied=inputs/case.name;shutil.copy2(case,copied);name=case.stem;generated=out/(name+'.c')
            for mode in ('run','module','typed','lowered','cgen'):
                target=generated if mode=='cgen' else out/(name+'.'+mode)
                run(name+'.'+mode,[host,copied,mode,target])
            text=generated.read_text(encoding='utf8')
            assert ('#include <__xl_runtime/xl6_dict_methods.h>' in text)==(index==0)
            assert not re.search(r'^\s*(?:static\s+)?(?:[\w*]+\s+)+xl6DictValueRemoveV1\([^;]*\)\s*\{',text,re.M)
            for opt in ('O0','O2'):run(name+'.strict.'+opt,[cc,*flags,'-'+opt,'-fsyntax-only',generated])
            binary=out/(name+('.exe' if windows else '.aot'))
            # The public API writes the exact requested path on both systems;
            # unlike some command-line drivers, it does not append .exe.
            run(name+'.aot-build',[host,copied,'exe',binary]);run(name+'.aot-run',[binary])
            imports=run(name+'.imports',['E:/software/w64devkit/bin/objdump.exe','-p',binary] if windows else ['readelf','-d',binary])
            assert not any(q in imports.lower() for q in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
        except Exception as error:r['Failures'].append(dict(Name=case.stem,Error=repr(error)));save()
    r['InputsUnchanged']=all(sha(q)==h for q,h in r['Inputs'].items());r['Complete']=r['InputsUnchanged'] and not r['Failures']
    r['Products']=[dict(Path=str(q),Sha256=sha(q)) for q in out.rglob('*') if q.is_file() and q.name!='results.json'];save();assert r['Complete'],r['Failures']
if __name__=='__main__':main()
