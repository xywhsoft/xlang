"""Private identity source-unit tests with one-shot forwarding observers.
Actual XRT is provided by a pinned shipping library; project identity/report/
completion and diagnostic code are compiled as test source, not passed off as
shipping compiler acceptance. Parser observers count fallback and forward to
the actual parser. Real IDE/JIT/AOT product gates remain separately required.
"""
from pathlib import Path
import argparse,hashlib,json,os,shutil,subprocess,sys

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--old-root',type=Path,help='Optional immutable pre-fix sources must expose the real disk fallback')
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();assert not out.exists()
    cc=shutil.which('gcc');assert cc;windows=os.name=='nt'
    build=json.loads(a.build_report.read_text(encoding='utf-8-sig'));assert build['Complete'] and build['InputsUnchanged']
    provider=next(Path(r['Path']) for r in build['Products'] if Path(r['Path']).name==('xlc.dll' if windows else 'libxlc.so'))
    recorded={Path(r['Path']):r['Sha256'] for r in build['Products']};assert sha(provider)==recorded[provider]
    sources=[root/'test/cases/stdlib_source_identity_faults.c',*[
        root/'src'/n for n in ('diag/xl6_diag.c','common/xl6_output.c','common/xl6_console.c','ast/xl6_ast.c','lexer/xl6_token.c',
            'lexer/xl6_lexer_api.c','lexer/xl6_lexer_gen.c','parser/xl6_parser.c','package/xl6_package.c',
            'compile/xl6_build_inputs.c','sem/xl6_type_registry.c','type/xl6_type_model.c')]]
    observed=[root/'src'/n for n in ('compile/xl6_source_overlay.c','compile/xl6_source_report.c','sem/xl6_completion.c',
        'compile/xl6_imports.c','parser/xl6_parser_api.c')]
    files={Path(__file__).resolve(),Path(sys.executable),Path(cc),a.build_report.resolve(),provider,*sources,*observed}
    files.update((root/'src').rglob('*.h'));files.update((root/'lib').glob('x*/x*.h'))
    files.update((root/'src').rglob('*.inc'))
    if a.old_root:
        old=a.old_root.resolve();files.update(old/'src/compile'/n for n in ('xl6_source_overlay.c','xl6_source_report.c'))
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(files)]
    out.mkdir();rows=[];proofs=[];failures=[];products=[]
    mutant=out/'imports-error-control.c'
    body=(root/'src/compile/xl6_imports.c').read_text(encoding='utf-8-sig')
    before='if (status != 0) return status;';assert body.count(before)==2
    mutant.write_text(body.replace(before,'if (status > 0) return status;'),encoding='utf-8')
    products.append(dict(Path=str(mutant),Sha256=sha(mutant)))
    env={k:v for k,v in os.environ.items() if k.upper() not in ('CPATH','C_INCLUDE_PATH','LIBRARY_PATH','LD_PRELOAD','LD_AUDIT','LD_LIBRARY_PATH')}
    if windows:env['PATH']=str(provider.parent)+';'+os.environ.get('PATH',os.environ.get('Path',''))
    def run(name,command,directory):
        r=subprocess.run(list(map(str,command)),cwd=directory,env=env,capture_output=True,timeout=180)
        data=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(data)
        rows.append(dict(Name=name,Command=list(map(str,command)),Exit=r.returncode,Log=str(log),LogSha256=sha(log)))
        return r.returncode,data
    try:
        for opt in ('O0','O2'):
            for variant in ('fixed','old','import') if a.old_root else ('fixed','import'):
                control=variant=='old';name=opt+'-'+variant;work=out/name;work.mkdir();binary=work/('probe'+('.exe' if windows else ''))
                flags=['-std=c11','-Wall','-Wextra','-Werror','-'+opt,'-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-DXL_STATIC_EMBED','-I'+str(root)]
                if not windows:flags+=['-D_GNU_SOURCE']
                if control:
                    flags+=['-DXL6_IDENTITY_OLD_CONTROL']
                    flags+=['-DXL6_IDENTITY_'+key+'_SOURCE="'+(old/'src/compile'/filename).as_posix()+'"'
                        for key,filename in (('OVERLAY','xl6_source_overlay.c'),('REPORT','xl6_source_report.c'))]
                if variant=='import':flags+=['-DXL6_IDENTITY_IMPORT_SOURCE="'+mutant.as_posix()+'"']
                links=[provider] if windows else ['-L'+str(provider.parent),'-lxlc','-Wl,-rpath,'+str(provider.parent),'-pthread','-ldl','-lm']
                objects=[]
                for number,source in enumerate(sources):
                    obj=work/(str(number)+'.o');compat=[]
                    if source.name in ('xl6_lexer_gen.c','xl6_parser.c'):
                        compat=['-Wno-unused-function','-Wno-unused-parameter','-Wno-unused-variable']
                    if source.name=='xl6_package.c':compat=['-Wno-unused-function']
                    code,data=run(name+'.compile.'+str(number),[cc,*flags,*compat,'-c',source,'-o',obj],work)
                    assert code==0,data[-6000:];objects.append(obj)
                code,data=run(name+'.build',[cc,'-Wl,--gc-sections',*objects,*links,'-o',binary],work);assert code==0,data[-6000:]
                products.append(dict(Path=str(binary),Sha256=sha(binary)))
                code,data=run(name+'.run',[binary],work)
                if control:
                    assert code==1 and b'unsafe source fallback: status=1 diskReads=1 injected=1' in data,data[-6000:]
                elif variant=='import':
                    assert code==1 and b'unsafe import resolution: status=1 injected=1' in data,data[-6000:]
                else:assert code==0 and b'source identity: PASS' in data,data[-6000:]
                proofs.append(dict(Name=name,OldUnsafeFallbackRejected=control,ImportErrorPropagationRejected=variant=='import',FixedSourceUnitPassed=variant=='fixed'))
    except Exception as e:failures.append(repr(e))
    unchanged=all(sha(r['Path'])==r['Sha256'] for r in inputs)
    complete=unchanged and len(proofs)==(6 if a.old_root else 4) and not failures
    report=dict(Complete=complete,InputsUnchanged=unchanged,
        Inputs=inputs,Products=products,Rows=rows,Proofs=proofs,Failures=failures,Scope=__doc__,
        SourceUnitAcceptance=complete,ActualCompilerProductAcceptance=False,Full53Acceptance=False)
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Proofs=len(proofs),Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
