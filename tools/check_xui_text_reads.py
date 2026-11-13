"""CPU borrowed-text acceptance with a full compiler and matched XGE SDK.

Runs representative real CString models and sized CodeEdit reads plus the
generic malformed-view/status/OOM fixture in JIT and independent dynamic/static
programs. All declared CString getters are linked as ABI probes, not executed.
No rendering, network tests or shipping SDK/IDE installation.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,sys,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','native-root','dynamic-packages','static-packages','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    root,compiler,build,native,dynamic,static,out=[p.resolve()for p in
        (args.root,args.compiler,args.compiler_report,args.native_root,args.dynamic_packages,args.static_packages,args.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={name:Path(shutil.which(name)).resolve()for name in ('gcc','g++','objdump')}
    catalogue=root/'test/fixtures/foreign_xge_cstrings/declared_getters.xl'
    case=root/'test/cases/foreign_xui_text_reads.xl'
    generic=root/'test/cases/foreign_xge3_binding_output_view.xl'
    providers={case:[case.with_suffix('.c'),root/'test/cases/foreign_xui_icon_models.c'],generic:[generic.with_suffix('.c')]}
    files=[compiler,build,Path(__file__),*extra,*tools.values(),catalogue,catalogue.with_suffix('.json'),
        Path(__file__).with_name('gen_xge_cstring_probes.py'),Path(__file__).with_name('check_xge3_full_surface.py'),
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h',root/'test/cases/foreign_xui_icon_models.h']
    for source,cs in providers.items():files.extend([source,source.with_suffix('.h'),*cs])
    files.extend(compiler.parent/n for n in (('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in (sdk,dynamic,static):files.extend(p for p in directory.rglob('*')if p.is_file())
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p)for p in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,fullCompilerBuild=True,shippingDelivery=False,networkTests=False,inputs=inputs,rows=[],programs=[])
    def save():
        p=out/'results.json.tmp';p.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(p,out/'results.json')
    def run(name,command,packages=dynamic):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log;return z.stdout+z.stderr
    def imports(name,program,kind):
        table=run(name+'-imports',[tools['objdump'],'-p',program])
        names=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
        lower={n.lower()for n in names}
        assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
        assert any(b'xge3_host'in n for n in lower)==(kind=='dynamic')
        report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[n.decode()for n in names]))
    try:
        includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
        for source in (catalogue,case,generic):
            name=source.stem;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes])
            text=c.read_text(encoding='utf-8-sig')
            if source==case:
                assert 'xuiCodeEditGetTextView('in text and 'xuiEditGetTextView('in text
                assert not re.search(r'\bxui(?:CodeEdit|Edit)GetText\(',text)
            if source==catalogue:
                mapping=json.loads(catalogue.with_suffix('.json').read_text())
                report['catalogue']=mapping
                module=out/('cstring-getters.exe'if win else'cstring-getters')
                run(name+'-link',[compiler,'build',source,'-o',module,*includes])
                imports(name,module,'dynamic')
                run(name+'-bootstrap',[module])
            else:
                for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
                    # The compiler automatically adds a same-stem C companion.
                    additional=[p for p in providers[source]if p!=source.with_suffix('.c')]
                    if kind=='jit':output=run(name+'-'+kind,[compiler,'run',source,*additional,*includes],packages)
                    else:
                        program=out/(name+'-'+kind+('.exe'if win else''))
                        run(name+'-'+kind+'-build',[compiler,'build',source,*additional,'-o',program,*includes],packages)
                        output=run(name+'-'+kind+'-execute',[program],packages)
                        # The generic fixture has no XGE dependency of its own.
                        imports(name+'-'+kind,program,kind if source==case else'static')
                    if source==case:assert b'Text reads: sized NUL snapshots, CString queries, checked native/copy OOM and lifetime passed'in output
            native_includes=['-I'+str(p)for p in(root,sdk,sdk/'include',*sorted(dynamic.glob('*/include')))]
            for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-public-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*native_includes,'-x',language,'-fsyntax-only',header])
        report['inputsUnchanged']=all(sha(p)==h for p,h in inputs.items());assert report['inputsUnchanged']
        report.update(complete=True,passed=True)
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
