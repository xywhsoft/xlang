"""Typed ownership capability catalog and actual source JIT graph retirement.

Full current compiler/runtime objects, genuine TCC loader, original language
fixtures and original complete field/destructor/root assertions. Not embedded
product VFS, independent language AOT, runtime-wide sanitizers or full53.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time,concurrent.futures
from xlir_public_build_inputs import verify_public_build
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','public-directory','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();assert not out.exists()
    build,pinned,compared=verify_public_build(root,a.public_directory);cc=shutil.which('gcc');assert cc
    windows=os.name=='nt';suffix='.exe' if windows else '';task=Path(__file__).resolve().parents[1]
    contract=task/'test/support/ownership_catalog_contract.c'
    declarations=task/'test/support/ownership_catalog_declarations.c'
    families=[('stack','stdlib_stack_ownership',1,25),('tree','stdlib_tree_ownership',5,12),
        ('queue','stdlib_concurrent_queue_ownership',6,25)]
    fixtures=[root/'test/cases'/(stem+'.xl') for _,stem,_,_ in families]
    hosts=[root/'test/cases'/(stem+'_public.c') for _,stem,_,_ in families]
    native=root/'release/xl.dll' if windows else Path(build['NativeRoot'])/'release/libxl.so'
    pinned.update((Path(__file__).resolve(),Path(sys.executable).resolve(),Path(cc),contract,declarations,native,*fixtures,*hosts))
    inputs=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(pinned)];out.mkdir(parents=True)
    objects=[row['Path'] for row in build['Objects'] if Path(row['Path']).stem not in
        ('xlir_public_source_driver','xlir_public_source_linux_driver')]
    systems=['-lws2_32','-liphlpapi','-lshell32','-lole32','-ladvapi32'] if windows else ['-pthread','-ldl','-lm']
    flags=['-std=c11','-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED','-I'+str(root),'-I'+str(root/'release'),
        '-I'+str(root/'release/include'),'-I'+str(root/'src/runtime'),'-I'+str(a.public_directory.resolve()/'include')]
    if not windows:flags+=['-D_GNU_SOURCE']
    links=[native,*systems,*([] if windows else ['-Wl,-rpath,'+str(native.parent),'-Wl,--export-dynamic'])]
    if windows:shutil.copy2(native,out/'xl.dll')
    rows=[];prefixes=[];proofs=[];env=dict(os.environ)
    if not windows:env['XL6_SOURCE_NATIVE_ROOT']=build['NativeRoot']
    report=dict(Complete=False,InputsUnchanged=False,Inputs=inputs,Rows=rows,ClosedPrefixes=prefixes,Proofs=proofs,
        ComparedSources=compared,Scope=__doc__,NativeCatalogAcceptance=False,SourceJitGraphAcceptance=False,
        AotGraphRetirementAcceptance=False,ShippingLibraryAcceptance=False,RuntimeInstrumented=False,Full53Acceptance=False)
    def run(name,command,reject=False):
        command=list(map(str,command));start=time.monotonic()
        try:
            process=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
            data,code=process.stdout+process.stderr,process.returncode
        except subprocess.TimeoutExpired as error:data,code=(error.stdout or b'')+(error.stderr or b''),'timeout'
        log=out/(name+'.log');log.write_bytes(data)
        row=dict(Name=name,Command=command,Exit=code,ExpectedRejection=reject,Seconds=time.monotonic()-start,
            Log=str(log),LogSha256=sha(log))
        assert code not in (0,'timeout') if reject else code==0,(name,data[-5000:].decode(errors='replace'))
        return row,data
    def check(name,command,reject=False):
        row,data=run(name,command,reject);rows.append(row);return data
    try:
        def closed_headers(header_text):
            return all(name.endswith('.h') and (root/'src/runtime'/name).is_file()
                for name in re.findall(r'^#\s*include "(xl6_[^"/]+)"',header_text,re.M))
        assert all(closed_headers(path.read_text(encoding='utf-8')) for path in (root/'src/runtime').glob('*.h'))
        actual=(root/'src/runtime/xl6_ownership_domains.h').read_text(encoding='utf-8')
        assert closed_headers(actual) and not closed_headers(actual+'\n#include "xl6_ownership_domains.inc"\n')
        domains=sorted((root/'src/runtime').glob('xl6_ownership_domain_*.c'))
        assert len(domains)==69
        for source in domains:
            includes=re.findall(r'^#\s*include "([^"]+)"',source.read_text(encoding='utf-8'),re.M)
            assert 'include/xruntime_decl.h' in includes and not any(name.startswith('release/') for name in includes),source
        report['PrivateSourceDeclarationIncludeAcceptance']=True
        report['DeclarationSiblingClosureAcceptance']=True
        for opt in ('O0','O2'):
            check(opt+'.declarations',[cc,*flags,'-'+opt,'-fsyntax-only',declarations])
            binary=out/('contract.'+opt+suffix)
            check(opt+'.contract.build',[cc,*flags,'-'+opt,contract,*objects,*links,'-o',binary])
            for mode in ('basic','plans','oom','ignored-oom'):
                data=check(opt+'.'+mode,[binary,mode])
                if mode.endswith('oom'):
                    match=re.search(rb'prefixes=(\d+) first-no-hit=(\d+) balanced',data);assert match
                    runs,first=map(int,match.groups());assert runs==first+1 and first>0
                    prefixes.append(dict(Optimization=opt,Mode=mode,Runs=runs,FirstNoHit=first))
        source=(root/'src/runtime/xl6_ownership_catalog.c').read_text(encoding='utf-8-sig')
        mutants=[
            ('lost-load-failure','!domains[i]->Load(catalog) || catalog->failed','!domains[i]->Load(catalog)','ignored-oom'),
            ('reopen-sealed','!catalog->building || !policy','false || !policy','basic'),
            ('lost-conflict','known && selected && (selected!=current || pending!=ready || finish!=step)','false','basic'),
            ('admit-unknown','if(!selected)return false;','if(!selected)return true;','basic'),
            ('lost-null-failure','if(!outcome.ok){xl6OwnershipCatalogDestroyV1(catalog);','if(outcome.error){xl6OwnershipCatalogDestroyV1(catalog);','basic'),
            ('lost-earlier-diagnostic','catalogPropagateFailure(outcome);','xl6ErrorScopePropagateFailure(outcome);','basic'),
            ('unknown-count','const xrtownershipadapterv1* selected=xrtValueOwnershipAdapterV1(ref);',
                'size_t untrusted=0;if(ref.Ops && ref.Ops->Count)(void)ref.Ops->Count(ref.Data,&untrusted);\n    const xrtownershipadapterv1* selected=xrtValueOwnershipAdapterV1(ref);','basic')]
        for name,old,new,mode in mutants:
            assert source.count(old)==1,(name,old)
            changed=out/(name+'.c');changed.write_text(source.replace(old,new),encoding='utf-8');binary=out/(name+suffix)
            others=[obj for obj in objects if Path(obj).stem!='xl6_ownership_catalog']
            check(name+'.build',[cc,*flags,'-O0',contract,changed,*others,*links,'-o',binary])
            data=check(name+'.reject',[binary,mode],True);assert b'assert' in data.lower()
        report['NativeCatalogAcceptance']=True;jobs=[]
        report['DeclarationSdkSyntaxAcceptance']=True
        for (family,stem,size,lifetimes),fixture,host in zip(families,fixtures,hosts):
            for opt in ('O0','O2'):
                binary=out/(family+'.'+opt+suffix)
                check(family+'.'+opt+'.build',[cc,*flags,'-'+opt,host,*objects,*links,'-o',binary])
                modes=[(0,0)]+[(mode,item) for mode in (1,2,3) for item in range(size)]
                for mode,item in modes:
                    name=family+'.'+opt+'.mode'+str(mode)+'.family'+str(item)
                    args=[binary,mode,*([] if family=='stack' else [item]),a.public_directory.resolve()/'include',
                        out/(name+'.tmp'),native,root/'res/tcc/win64/x86_64' if windows else '',fixture]
                    jobs.append((name,args,lifetimes))
        def graph(job):
            name,args,lifetimes=job;row,data=run(name,args)
            assert b'balanced' in data and (str(lifetimes)+' ').encode() in data
            return row,dict(Name=name,MeasuredLifetimes=lifetimes)
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
            for row,proof in executor.map(graph,jobs):rows.append(row);proofs.append(proof)
        assert len(proofs)==78 and sum(p['MeasuredLifetimes'] for p in proofs)==1534
        report['SourceJitGraphAcceptance']=True
        report['InputsUnchanged']=all(sha(row['Path'])==row['Sha256'] for row in inputs);assert report['InputsUnchanged']
        report.update(Complete=True,PhaseCount=len(rows))
    except Exception as error:report['Error']=repr(error);print(repr(error),flush=True)
    report['Products']=[dict(Path=str(path),Sha256=sha(path)) for path in sorted(out.rglob('*')) if path.is_file()]
    (out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print('Ownership catalog phases='+str(len(rows))+' complete='+str(report['Complete']),flush=True)
    return 0 if report['Complete'] else 1
if __name__=='__main__':raise SystemExit(main())
