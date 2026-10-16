"""Exercise process.open through genuine products and real OS launch routes.
Windows ShellExecute launches a headless controlled executable; Linux executes
a private xdg-open external launcher and verifies the target's exact argv.
No XRT/provider substitution, registry changes, desktop application or GUI.
Acceptance is not application completion or arbitrary desktop association proof.
"""
from pathlib import Path
import argparse, hashlib, json, os, re, shutil, subprocess, sys, time


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();out=args.out.resolve();assert not out.exists()
    task=Path(__file__).resolve().parents[1];win=os.name=='nt'
    cc=shutil.which('gcc');inspector=shutil.which('objdump' if win else 'readelf');assert cc and inspector
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    products=[root/'release'/name for name in (('xl.exe','xl.dll','xlc.dll') if win else ('xl','libxl.so','libxlc.so'))]
    recorded={Path(r['Path']).resolve():r['Sha256'].lower() for r in build['Products']}
    assert all(recorded.get(p)==sha(p) for p in products)
    source=task/'test/cases/stdlib_association_product.xl'
    child_source=task/'test/cases/stdlib_association_child.c'
    host=task/'test/cases/stdlib_internal_container_product_host.c'
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),Path(inspector),args.build_report.resolve(),source,child_source,host}
    for row in build['Inputs']+build['Products']:
        assert sha(row['Path'])==row['Sha256'].lower(),row['Path'];files.add(Path(row['Path']))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();isolated=out/'isolated';release=isolated/'release';release.mkdir(parents=True)
    sdk=out/'sdk';sdk.mkdir();moved=out/'relocated';moved.mkdir();private_bin=out/'launcher';private_bin.mkdir()
    for product in products:
        shutil.copy2(product,release/product.name)
        if win and product.suffix=='.dll':shutil.copy2(product,isolated/product.name)
    for header in (root/'release/include').glob('*.h'):shutil.copy2(header,sdk/header.name)
    sys.path.insert(0,str(root/'tools'));from gen_tcc_vfs_resources import load_resources
    bd=root/('build/tmp' if win else 'build/linux')
    manifest=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if win else 'tcc_vfs_manifest_linux_runtime.txt')
    dist=root/'res/tcc/win64/x86_64' if win else bd/'tcc-dist'
    for resource in load_resources(manifest,root,dist,bd/'xrt.o',bd):
        if resource.virtual.startswith('include/__xl_runtime/'):
            target=sdk/Path(resource.virtual).relative_to('include');target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(resource.source,target)
    shutil.copy2(root/'lib/xruntime/xruntime.h',sdk/'xruntime.h')
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k not in
         ('LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','CPATH','C_INCLUDE_PATH','LIBRARY_PATH')}
    env['PATH']=str(private_bin)+os.pathsep+env.get('PATH','')
    child=private_bin/('open 你 probe.exe' if win else 'xdg-open')
    target=str(child) if win else 'xlang-probe://你 path/a;$(not-a-command)&\'literal"'
    env['XL6_ASSOCIATION_TARGET']=target
    rows=[];failures=[];proofs=[]
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,Failures=failures,Proofs=proofs,
                Scope=__doc__,ProviderOverrides=False,CompilerSourceObjects=False,Full53Acceptance=False,
                RealPlatformLaunchAcceptance=False,DesktopAssociationAcceptance=False)
    def run(name,command,environment=env):
        command=list(map(str,command));result=subprocess.run(command,cwd=isolated,env=environment,capture_output=True,timeout=120)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        rows.append(dict(Name=name,Command=command,Exit=result.returncode,Log=str(log),LogSha256=sha(log)))
        return result.returncode,(result.stdout+result.stderr).decode(errors='replace')
    def ok(name,command,environment=env):
        code,text=run(name,command,environment);assert code==0,(name,code,text[-6000:]);return text
    def marker(name):
        path=out/(name+'.marker');assert not path.exists();env['XL6_ASSOCIATION_MARKER']=str(path);return path
    def observed(path):
        deadline=time.monotonic()+10
        while time.monotonic()<deadline:
            if path.exists() and path.read_bytes()==b'controlled application reached\n':return
            time.sleep(0.01)
        raise AssertionError(('Controlled application did not receive exact target',str(path)))
    try:
        flags=['-std=c11','-Wall','-Wextra','-Werror','-O2']
        ok('child.build',[cc,*flags,*(['-municode','-mwindows'] if win else []),child_source,'-o',child])
        good=marker('child-control');code,_=run('child.control',[child,*([] if win else [target])]);assert code==7;observed(good)
        bad=marker('child-refusal');wrong=dict(env);wrong['XL6_ASSOCIATION_TARGET']='wrong target'
        code,_=run('child.refusal',[child,*([] if win else [target])],wrong);assert code==17 and not bad.exists()
        observer=isolated/('observer.exe' if win else 'observer');compiler=release/products[2].name
        links=[compiler] if win else ['-L'+str(release),'-lxlc','-Wl,-rpath,$ORIGIN/release','-pthread','-ldl','-lm']
        ok('observer.build',[cc,*flags,'-DXL_STATIC_EMBED','-I'+str(sdk),host,*links,'-o',observer])
        local=isolated/source.name;shutil.copy2(source,local)
        assert not any((isolated/n).exists() for n in ('src','lib','res','include'))
        for mode in ('run','module','aot'):
            work=out/mode;work.mkdir();image=work/('program.exe' if win else 'program');proof=marker(mode)
            ok(mode,[observer,compiler,local,mode,work,image])
            if mode=='aot':
                relocated=moved/image.name;shutil.move(image,relocated)
                text=ok('aot.imports',[inspector,'-p' if win else '-d',relocated])
                dependencies=re.findall(r'DLL Name:\s*(\S+)',text) if win else re.findall(r'\(NEEDED\).*?\[(.*?)\]',text)
                assert not any('libxl' in n.lower() or n.lower() in ('xl.dll','xlc.dll') for n in dependencies)
                assert win or not re.search(r'\((?:RPATH|RUNPATH)\)',text)
                ok('aot.relocated',[relocated])
            observed(proof);proofs.append(dict(Mode=mode,Marker=str(proof),Sha256=sha(proof),ExactTarget=True))
        compat=['-Wno-unused-function','-Wno-unused-variable','-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-const-variable','-Wno-unused-but-set-variable','-Wno-missing-field-initializers','-Wno-pointer-sign']
        for opt in ('O0','O2'):
            for name in ('module.c','xl6_xrt_impl.c'):
                ok(opt+'.'+name,[cc,'-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*([] if win else ['-D_GNU_SOURCE']),'-I'+str(sdk),*compat,'-'+opt,'-c',work/name,'-o',out/(opt+'.'+name+'.o')])
        report['RealPlatformLaunchAcceptance']=len(proofs)==3
        report['InputsUnchanged']=all(sha(r['Path'])==r['Sha256'] for r in inputs)
        report['Complete']=report['InputsUnchanged'] and report['RealPlatformLaunchAcceptance']
    except Exception as error:failures.append(repr(error))
    report['Products']=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(out.rglob('*')) if f.is_file()]
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Failures=failures,Phases=len(rows)),ensure_ascii=False),flush=True)
    return 0 if report['Complete'] else 1


if __name__=='__main__':raise SystemExit(main())
