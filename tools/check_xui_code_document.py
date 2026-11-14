"""Complete-compiler unique CodeDocument and owned change callback acceptance.

Actual model, callback/epoch/error/OOM/thread and prepared-payload heap counts;
local UTF8 save/load and callback autosave. JIT and independent dynamic/static
programs, C11/C++11/C++17 headers, readable console lesson. No graphics, remote
services or network/security tests. This does not install shipping artifacts.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','native-root','dynamic-packages','static-packages','out'):
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,build,native,dynamic,static,out=[v.resolve()for v in
        (a.root,a.compiler,a.compiler_report,a.native_root,a.dynamic_packages,a.static_packages,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    case=root/'test/cases/foreign_xui_code_document.xl';example=root/'lib/bindings/xui/examples/code_document.xl'
    files=[Path(__file__).resolve(),compiler,build,*extra,*tools.values(),case,case.with_suffix('.c'),case.with_suffix('.h'),example,
        native/'include/xui.h',native/'src/xui/code/xui_code_document.c',native/'src/xui/code/xui_code_document_owned.inl',
        native/'src/xui/code/xui_code_edit.c',Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py')]
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    for directory in(sdk,dynamic,static):files.extend(v for v in directory.rglob('*')if v.is_file())
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest();inputs={str(v):sha(v)for v in files}
    out.mkdir(parents=True,exist_ok=False);snapshots=out/'input-snapshots';snapshots.mkdir()
    for v in[Path(__file__).resolve(),case,case.with_suffix('.c'),case.with_suffix('.h'),example]:shutil.copy2(v,snapshots/v.name)
    report=dict(complete=False,passed=False,scope=__doc__,shippingDelivery=False,fullCompilerBuild=True,networkTests=False,
        inputs=inputs,rows=[],programs=[],heap=[])
    def save():
        target=out/'results.json.tmp';target.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(target,out/'results.json')
    def run(name,command,packages):
        env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
        env['XLANG_PACKAGE_PATH']=str(packages);key='PATH'if win else'LD_LIBRARY_PATH'
        env[key]=str(packages/'_xge_native/native')+os.pathsep+env.get(key,'')
        env['XUI_CODE_DOCUMENT_PATH']=str(out/(name+'.txt'))
        command=list(map(str,command));start=time.monotonic()
        z=subprocess.run(command,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=command,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log;return z.stdout+z.stderr
    includes=['--inc-path',root,'--inc-path',sdk,'--inc-path',native]
    c_includes=['-I'+str(v)for v in(root,sdk,native,native/'include',*sorted(dynamic.glob('*/include')))]
    try:
        for source in(case,example):
            name=source.stem;lesson=source==example;c=out/(name+'.c');header=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,'--emit',c,'--emit-header',header,*includes],dynamic)
            generated=c.read_text(encoding='utf-8-sig')
            symbols=['xuiCodeDocumentCreate','xuiCodeDocumentSetTextLength','xuiCodeDocumentGetTextView',
                'xuiCodeDocumentSetChangeCallbackOwned','xuiCodeDocumentBeginEdit','xuiCodeDocumentEndEdit','xuiCodeDocumentUndo','xuiCodeDocumentRedo']
            if not lesson:symbols+=['xuiCodeDocumentCopyRange','xuiCodeDocumentGetByte','xuiCodeDocumentLoadTextFile','xuiCodeDocumentSaveTextFile']
            for symbol in symbols:assert re.search(r'\b'+symbol+r'\(',generated),symbol
            assert not re.search(r'\bxuiCodeDocument(?:GetText|SetChangeCallback)\(',generated)
            if not lesson:
                for level in('O0','O2'):
                    run('provider-'+level,[tools['gcc'],'-std=c11','-'+level,'-Wall','-Wextra','-Werror',*c_includes,'-fsyntax-only',case.with_suffix('.c')],dynamic)
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_includes,'-x',language,'-fsyntax-only',header],dynamic)
            for kind,packages in(('jit',dynamic),('dynamic',dynamic),('static',static)):
                if kind=='jit':output=run(name+'-'+kind,[compiler,'run',source,*includes],packages)
                else:
                    binary=out/(name+'-'+kind+('.exe'if win else''))
                    run(name+'-'+kind+'-build',[compiler,'build',source,'-o',binary,*includes],packages)
                    output=run(name+'-'+kind+'-execute',[binary],packages)
                    table=run(name+'-'+kind+'-imports',[tools['objdump'],'-p',binary],packages)
                    imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table);lower={v.lower()for v in imports}
                    assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
                    assert any(b'xge3_host'in v for v in lower)==(kind=='dynamic')
                    report['programs'].append(dict(path=str(binary),sha256=sha(binary),bytes=binary.stat().st_size,imports=[v.decode()for v in imports]))
                if not lesson:
                    assert b'CodeDocument: counted text, history, typed outputs, owned callback reentry/error/expiry, OOM and thread checks passed'in output
                    counts=re.findall(rb'CodeDocument heap (language|native): alloc=(\d+) free=(\d+) realloc=(\d+)',output)
                    assert len(counts)==2 and all(tuple(map(int,row[1:]))==(2000,2000,1000)for row in counts)
                    report['heap'].append(dict(mode=kind,rounds=1000,preparedPayload=True,
                        counts={row[0].decode():dict(zip(('alloc','free','realloc'),map(int,row[1:])))for row in counts},cpuEquivalenceClaim=False))
                else:
                    for marker in('合并编辑：你好, xlang! 欢迎！','撤销一步：Hello, xlang!','重新应用：你好, xlang! 欢迎！'):
                        assert marker.encode('utf-8')in output,output
                save()
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()));report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
