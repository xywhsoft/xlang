"""Strict native three-family Queue reference admission, mixed Tree/Array depth,
and existing Tree/16-domain Sequence regressions. Provider overrides are explicit;
this is not whole-runtime instrumentation or source/shipping-product acceptance.
"""
from __future__ import annotations
import argparse, hashlib, json, shutil, subprocess, sys, time
from pathlib import Path

def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','out'): parser.add_argument('--'+name,type=Path,required=True)
    for name in ('public','runtime'): parser.add_argument('--'+name,type=Path)
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--queue-host',type=Path)
    args=parser.parse_args(); root,out=args.root.resolve(),args.out.resolve()
    cc=shutil.which('gcc'); assert cc and not out.exists() and bool(args.public)!=bool(args.runtime)
    providers=['xl6_tree','xl6_tree_admission','xl6_array_sequence_admission',
        'xl6_queue_spsc','xl6_queue_mpsc','xl6_queue_mpmc',
        'xl6_queue_spsc_admission','xl6_queue_mpsc_admission','xl6_queue_mpmc_admission']
    hosts={'queue':'stdlib_queue_reference_admission_native_host.c',
        'tree':'stdlib_tree_admission_native_host.c','sequence':'stdlib_array_sequence_admission_native_host.c'}
    files={Path(__file__).resolve(),Path(cc),Path(sys.executable)}
    files.update(root/('src/runtime/'+name+'.c') for name in providers)
    files.update(root/('test/cases/'+name) for name in hosts.values())
    if args.queue_host: files.add(args.queue_host.resolve())
    files.update(root/('test/support/'+name) for name in ('module_product_memory.c','module_product_memory.h'))
    for folder in ('src','lib','release/include'):
        files.update(p for p in (root/folder).rglob('*') if p.suffix in ('.c','.h','.inc'))
    if args.public:
        assert sys.platform=='win32' and not args.sanitize
        manifest=args.public.resolve()/'build.json'; files.add(manifest)
        build=json.loads(manifest.read_text(encoding='utf-8-sig'))
        assert build['SourceXrt'] and not build['RuntimeOnly']
        for row in build['Inputs']+build['Headers']+build['Objects']:
            assert sha(row['Path'])==row['Sha256'].lower(); files.add(Path(row['Path']))
        omitted={name+'.o' for name in providers}|{'xlir_public_source_driver.o'}
        objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).name not in omitted]
        # Source island contains no changed DLL: the real source-XRT objects
        # implement the tested providers; the base DLL remains only a link input.
        base=Path(next(row['Path'] for row in build['Inputs'] if str(row['Path']).replace('\\','/').endswith('/src/runtime/xl6_tree.c'))).parents[2]
        dll=base/'release/xl.dll'; files.add(dll)
        links=[*objects,str(dll),'-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32']; suffix='.exe'
    else:
        assert sys.platform=='linux'; runtime=args.runtime.resolve(); suffix=''
        files.update(runtime/name for name in ('libxl.so','libxlc.so'))
        links=['-L'+str(runtime),'-lxlc','-lxl','-Wl,-rpath,'+str(runtime),'-pthread','-ldl','-lm']
    inputs=[{'Path':str(path),'Sha256':sha(path)} for path in sorted(files)]
    out.mkdir(parents=True)
    if args.public: shutil.copy2(dll,out/'xl.dll')
    rows,prefixes=[],[]
    report={'Complete':False,'InputsUnchanged':False,'Scope':__doc__,'Platform':sys.platform,
        'Full53Acceptance':False,'SourceProductAcceptance':False,'ShippingLibraryAcceptance':False,
        'LanguageGraphAcceptance':False,'WholeRuntimeInstrumented':False,
        'ProvidersInstrumented':args.sanitize,'HostInstrumented':args.sanitize,
        'QueueHostOverride':str(args.queue_host.resolve()) if args.queue_host else None,
        'Inputs':inputs,'Rows':rows,'Prefixes':prefixes}
    def save(): (out/'queue.reference.admission.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        command=list(map(str,command)); start=time.monotonic()
        result=subprocess.run(command,cwd=root,capture_output=True,timeout=180)
        data=result.stdout+result.stderr; (out/(name+'.log')).write_bytes(data)
        rows.append({'Name':name,'Command':command,'Exit':result.returncode,'Seconds':time.monotonic()-start}); save()
        if result.returncode: raise RuntimeError(name+': '+data[-5000:].decode(errors='replace'))
        return data.decode(errors='replace')
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I.','-Irelease','-Irelease/include']
    if args.sanitize: flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-fno-pie','-no-pie']
    try:
        for level in (0,2):
            lane='O'+str(level); objects=[]
            for provider in providers:
                obj=out/(provider+'.'+lane+'.o')
                run(provider+'.'+lane+'.build',[cc,*flags,'-O'+str(level),'-c','src/runtime/'+provider+'.c','-o',obj]); objects.append(obj)
            for label,host in hosts.items():
                binary=out/(label+'.'+lane+suffix)
                hostSource=args.queue_host.resolve() if label=='queue' and args.queue_host else 'test/cases/'+host
                run(label+'.'+lane+'.build',[cc,*flags,'-O'+str(level),*objects,hostSource,
                    *(['test/support/module_product_memory.c'] if label!='sequence' else []),*links,'-o',binary])
                assert 'balanced' in run(label+'.'+lane+'.vectors',[binary])
                if label!='queue': continue
                for operation in ('0','1','2','deep'):
                    for point in range(4097):
                        text=run(lane+'.'+operation+'.prefix.'+str(point),[binary,operation,point])
                        if 'hit=0 complete-prefix balanced' in text:
                            prefixes.append({'Optimization':lane,'Operation':operation,'FirstNoHit':point,'Runs':point+1}); break
                    else: raise AssertionError('Allocation prefix did not close: '+operation)
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs)
        assert report['InputsUnchanged']; report['Complete']=True
    except Exception as error: report['Error']=repr(error); raise
    finally: save()
    print(f'Queue reference admission: {len(rows)} phases, {len(prefixes)} complete OOM prefixes balanced',flush=True)
if __name__=='__main__': main()
