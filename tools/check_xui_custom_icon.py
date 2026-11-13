"""Custom Icon owned callbacks and callback-scoped Painter direct declarations.

Uses real owning CPU Proxy/Font/Surface/Draw controls, not graphics or network.
Checks thirteen Painter methods, four callback slots, derived scope views,
reentrant unregister/close, callback/void-cleanup failures (including OOM),
80 registration budgets, and language/native 2000-call heap comparison.
Manual Painter sessions, active cache readers and module-cycle retirement are
separate work. The graphical learning example is emitted, not executed here.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
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
    case=root/'test/cases/foreign_xui_custom_icon.xl'
    provider=case.with_suffix('.c');example=root/'lib/bindings/xui/examples/icon_custom.xl'
    adapter=root/'test/cases/foreign_xui_render_adapters.inc'
    files=[compiler,build,Path(__file__),case,provider,case.with_suffix('.h'),example,
        example.with_name('_window.xl'),adapter,*extra,*tools.values(),
        root/'test/cases/foreign_xui_render_backend.c',root/'test/cases/foreign_xui_render_backend.h',
        root/'tools/gen_xui_render_fixture.py',Path(__file__).with_name('foreign_compiler_cohort.py'),
        Path(__file__).with_name('full_compiler_qualification.py'),
        native/'tests/xui/xui_test_proxy.c',native/'tests/xui/xui_test_proxy.h']
    files.extend(compiler.parent/n for n in (('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in (sdk,dynamic,static):files.extend(p for p in directory.rglob('*')if p.is_file())
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p)for p in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,fullCompilerBuild=True,
        shippingDelivery=False,networkTests=False,inputs=inputs,rows=[],programs=[],heap=[])
    def save():
        temporary=out/'results.json.tmp';temporary.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(temporary,out/'results.json')
    def run(name,command,packages=dynamic):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        command=list(map(str,command));start=time.monotonic()
        result=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=result.returncode,
            seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,result.returncode,flush=True);assert not result.returncode,log
        return result.stdout+result.stderr
    try:
        generated=out/'adapters.inc'
        run('adapter-source',[os.sys.executable,'-B',root/'tools/gen_xui_render_fixture.py','--native-root',native,'--out',generated])
        assert generated.read_text()==adapter.read_text(),'test adapters must match the native header'
        includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
        native_includes=['-I'+str(p)for p in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
        for source in (case,example):
            name=source.stem;c=out/(name+'.c');header=out/(name+'.h')
            # Same-stem companion discovery supplies this provider exactly once.
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes])
            text=c.read_text(encoding='utf-8-sig')
            assert re.search(r'\bxuiIconAddCustomOwned\(',text)
            if source==case:
                for symbol in ('GetDrawContext','ClearRect','DrawSurface','DrawSurfaceQuad','DrawMeshTriangles',
                    'FillPath','DrawPath','DrawSvgPath','FillRect','StrokeRect','DrawVectorIcon','DrawText','DrawNinePatch'):
                    assert re.search(r'\bxuiPainter'+symbol+r'\(',text),symbol
                for level in ('O0','O2'):
                    run('provider-'+level,[tools['gcc'],'-std=c11','-'+level,'-Wall','-Wextra','-Werror',*native_includes,'-fsyntax-only',provider])
            for standard,tool,language in (('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-public-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*native_includes,'-x',language,'-fsyntax-only',header])
        for kind,packages in (('jit',dynamic),('dynamic',dynamic),('static',static)):
            if kind=='jit':output=run(kind,[compiler,'run',case,*includes],packages)
            else:
                program=out/('custom-icon-'+kind+('.exe'if win else''))
                run(kind+'-build',[compiler,'build',case,'-o',program,*includes],packages)
                output=run(kind+'-execute',[program],packages)
                table=run(kind+'-imports',[tools['objdump'],'-p',program],packages)
                imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
                lower={n.lower()for n in imports}
                assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                assert any(b'xge3_host'in n for n in lower)==(kind=='dynamic')
                report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[n.decode()for n in imports]))
            assert b'four owned callbacks, thirteen Painter methods, backend checks and epoch expiry passed'in output
            assert b'reentrant unregister/close, callback failures and 80 registration OOM budgets passed'in output
            counts=re.findall(rb'Custom callback heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+)',output)
            assert len(counts)==2 and {row[0]for row in counts}=={b'language',b'native'}
            metrics={row[0].decode():dict(zip(('alloc','free','realloc'),map(int,row[1:])))for row in counts}
            assert all(value==0 for row in metrics.values()for value in row.values())
            report['heap'].append(dict(mode=kind,calls=2000,counts=metrics,zeroAllocationBudget=True))
            save()
        report.update(complete=True,passed=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()));assert report['inputsUnchanged']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
