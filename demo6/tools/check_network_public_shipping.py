"""Original public network fixtures on ordinary, source-free CLI/DLL/SO products.

Only numeric address operations, local adapter metadata and self-owned literal
loopback. No public DNS, scans, attack simulation or security campaigns.
This is a network migration product gate, not complete five-group acceptance.
"""
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,threading,time
from gen_tcc_vfs_resources import load_resources

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--manifest',type=Path,help='Explicit ordinary network/TLS cases and .xl support; default network batch is unchanged')
    args=parser.parse_args();root,out=args.root.resolve(),args.out.resolve()
    assert not out.exists() and args.jobs>0
    build=json.loads(args.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    controls=Path(__file__).resolve().parents[1];windows=os.name=='nt'
    names=('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so')
    originals=[root/'release'/name for name in names]
    recorded={Path(row['Path']).resolve():row['Sha256'] for row in build['Products']}
    assert all(recorded.get(path)==sha(path) for path in originals)
    manifest=args.manifest.resolve() if args.manifest else controls/'tools/network_public_fixtures.json'
    selection=json.loads(manifest.read_text(encoding='utf-8-sig'));cases=selection['Cases']
    assert cases and len({r['Name'] for r in cases})==len(cases)
    if not args.manifest:assert len(cases)==18
    support=selection.get('Support',[])
    assert len(support)==len(set(support))
    for row in cases:
        assert re.fullmatch(r'[A-Za-z0-9_]+',row['Name']) and type(row['Negative']) is bool
    for relative in [r['Source'] for r in cases]+support:
        source=(controls/relative).resolve()
        assert source.is_relative_to(controls) and source.is_file() and source.suffix=='.xl'
    cc=Path(shutil.which('gcc')).resolve()
    vfs=root/'tools'/('tcc_vfs_manifest_win64_runtime.txt' if windows else 'tcc_vfs_manifest_linux_runtime.txt')
    directory=root/('build/tmp' if windows else 'build/linux')
    dist=root/'res/tcc/win64/x86_64' if windows else directory/'tcc-dist'
    resources=load_resources(vfs,root,dist,directory/'xrt.o',directory)
    declarations=[r for r in resources if r.virtual.startswith('include/__xl_runtime/')]
    assert declarations and all(r.source.suffix in ('.h','.inc') for r in declarations)
    out.mkdir(parents=True);isolated=out/'isolated';work=out/'empty-work';sources=out/'inputs';sdk=out/'strict-sdk'
    for folder in (isolated,work,sources,sdk):folder.mkdir()
    pinned={Path(__file__).resolve(),Path(sys.executable).resolve(),cc,args.build_report.resolve(),manifest,
            Path(load_resources.__code__.co_filename).resolve(),vfs,*originals,
            controls/'test/cases/xlir_core_guard_source_host.c'}
    for path in originals:shutil.copy2(path,isolated/path.name)
    for r in declarations:
        destination=sdk/Path(r.virtual).relative_to('include');destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(r.source,destination);pinned.update((r.source,destination))
    for relative in sorted({r['Source'] for r in cases}|set(support)):
        original=controls/relative;copied=sources/relative
        copied.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(original,copied);pinned.update((original,copied))
    public_headers=list((root/'release/include').glob('*.h'));pinned.update(public_headers)
    report=dict(Complete=False,Rows=[],Failures=[],Products=[],Proofs=[],Inputs={str(p):sha(p) for p in sorted(pinned)},
        ActualShippingProducts=True,SourceObjectOverrides=False,SourceFreeDeployment=True,FullFiveGroupsAccepted=False,Scope=__doc__)
    environment={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
                 ('CPATH','C_INCLUDE_PATH','LIBRARY_PATH','LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT')}
    environment.update(TMPDIR=str(out),TEMP=str(out),TMP=str(out))
    lock=threading.RLock()
    def save():
        with lock:(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command):
        command=list(map(str,command));log=out/(name+'.log');started=time.monotonic()
        try:
            with log.open('wb') as stream:
                code=subprocess.run(command,cwd=work,env=environment,stdout=stream,stderr=subprocess.STDOUT,timeout=600).returncode
        except subprocess.TimeoutExpired:code='timeout'
        with lock:
            report['Rows'].append(dict(Name=name,Command=command,Exit=code,Log=str(log),Sha256=sha(log),Seconds=time.monotonic()-started));save()
        print(name,code,flush=True);assert code==0,(name,str(log))
        return log.read_text(encoding='utf8',errors='replace')
    save();host=isolated/('network-host.exe' if windows else 'network-host')
    try:
        run('public-host-build',[cc,'-std=c11','-Wall','-Wextra','-Werror','-I'+str(root/'release/include'),
            controls/'test/cases/xlir_core_guard_source_host.c',isolated/names[2],
            *([] if windows else ['-Wl,-rpath,$ORIGIN']),'-o',host])
    except Exception as error:report['Failures'].append(dict(Name='public-host-build',Error=repr(error)));save();return 1
    def check(row):
        name=row['Name'];negative=row['Negative'];directory=out/name;directory.mkdir()
        try:
            text=run(name+'-host',[host,work,sources/row['Source'],directory,sdk,isolated/names[1],'-','-',
                'negative' if negative else 'positive'])
            if negative:
                assert 'core-host: checked-rejection' in text
                assert not (directory/'rejected.c').exists() and not (directory/'rejected.h').exists()
            else:
                for marker in ('load','compile','typed','lowered','cgen','cgen-repeat','header','aot-build','jit','module','checked-retirement'):
                    assert text.count('core-host: '+marker+'\n')==1,(name,marker)
                generated=directory/'module.c';code=generated.read_text(encoding='utf-8-sig')
                assert generated.read_bytes()==(directory/'module.repeat.c').read_bytes()
                assert not any(s in code for s in ('xl6_xrt_tcp_','xl6_xrt_udp_','xlir_network_services'))
                flags=['-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',
                    '-Wno-unused-parameter','-Wno-unused-label','-Wno-unused-but-set-variable',
                    '-Wno-unused-const-variable','-Wno-missing-field-initializers','-I'+str(sdk)]
                for opt in ('O0','O2'):run(name+'-'+opt+'-strict',[cc,*flags,'-'+opt,'-fsyntax-only',generated])
                standalone=out/'standalone'/name;standalone.mkdir(parents=True)
                binary=standalone/'program.exe';shutil.copy2(directory/'program.exe',binary)
                run(name+'-aot-relocated',[binary])
                imports=run(name+'-aot-imports',(['E:/software/w64devkit/bin/objdump.exe','-p',binary] if windows else ['readelf','-d',binary]))
                assert not any(s in imports.lower() for s in ('xl.dll','xlc.dll','libxl.so','libxlc.so'))
                run(name+'-cli',[isolated/names[0],'run',sources/row['Source']])
            with lock:report['Proofs'].append(dict(Name=name,Negative=negative));save()
        except Exception as error:
            with lock:report['Failures'].append(dict(Name=name,Error=repr(error)));save()
    with ThreadPoolExecutor(max_workers=args.jobs) as workers:list(workers.map(check,cases))
    report['InputsUnchanged']=all(sha(p)==h for p,h in report['Inputs'].items())
    report['OriginalProductsUnchanged']=all(sha(isolated/p.name)==sha(p) for p in originals)
    report['NoSourceDeployment']=not any((isolated/f).exists() for f in ('src','lib','res','include'))
    report['ExpectedStages']=1+sum(1 if r['Negative'] else 6 for r in cases)
    report['Complete']=not report['Failures'] and report['InputsUnchanged'] and report['OriginalProductsUnchanged'] and report['NoSourceDeployment'] and len(report['Rows'])==report['ExpectedStages']
    report['Products']=[dict(Path=str(p),Sha256=sha(p)) for p in sorted(out.rglob('*')) if p.is_file() and p.name!='results.json']
    save();print(json.dumps(dict(Complete=report['Complete'],Failures=report['Failures'])),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
