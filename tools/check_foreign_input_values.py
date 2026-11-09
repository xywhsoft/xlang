"""Readonly nullable Buffer aliases and pointer-free fixed-array C records.

Requires an independently qualified compiler/SDK and two sealed-IR checkers.
Runs CPU fixtures only; no shipping or full compiler build claim is inferred.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    for n in ('compiler','span-ir','pod-ir','runtime','cc','cxx','out'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();cli=a.compiler.resolve();runtime=a.runtime.resolve()
    out.mkdir(parents=True,exist_ok=False);sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    cases=[('span','foreign_nullable_span','foreign_nullable_span_native.c',a.span_ir),
           ('pod','foreign_pod_fixed_array','foreign_pod_fixed_array.c',a.pod_ir)]
    files={Path(__file__).resolve(),cli,runtime,a.span_ir.resolve(),a.pod_ir.resolve(),a.cc.resolve(),a.cxx.resolve()}
    files.update(cli.parent.glob('*.dll'))
    files.update(root/n for n in ('src/sem/xl6_sem_foreign_compat.c','src/sem/xl6_sem_foreign_pack.c',
        'src/ir/xl6_ir_foreign_span_compat.c','src/ir/xl6_ir_foreign_output_record.c','src/cgen/xl6_ir_cgen_foreign_pack.c'))
    for _,stem,native,_ in cases:files.update(root/'test/cases'/n for n in (stem+'.xl',stem+'.h',native,stem+'_ir.c'))
    report=dict(passed=False,shippingDelivery=False,compilerCohort='caller supplied; qualify independently',inputs={str(x):sha(x) for x in files},rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,cmd,reject=False):
        z=subprocess.run(list(map(str,cmd)),cwd=root/'release'/('win_x64' if os.name=='nt' else 'linux_x64'),capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=list(map(str,cmd)),exitCode=z.returncode,rejected=reject,logSha256=sha(log)));save()
        print(name,z.returncode,flush=True);assert (z.returncode!=0)==reject,log
        return z
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    include=['--inc-path',root,'--inc-path',cli.parent/'include']
    inc=['-I'+str(x) for x in (root,cli.parent/'include')]
    flags=['-std=c11','-DXL_STATIC_EMBED',*inc]
    for tag,stem,native,gate in cases:
        source=root/'test/cases'/(stem+'.xl');helper=root/'test/cases'/native
        cfile=out/(tag+'-module.c');hfile=out/(tag+'-module.h')
        run(tag+'-emit',[cli,'cgen',source,'--emit',cfile,'--emit-header',hfile])
        run(tag+'-ir',[gate,source])
        sidecars=[] if helper==source.with_suffix('.c') else [helper]
        run(tag+'-jit',[cli,'run',source,*sidecars,*include])
        binary=out/(tag+'-tcc'+('.exe' if os.name=='nt' else ''))
        run(tag+'-aot-build',[cli,'build',source,*sidecars,*include,'-o',binary]);run(tag+'-aot-run',[binary])
        for opt in ('O0','O2'):
            binary=out/(tag+'-'+opt+('.exe' if os.name=='nt' else ''))
            run(tag+'-'+opt+'-build',[a.cc,*flags,'-'+opt,cfile,helper,root/'test/cases/foreign_decl_module_host.c',runtime,'-o',binary])
            run(tag+'-'+opt+'-run',[binary])
        for cc,std in ((a.cc,'c11'),(a.cxx,'c++11'),(a.cxx,'c++17')):
            run(tag+'-header-'+std,[cc,'-std='+std,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*inc,'-x','c' if std=='c11' else 'c++','-fsyntax-only',hfile])
    code=(root/'test/cases/foreign_nullable_span.xl').read_text()
    probe=next(x for x in code.splitlines() if 'func probe(' in x);read=next(x for x in code.splitlines() if 'func read(string' in x)
    negatives={n:code.replace(probe,probe.replace('Buffer? data',typ+' data')) for n,typ in
        [('nullable-string','string?'),('nullable-bytes','bytes?'),('nullable-pod','Pod?')]}
    for name,old,new in [('pointer','const void*','const char*'),('count','size_t','int'),('effect','@no_raise','@pure @no_raise')]:
        negatives[name]=code.replace(read,read.replace(old,new))
    negatives['writable-string']=code.replace(read,read.replace('const void*','void*'))
    packed=next(x for x in code.splitlines() if 'func pack(string' in x)
    negatives['packed-span-header']=code.replace(packed,packed.replace('"size_t")','"size_t","other.h")'))
    for name,bad in negatives.items():
        f=out/('bad-'+name+'.xl');f.write_text(bad,encoding='utf8');target=out/('bad-'+name+'-module.c')
        run('reject-'+name,[cli,'cgen',f,'--emit',target],True);assert not target.exists()
    # Declaration order must not choose the native ABI or preparation policy.
    lines=code.splitlines()
    for method in ('read','pack'):
        indices=[i for i,x in enumerate(lines) if 'extern ' in x and 'func '+method+'(' in x];assert len(indices)==4
        values=[lines[i] for i in indices][::-1]
        for i,value in zip(indices,values):lines[i]=value
    source=out/'reversed.xl';source.write_text('\n'.join(lines)+'\n',encoding='utf8')
    run('reversed-jit',[cli,'run',source,root/'test/cases/foreign_nullable_span_native.c',*include])
    assert all(sha(path)==digest for path,digest in report['inputs'].items())
    report.update(passed=True,inputsUnchanged=True);save()


if __name__=='__main__':main()
