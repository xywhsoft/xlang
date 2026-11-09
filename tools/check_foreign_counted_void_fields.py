"""Exact void/const-void counted byte fields, without widening text/pointee ABI.

Requires an independently qualified compiler and matching runtime. No native
provider wrapper, graphics device, network or shipping claim is involved.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    for key in ('compiler','runtime','cc','cxx','out'):p.add_argument('--'+key,type=Path,required=True)
    a=p.parse_args();root=a.root.resolve();out=a.out.resolve();cli=a.compiler.resolve();runtime=a.runtime.resolve()
    out.mkdir(parents=True,exist_ok=False);sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    source=root/'test/cases/foreign_counted_void_fields.xl'
    nested=root/'test/cases/foreign_counted_void_fields_nested.xl'
    env=dict(os.environ,XL6_NESTED_TEST_TEMP=str(out/"nested-temp"))
    (out/"nested-temp").mkdir()
    paths={Path(__file__).resolve(),cli,runtime,a.cc.resolve(),a.cxx.resolve(),nested,source,source.with_suffix('.h'),root/'src/cgen/xl6_ir_cgen_identity_closure.c',root/'src/cgen/xl6_ir_cgen_type_call.c'}
    paths.update(cli.parent.glob('*.dll'))
    report=dict(passed=False,shippingDelivery=False,compilerCohort='caller supplied; qualify independently',inputs={str(x):sha(x) for x in paths},rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    def run(name,command,rejected=False,diagnostic=None):
        cmd=list(map(str,command));cwd=root if name.startswith('nested-') else root/'release'/('win_x64' if os.name=='nt' else 'linux_x64')
        z=subprocess.run(cmd,env=env,cwd=cwd,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=cmd,cwd=str(cwd),exitCode=z.returncode,rejected=rejected,logSha256=sha(log)));save()
        print(name,z.returncode,flush=True);assert (z.returncode!=0)==rejected,log
        if diagnostic:assert diagnostic.encode() in z.stdout+z.stderr,log
    if os.name=='nt':shutil.copy2(runtime,out/runtime.name)
    inc=['-I'+str(root),'-I'+str(cli.parent/'include')];include=['--inc-path',root,'--inc-path',cli.parent/'include']
    cfile=out/'counted-module.c';header=out/'counted-module.h'
    run('emit',[cli,'cgen',source,'--emit',cfile,'--emit-header',header])
    run('jit',[cli,'run',source,*include])
    binary=out/('counted-tcc'+('.exe' if os.name=='nt' else ''))
    run('tcc-build',[cli,'build',source,*include,'-o',binary]);run('tcc-run',[binary])
    for opt in ('O0','O2'):
        binary=out/('counted-'+opt+('.exe' if os.name=='nt' else ''))
        run(opt+'-build',[a.cc,'-std=c11','-DXL_STATIC_EMBED',*inc,'-'+opt,cfile,root/'test/cases/foreign_decl_module_host.c',runtime,'-o',binary])
        run(opt+'-run',[binary])
    bad=out/'text.xl'
    bad.write_text(source.read_text(encoding='utf8').split('func main')[0].replace('get constant() as bytes','get constant() as string').replace('@c_result("bytes_view") get constant()','@c_result("text_view") get constant()')+'auto value=view(0)\nprint(value.constant)\n',encoding='utf8')
    badHeader=out/'text.h';run('text-emit',[cli,'cgen',bad,'--emit',out/'text-module.c','--emit-header',badHeader])
    for cc,std in ((a.cc,'c11'),(a.cxx,'c++11'),(a.cxx,'c++17')):
        flags=[cc,'-std='+std,'-Wall','-Wextra','-Werror','-DXL_STATIC_EMBED',*inc,'-x','c' if std=='c11' else 'c++','-fsyntax-only']
        run('header-'+std,[*flags,header])
        run('reject-pointer-'+std,[*flags,'-DCOUNTED_BAD_POINTER',header],True,'native field data type')
        run('reject-length-'+std,[*flags,'-DCOUNTED_BAD_LENGTH',header],True,'native field length type')
        run('reject-text-'+std,[*flags,badHeader],True,'native field data type')
    run('nested-jit',[cli,'run',nested,*include])
    nestedBinary=out/('nested-tcc'+('.exe' if os.name=='nt' else ''))
    run('nested-tcc-build',[cli,'build',nested,*include,'-o',nestedBinary])
    run('nested-tcc-run',[nestedBinary])
    assert all(sha(path)==digest for path,digest in report['inputs'].items())
    report.update(passed=True,inputsUnchanged=True);save()


if __name__=='__main__':main()
