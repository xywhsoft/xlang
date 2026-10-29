"""Typed-tree native Array producer: exact slots, sealed schemas and complete OOM prefixes.
This prerequisite is not full language ownership/53-module acceptance.
"""
from __future__ import annotations
import argparse, hashlib, json, re, shutil, subprocess, sys, time
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    for name in ('public','runtime'): parser.add_argument('--'+name,type=Path)
    args=parser.parse_args(); root=args.root.resolve(); out=args.out.resolve()
    assert bool(args.public)!=bool(args.runtime) and not out.exists()
    cc=shutil.which('gcc'); assert cc
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable)}
    for folder in ('src','lib','release/include'):
        files.update(f for f in (root/folder).rglob('*') if f.suffix in ('.c','.h','.inc'))
    sources=['test/cases/stdlib_tree_projection_native_host.c','test/support/module_product_memory.c']
    files.update(root/f for f in sources+['test/support/module_product_memory.h'])
    files.update(root/f for f in ['test/cases/xlir_tree_projection_contract.c','test/cases/stdlib_tree_native_projection.xl'])
    objects=[]
    if args.public:
        build=json.loads((args.public/'build.json').read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
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
    rows=[]; prefixes=[]
    report={'Complete':False,'InputsUnchanged':False,'Full53Acceptance':False,
            'LanguageOwnershipGraphAcceptance':False,'Platform':sys.platform,
            'Inputs':inputs,'Rows':rows,'Prefixes':prefixes,'Scope':__doc__}
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start})
        if result.returncode: raise RuntimeError(name+': '+data[-4000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include']
    try:
        for level in (0,2):
            lane='O'+str(level); binary=out/('tree-'+lane+suffix)
            run(lane+'.build',[cc,*flags,'-O'+str(level),*sources,*objects,*links,'-o',binary])
            run(lane+'.vectors',[binary])
            contract=out/('tree-contract-'+lane+suffix)
            run(lane+'.contract.build',[cc,*flags,'-O'+str(level),'test/cases/xlir_tree_projection_contract.c',
                *objects,*links,'-o',contract])
            text=run(lane+'.contract.mutations',[contract,'test/cases/stdlib_tree_native_projection.xl'])
            match=re.search(r'(\d+) actual producers, (\d+) rejected mutations; output retained',text)
            assert match and int(match[1])>=6 and int(match[2])==int(match[1])*9
            for point in range(4097):
                text=run(lane+'.prefix.'+str(point),[binary,point])
                if 'hit=0 complete-prefix balanced' in text:
                    prefixes.append({'Optimization':lane,'FirstNoHit':point,'Runs':point+1}); break
            else: raise AssertionError('allocation prefix did not close')
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error:
        report['Error']=repr(error); raise
    finally:
        (out/'tree-projection.native.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(f'Tree projection native: {len(rows)} phases, two complete OOM prefixes, balanced',flush=True)

if __name__=='__main__': main()
