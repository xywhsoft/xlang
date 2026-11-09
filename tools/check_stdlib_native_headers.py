"""Qualify authoritative CRT headers across language/native C translation units.

The caller supplies an actual compiler/SDK cohort. Optional XGE packages add
the integration reproduction; the ordinary CRT checks do not depend on XGE.
"""
from pathlib import Path
import argparse, hashlib, json, os, subprocess


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    for name in ('compiler','out','cc','cxx'):p.add_argument('--'+name,type=Path,required=True)
    for name in ('dynamic-packages','static-packages'):p.add_argument('--'+name,type=Path)
    a=p.parse_args();root,compiler,out,cc,cxx=[v.resolve() for v in (a.root,a.compiler,a.out,a.cc,a.cxx)]
    assert bool(a.dynamic_packages)==bool(a.static_packages)
    out.mkdir(parents=True,exist_ok=False);sha=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    files=[Path(__file__).resolve(),compiler,cc,cxx,root/'src/stdlib/xl6_stdlib.c',
        root/'src/stdlib/xl6_stdlib_data.c',root/'src/stdlib/xl6_stdlib_data.h',
        root/'res/tcc/win64/x86_64/include/tcc/tcc_libm.h']
    cases=[root/'test/cases'/name for name in ('stdlib_crt_math.xl','stdlib_crt_math_header.xl')]
    files+=cases+[root/'test/cases'/('stdlib_crt_math_header'+suffix) for suffix in ('.h','.c')]
    overload=root/'test/cases/foreign_header_overload.xl'
    files += [overload,overload.with_suffix('.h')]
    files+=list(compiler.parent.glob('*.dll'))
    for folder in (a.dynamic_packages,a.static_packages):
        if folder:files += [path for path in folder.resolve().rglob('*') if path.is_file()]
    report=dict(passed=False,installed=False,compilerCohort='caller supplied; qualify build separately',
        inputs={str(path):sha(path) for path in files},rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,argv,packages=None,refused=False):
        env=os.environ.copy();env.pop('XLANG_PACKAGE_PATH',None)
        if packages:env['XLANG_PACKAGE_PATH']=str(packages.resolve())
        argv=list(map(str,argv));r=subprocess.run(argv,cwd=root/'release'/('win_x64' if os.name=='nt' else 'linux_x64'),
            env=env,capture_output=True,timeout=180)
        log=out/(name+'.log');log.write_bytes(r.stdout+r.stderr)
        report['rows'].append(dict(name=name,command=argv,packages=str(packages),exitCode=r.returncode,refused=refused,logSha256=sha(log)));save()
        if (r.returncode!=0)!=refused:raise RuntimeError(str(log))
        if refused:assert b'declared C function ABI' in r.stdout+r.stderr,log
    include=['--inc-path',root,'--inc-path',compiler.parent/'include']
    for case in cases:
        name=case.stem;cfile=out/(name+'-module.c');hfile=out/(name+'-module.h')
        run(name+'-emit',[compiler,'cgen',case,'--emit',cfile,'--emit-header',hfile])
        text=hfile.read_text(encoding='utf-8')
        assert '#include <math.h>' in text and 'extern double fabs(' not in text
        assert 'XLIR_NATIVE_IS_FUNCTION(&fabs,' in text
        run(name+'-jit',[compiler,'run',case,*include])
        binary=out/(name+('.exe' if os.name=='nt' else ''))
        run(name+'-build',[compiler,'build',case,'-o',binary,*include]);run(name+'-run',[binary])
        if name.endswith('_header'):
            for language,tool,std in [('c',cc,'c11'),('c++',cxx,'c++17')]:
                run('header-'+language,[tool,'-std='+std,'-Wall','-Wextra','-Werror','-fsyntax-only',
                    '-I'+str(compiler.parent/'include'),'-I'+str(root),'-x',language,hfile])
    for name,source,refused in [
        ('overload',overload,False),
        ('overload-wrong-parameter',out/'wrong-parameter.xl',True),
        ('overload-wrong-result',out/'wrong-result.xl',True)]:
        if refused:
            text=overload.read_text(encoding='utf-8')
            if name.endswith('parameter'):text=text.replace('func read(c_double value)','func read(c_int value)').replace('read(c_double(1.5))','read(c_int(1))')
            else:text=text.replace('as c_double','as c_float',1)
            source.write_text(text,encoding='utf-8')
        hfile=out/(name+'-module.h')
        run(name+'-emit',[compiler,'cgen',source,'--emit',out/(name+'-module.c'),'--emit-header',hfile])
        for language,tool,std in [('c',cc,'c11'),('c++',cxx,'c++11'),('c++',cxx,'c++17')]:
            run(name+'-'+std,[tool,'-std='+std,'-Wall','-Wextra','-Werror','-fsyntax-only',
                '-I'+str(compiler.parent/'include'),'-I'+str(root),'-x',language,hfile],refused=refused)
        if not refused:run(name+'-jit',[compiler,'run',source,*include])
    if a.dynamic_packages:
        case=out/'xge_math.xl';case.write_text('import xge\n'+cases[1].read_text(encoding='utf-8'),encoding='utf-8')
        helper=root/'test/cases/stdlib_crt_math_header.c'
        run('xge-jit',[compiler,'run',case,helper,*include],a.dynamic_packages)
        binary=out/('xge_math.exe' if os.name=='nt' else 'xge_math')
        run('xge-build',[compiler,'build',case,helper,'-o',binary,*include],a.static_packages)
        run('xge-run',[binary])
    assert all(sha(path)==value for path,value in report['inputs'].items())
    report.update(passed=True,inputsUnchanged=True);save();print(json.dumps(dict(passed=True,rows=len(report['rows']))))


if __name__=='__main__':main()
