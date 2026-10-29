"""Resident typed Tree owner, results, real edges, reentry and complete allocation-failure prefixes.
This is not full language graph/continuation/53-module or shipping-DLL acceptance.
"""
from __future__ import annotations
import argparse, hashlib, json, re, shutil, subprocess, sys, time
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    for name in ('public','runtime'): parser.add_argument('--'+name,type=Path)
    parser.add_argument('--provider',type=Path)
    parser.add_argument('--host',type=Path)
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    assert bool(args.public)!=bool(args.runtime) and not out.exists()
    cc=shutil.which('gcc'); assert cc
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable)}
    for folder in ('src','lib','release/include'):
        files.update(f for f in (root/folder).rglob('*') if f.suffix in ('.c','.h','.inc'))
    sources=[args.host.resolve() if args.host else 'test/cases/stdlib_tree_native_host.c',
             args.provider.resolve() if args.provider else 'src/runtime/xl6_tree.c',
             'test/support/module_product_memory.c']
    contract='test/cases/xlir_tree_native_contract.c'
    cases=['test/cases/'+n+'.xl' for n in ('xlir_avl_resource_abi','stdlib_tree_native_reference','stdlib_tree_native_projection')]
    files.update(root/f for f in sources+cases+[contract,'test/support/module_product_memory.h'])
    objects=[]
    if args.public:
        build=json.loads((args.public/'build.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly'] and not args.sanitize
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name!='xlir_public_source_driver.o']
        dll=root/'release/xl.dll'; files.add(dll)
        links=[str(dll),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']; suffix='.exe'
    else:
        assert sys.platform=='linux'; runtime=args.runtime.resolve(); suffix=''
        files.update(runtime/n for n in ('libxl.so','libxlc.so'))
        links=['-L'+str(runtime),'-lxlc','-lxl','-Wl,-rpath,'+str(runtime),'-pthread','-ldl','-lm']
    inputs=[{'Path':str(f),'Sha256':sha(f)} for f in sorted(files)]
    out.mkdir(parents=True)
    if args.public: shutil.copy2(dll,out/'xl.dll')
    rows=[]; prefixes=[]; contracts=[]
    report={'Complete':False,'InputsUnchanged':False,'Full53Acceptance':False,
            'SourceProductAcceptance':args.provider is None and args.host is None,
            'NativeProviderOverride':str(args.provider.resolve()) if args.provider else None,
            'HostOverride':str(args.host.resolve()) if args.host else None,
            'LanguageOwnershipGraphAcceptance':False,'WholeRuntimeInstrumented':False,
            'NativeTreeInstrumented':args.sanitize,'HostInstrumented':args.sanitize,
            'Platform':sys.platform,'Inputs':inputs,'Rows':rows,'Prefixes':prefixes,
            'Contracts':contracts,'Scope':__doc__}
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start})
        if result.returncode: raise RuntimeError(name+': '+data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include']
    if args.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    try:
        for level in (0,2):
            lane='O'+str(level); binary=out/('tree-'+lane+suffix)
            # Compile the NEW provider body at each level, not just the host.
            nativeObjects=[f for f in objects if Path(f).name!='xl6_tree.o']
            run(lane+'.build',[cc,*flags,'-O'+str(level),*sources,*nativeObjects,*links,'-o',binary])
            run(lane+'.vectors',[binary])
            checker=out/('tree-contract-'+lane+suffix)
            run(lane+'.contract.build',[cc,*flags,'-O'+str(level),contract,*objects,*links,'-o',checker])
            for index,case in enumerate(cases):
                text=run(lane+'.contract.'+str(index),[checker,case])
                match=re.search(r'(\d+) actual factories \((\d+) nominal\), (\d+) operations, (\d+) rejected mutations',text)
                assert match
                factories,nominal,operations,mutations=map(int,match.groups())
                assert factories and operations>=factories and mutations>=operations*5+factories*3
                if 'reference' in case: assert nominal>=2
                contracts.append({'Optimization':lane,'Source':case,'Factories':factories,
                    'NominalFactories':nominal,'Operations':operations,'RejectedMutations':mutations})
            for operation in ('create','clone','endpoint','projection'):
                for point in range(4097):
                    text=run(lane+'.'+operation+'.prefix.'+str(point),[binary,operation,point])
                    if 'hit=0 complete-prefix balanced' in text:
                        prefixes.append({'Optimization':lane,'Operation':operation,'FirstNoHit':point,'Runs':point+1}); break
                else: raise AssertionError('allocation prefix did not close: '+operation)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); raise
    finally:
        (out/'tree.native.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(f'Native Tree: {len(rows)} phases, {len(prefixes)} complete OOM prefixes, balanced',flush=True)

if __name__=='__main__': main()
