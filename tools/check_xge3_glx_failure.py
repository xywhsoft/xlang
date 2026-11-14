"""Full-compiler actual Linux GLX absence and healthy display recovery.
Two local Xvfb servers with TCP disabled; one has GLX explicitly disabled.
JIT and independent dynamic/static AOT with unchanged full compiler. No
replacement engine, display interposer, remote service or security tests.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,select,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','native-root','dynamic-packages','static-packages','out'):
        parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();assert os.name!='nt','Actual Linux GLX failure gate only'
    root,compiler,build,native,dynamic,static,out=[p.resolve()for p in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build)
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xge3_glx_failure.xl'
    marker=b'GLX startup: 12 checked failures, 3 same-process actual window recoveries and manual mode passed'
    files=[Path(__file__),compiler,build,case,*extra,*tools.values(),
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py'),
        native/'extlibs/sokol/sokol_app.h',native/'src/xge/core/xge_impl.c']
    files.extend(compiler.parent/n for n in('libxl.so','libxlc.so'))
    for folder in(sdk,dynamic,static):files.extend(p for p in folder.rglob('*')if p.is_file())
    files.append(Path(shutil.which('Xvfb')).resolve())
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    inputs={str(p):sha(p)for p in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,
        networkTests=False,windowTests=True,inputs=inputs,rows=[],programs=[])
    def save():
        p=out/'results.json.tmp';p.write_text(json.dumps(report,indent=2)+'\n');os.replace(p,out/'results.json')
    servers=[];server_logs=[];displays={}
    try:
        for key,extra in [('bad',['-extension','GLX']),('good',[])]:
            log=(out/('xvfb-'+key+'.log')).open('wb');server_logs.append(log)
            server=subprocess.Popen(['Xvfb','-displayfd','1','-screen','0','640x480x24','-nolisten','tcp','-noreset',*extra],stdout=subprocess.PIPE,stderr=log);servers.append(server)
            assert select.select([server.stdout],[],[],15)[0];number=server.stdout.readline().decode().strip();assert number.isdigit();displays[key]=':'+number
        def run(name,command,packages):
            env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k not in('XLANG_PACKAGE_PATH','DISPLAY')}
            env['XLANG_PACKAGE_PATH']=str(packages)
            env.update(DISPLAY=displays['bad'],XGE_BAD_DISPLAY=displays['bad'],XGE_GOOD_DISPLAY=displays['good'],LIBGL_ALWAYS_SOFTWARE='1')
            env['LD_LIBRARY_PATH']=str(packages/'_xge_native/native')+os.pathsep+env.get('LD_LIBRARY_PATH','')
            command=list(map(str,command));start=time.monotonic()
            z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
            log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
            report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,
                seconds=time.monotonic()-start,logSha256=sha(log)))
            save();print(name,z.returncode,flush=True);assert not z.returncode,log
            return z.stdout+z.stderr
        includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
        c_includes=['-I'+str(p)for p in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
        header=out/'public.h';c=out/'generated.c'
        run('emit',[compiler,'cgen',case,'--emit',c,'--emit-header',header,*includes],dynamic)
        assert re.search(r'\bxl6Xge3RunOwned\(',c.read_text(encoding='utf-8-sig'))
        for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
            run('header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_includes,'-x',language,'-fsyntax-only',header],dynamic)
        for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
            if kind=='jit':output=run(kind,[compiler,'run',case,*includes],packages)
            else:
                program=out/kind
                run(kind+'-build',[compiler,'build',case,'-o',program,*includes],packages)
                output=run(kind+'-execute',[program],packages)
                table=run(kind+'-imports',[tools['objdump'],'-p',program],packages)
                imports=re.findall(rb'\bNEEDED\s+(\S+)',table);lower={n.lower()for n in imports}
                assert not {b'libxl.so',b'libxlc.so',b'libtcc.so'}.intersection(lower)
                assert any(b'xge3_host'in n for n in lower)==(kind=='dynamic')
                report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[n.decode()for n in imports]))
            assert marker in output
        report.update(complete=True,inputsUnchanged=all(sha(p)==h for p,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:
        for server in servers:server.terminate();server.wait(timeout=10)
        for log in server_logs:log.close()
        save()


if __name__=='__main__':main()
