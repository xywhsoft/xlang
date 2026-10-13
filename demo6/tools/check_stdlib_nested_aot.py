"""Real rebuilt CLI/VFS and compiler-import AOT in source-free directories.
Uses both actual shared libraries and their generated resources. Native-system
libc/CRT/linker inputs remain required on Linux; no full53 acceptance claim.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess
from gen_tcc_vfs_resources import Resource,load_resources

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def verify_compiler_source_inventory(root):
    api=root/'src/api/xl_api.c';normal=root/'tools/build_xlir_public_source_driver.ps1'
    block=re.search(r'static int xlAddEmbeddedXlangRuntimeSources\b.*?unsigned int features;',api.read_text(encoding='utf-8'),re.S)
    assert block,'Embedded compiler source inventory missing'
    embedded=set(re.findall(r'"(src/[^" ]+\.c)"',block.group()))
    body=normal.read_text(encoding='utf-8-sig').split('$sources = @(',1)[1].split('$files =',1)[0]
    core=set(re.findall(r"'(src/(?:abi|ast|backend|common|compile|diag|ir|lexer|parser|sem|stdlib|type|package|resource)/[^']+\.c)'",body))
    assert core and core.issubset(embedded),sorted(core-embedded)
    assert all((root/p).is_file() for p in embedded)
    return sorted(core),{api,normal}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve()
    assert not out.exists();windows=os.name=='nt';cc=shutil.which('gcc');assert cc
    project=Path(__file__).resolve().parents[1]
    core,core_inputs=verify_compiler_source_inventory(root)
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged']
    products=[root/'release'/name for name in (('xl.exe','xlc.dll','xl.dll') if windows else ('xl','libxlc.so','libxl.so'))]
    assert all(p.is_file() for p in products)
    cases=[project/'test/cases'/name for name in ('stdlib_nested_sdk.xl','stdlib_string_bridge_boundaries.xl',
        'stdlib_nested_aot.xl','stdlib_future_metadata.xl')]
    host=project/'test/cases/stdlib_nested_sdk_host.c'
    build_directory=root/('build/tmp' if windows else 'build/linux')
    tcc_dist=root/'res/tcc/win64/x86_64' if windows else build_directory/'tcc-dist'
    manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
    resources=load_resources(manifest,root,tcc_dist,build_directory/'xrt.o',build_directory)
    declarations=[r for r in resources if r.virtual.startswith('include/__xl_runtime/')]
    declarations.extend(Resource('include/'+p.name,p) for p in sorted((root/'release/include').glob('xl*.h')))
    pinned={Path(__file__).resolve(),Path(cc),args.build_report.resolve(),host,manifest,*cases,*products}
    pinned.update(core_inputs)
    pinned.update(Path(r['Path']) for r in build['Inputs']+build['Products'])
    pinned.update(r.source for r in declarations)
    for p in (root/'src/runtime').glob('*.h'):pinned.add(p)
    inputs=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(pinned)]
    for row in build['Inputs']+build['Products']:assert sha(row['Path'])==row['Sha256'],row['Path']
    out.mkdir();isolated=out/'isolated';isolated.mkdir();relocated=out/'relocated';relocated.mkdir()
    work=out/'empty-work';work.mkdir();strict_sdk=out/'strict-sdk'
    for p in products:shutil.copy2(p,isolated/p.name)
    for r in declarations:
        target=strict_sdk/Path(r.virtual).relative_to('include')
        target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(r.source,target)
        assert sha(target)==sha(r.source)
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
        ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env.update(XL6_NESTED_SDK_WORK=str(work),TMPDIR=str(out),TEMP=str(out),TMP=str(out))
    cli=isolated/products[0].name;rows=[];outputs=[];failures=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Products=outputs,Failures=failures,Scope=__doc__,
        SourceFreeCli=True,SourceFreeAot=True,ExternalTccDistributionUsed=False,
        TargetSystemLibcHeadersCrtAndLinkerRequired=not windows,Full53Acceptance=False,CompilerCoreSources=core)
    def product(p):outputs.append(dict(Path=str(p),Sha256=sha(p)))
    def run(name,command,cwd=isolated):
        result=subprocess.run(list(map(str,command)),cwd=cwd,env=env,capture_output=True,timeout=300)
        data=result.stdout+result.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=result.returncode,Log=str(log),LogSha256=sha(log)))
        assert result.returncode==0,(name,data[-8000:])
        return data
    for p in products:
        copied=isolated/p.name;assert sha(copied)==sha(p);product(copied)
    def check_case(case):
        local=isolated/case.name;shutil.copy2(case,local);product(local)
        run(case.stem+'-jit',[cli,'run',local.name])
        generated=out/(case.stem+'.c')
        run(case.stem+'-cgen',[cli,'cgen',local.name,'--emit',generated]);product(generated)
        flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(strict_sdk),
            '-Wno-unused-function','-Wno-unused-variable','-Wno-unused-const-variable','-Wno-unused-parameter',
            '-Wno-unused-label','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
        if not windows:flags+=['-D_GNU_SOURCE']
        for level in ('O0','O2'):
            obj=out/(case.stem+'-'+level+'.o')
            run(case.stem+'-'+level,[cc,*flags,'-'+level,'-c',generated,'-o',obj]);product(obj)
        binary=isolated/(case.stem+'.exe')
        run(case.stem+'-aot-build',[cli,'build',local.name,'-o',binary]);product(binary)
        run(case.stem+'-aot-run',[binary],work)
        independent=relocated/binary.name;shutil.copy2(binary,independent);product(independent)
        run(case.stem+'-aot-relocated',[independent],relocated)
    try:
        for case in cases:
            try:check_case(case)
            except AssertionError as error:failures.append(dict(Case=case.stem,Diagnostic=str(error)))
        for original in products[1:]:
            host_binary=isolated/(original.stem+'-default-host'+('.exe' if windows else ''))
            links=[isolated/original.name] if windows else ['-L'+str(isolated),'-l'+original.stem[3:],
                '-Wl,-rpath,$ORIGIN','-pthread','-ldl','-lm']
            flags=['-std=c11','-Wall','-Wextra','-Werror','-O2','-DXL_STATIC_EMBED','-I'+str(root/'release')]
            if not windows:flags+=['-D_GNU_SOURCE']
            run(original.stem+'-host-build',[cc,*flags,host,*links,'-o',host_binary]);product(host_binary)
            run(original.stem+'-host-run',[host_binary,work])
        assert not any((isolated/n).exists() for n in ('src','lib','res','include'))
        assert {p.suffix for p in relocated.iterdir()}=={'.exe'}
        assert all(sha(isolated/p.name)==sha(p) for p in products)
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        assert report['InputsUnchanged'];assert not failures,failures;report['Complete']=True
    finally:
        for p in strict_sdk.rglob('*'):
            if p.is_file():product(p)
        report['PhaseCount']=len(rows)
        (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Real nested compiler CLI/VFS/AOT: PASS',flush=True)

if __name__=='__main__':main()
