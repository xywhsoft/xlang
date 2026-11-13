"""Direct trivial C output storage with unchanged transactional owning outputs.

Requires a complete O2 compiler/TCC/XRT/VFS cohort. Executes sized partial POD
writes, tuple/array outputs, bound/protocol calls, aliased source destinations,
native failure and 4000 allocation-free calls. Separate borrowed/owned text,
bytes and projected-record fixtures preserve conversion/cleanup coverage.
CPU only; no device, window, network or shipping installation.
"""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,subprocess,time
from foreign_compiler_cohort import load_compiler_cohort


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in('root','compiler','compiler-report','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,build,out=[v.resolve()for v in(a.root,a.compiler,a.compiler_report,a.out)]
    sdk,extra=load_compiler_cohort(root,compiler,build);win=os.name=='nt'
    tools={n:Path(shutil.which(n)).resolve()for n in('gcc','g++','objdump')}
    cases={
        'foreign_output_storage':[],
        'foreign_output_values':['foreign_output_values_native.c'],
        'foreign_fixed_output':[],
        'foreign_output_record':['foreign_output_record_native.c'],
        'foreign_xge3_binding_output_view':[],
        'foreign_owned_bytes':[],
        'foreign_owned_text':[],
    }
    files=[Path(__file__),compiler,build,*extra,*tools.values(),
        Path(__file__).with_name('foreign_compiler_cohort.py'),Path(__file__).with_name('full_compiler_qualification.py')]
    # Pin actual quoted fixture includes without depending on unrelated cases.
    pending=[]
    for name,providers in cases.items():
        source=root/'test/cases'/(name+'.xl')
        pending.extend(v for v in(source,source.with_suffix('.c'),source.with_suffix('.h'))if v.is_file())
        pending.extend(root/'test/cases'/v for v in providers)
    seen=set()
    while pending:
        source=pending.pop().resolve()
        if source in seen:continue
        seen.add(source);files.append(source)
        for relative in re.findall(r'#\s*include\s*"([^"]+)"',source.read_text(encoding='utf-8-sig')):
            dependency=next((v for v in(source.parent/relative,root/relative)if v.is_file()),None)
            if dependency:pending.append(dependency)
    files.extend(compiler.parent/n for n in(('xl.dll','xlc.dll')if win else('libxl.so','libxlc.so')))
    files.extend(v for v in sdk.rglob('*')if v.is_file())
    sha=lambda q:hashlib.sha256(Path(q).read_bytes()).hexdigest()
    inputs={str(q):sha(q)for q in files};out.mkdir(parents=True,exist_ok=False)
    report=dict(complete=False,passed=False,shippingDelivery=False,fullCompilerBuild=True,
        scope=__doc__,inputs=inputs,rows=[],programs=[],oracles=[])
    env={k:v for k,v in os.environ.items()if not k.startswith('XL6_')and k!='XLANG_PACKAGE_PATH'}
    key='PATH'if win else'LD_LIBRARY_PATH';env[key]=str(compiler.parent)+os.pathsep+env.get(key,'')
    def save():
        q=out/'results.json.tmp';q.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');os.replace(q,out/'results.json')
    def run(name,cmd):
        cmd=list(map(str,cmd));start=time.monotonic();z=subprocess.run(cmd,cwd=root,env=env,capture_output=True,timeout=300)
        log=out/(name+'.log');log.write_bytes(z.stdout+z.stderr)
        report['rows'].append(dict(name=name,command=cmd,exitCode=z.returncode,seconds=time.monotonic()-start,logSha256=sha(log)))
        save();print(name,z.returncode,flush=True);assert not z.returncode,log;return z.stdout+z.stderr
    include=['--inc-path',root,'--inc-path',sdk]
    c_include=['-I'+str(root),'-I'+str(sdk)]
    try:
        for name,providers in cases.items():
            source=root/'test/cases'/(name+'.xl');providers=[root/'test/cases'/v for v in providers]
            c=out/(name+'.c');h=out/(name+'.h')
            run(name+'-emit',[compiler,'cgen',source,*providers,'--emit',c,'--emit-header',h,*include])
            text=c.read_text(encoding='utf-8-sig')
            if name=='foreign_output_storage':
                assert 'native_raw'not in text
                for symbol in('storage_read','storage_pair'):
                    calls=[line for line in text.splitlines()if ' = '+symbol+'('in line]
                    assert calls and all('native_raw'not in line for line in calls)
                assert re.search(r'v\d+\.size = sizeof\(v\d+\)',text)
                assert re.search(r'v\d+\.e0\.size = sizeof\(v\d+\.e0\)',text)
                assert re.search(r'v\d+\.e1\.size = sizeof\(v\d+\.e1\)',text)
                report['oracles'].append(dict(case=name,directSizedRecordAndTuple=True,noRawResultTemporary=True))
                for level in('O0','O2'):
                    run('storage-provider-'+level,[tools['gcc'],'-std=c11','-'+level,'-Wall','-Wextra','-Werror',*c_include,'-fsyntax-only',source.with_suffix('.c')])
            elif name=='foreign_fixed_output':
                assert re.search(r'\bv\d+\.Elements',text)and re.search(r'\bv\d+\.e0\.Elements',text)
                calls=[line for line in text.splitlines()if ' = fixed_vertices('in line]
                assert calls and all('native_raw'not in line for line in calls)
                report['oracles'].append(dict(case=name,directFixedArrays=True))
            elif name=='foreign_output_record':
                assert 'OutputRecordNative native_raw'in text and 'native_admitted'in text
                report['oracles'].append(dict(case=name,projectionTransactionRetained=True))
            elif name=='foreign_xge3_binding_output_view':
                assert 'native_raw'in text and 'native_admitted'in text
                report['oracles'].append(dict(case=name,borrowedViewTransactionRetained=True))
            elif name in('foreign_owned_bytes','foreign_owned_text'):
                assert 'native_raw'in text
                assert ('xl6COutputTextTakeV1'in text)if name.endswith('text')else('xl6COutputBytesTakeV1'in text)
                report['oracles'].append(dict(case=name,ownedOutputTransactionRetained=True))
            for standard,tool,language in(('c11','gcc','c'),('c++11','g++','c++'),('c++17','g++','c++')):
                run(name+'-header-'+standard,[tools[tool],'-std='+standard,'-Wall','-Wextra','-Werror',*c_include,'-x',language,'-fsyntax-only',h])
            result=run(name+'-jit',[compiler,'run',source,*providers,*include])
            if name=='foreign_output_storage':assert b'4000 zero-heap native calls passed'in result
            program=out/(name+('.exe'if win else''))
            run(name+'-build',[compiler,'build',source,*providers,'-o',program,*include])
            result=run(name+'-execute',[program])
            if name=='foreign_output_storage':assert b'4000 zero-heap native calls passed'in result
            table=run(name+'-imports',[tools['objdump'],'-p',program])
            imports=re.findall(rb'DLL Name:\s*(\S+)',table)if win else re.findall(rb'\bNEEDED\s+(\S+)',table)
            lower={n.lower()for n in imports}
            assert not {b'xl.dll',b'xlc.dll',b'libxl.so',b'libxlc.so',b'libtcc.dll',b'libtcc.so'}.intersection(lower)
            report['programs'].append(dict(path=str(program),sha256=sha(program),bytes=program.stat().st_size,imports=[n.decode()for n in imports]));save()
        report.update(complete=True,inputsUnchanged=all(sha(q)==h for q,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as e:report.update(complete=True,passed=False,error=str(e));raise
    finally:save()


if __name__=='__main__':main()
