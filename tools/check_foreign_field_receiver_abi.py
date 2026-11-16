"""Ensure the actual embedded TCC rejects native receiver/header mismatches.

Invalid programs are compiled only; they must never reach execution. The
complete O2 compiler, SDK headers and fixtures are pinned by exact hashes.
"""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,time
from full_compiler_qualification import verify_full_compiler


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','compiler','compiler-report','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,compiler,build,out=[v.resolve() for v in (a.root,a.compiler,a.compiler_report,a.out)]
    verify_full_compiler(build,compiler);win=os.name=='nt';sdk=compiler.parent/'include'
    source=root/'test/cases/foreign_field_receiver.xl';native=source.with_name('foreign_field_receiver_native.c');header=source.with_suffix('.h')
    sha=lambda v:hashlib.sha256(Path(v).read_bytes()).hexdigest()
    files=[source,native,header,compiler,build,Path(__file__),Path(__file__).with_name('full_compiler_qualification.py')]
    files += [compiler.parent/n for n in (('xl.dll','xlc.dll') if win else ('libxl.so','libxlc.so'))]
    files += [v for v in sdk.rglob('*') if v.is_file()]
    inputs={str(v):sha(v) for v in files};out.mkdir(parents=True,exist_ok=False)
    env={k:v for k,v in os.environ.items() if not k.startswith('XL6_') and k!='XLANG_PACKAGE_PATH'}
    env.update(TMP=str(out),TEMP=str(out),TMPDIR=str(out))
    if not win:env['LD_LIBRARY_PATH']=str(compiler.parent)+':'+env.get('LD_LIBRARY_PATH','')
    report=dict(complete=False,passed=False,scope=__doc__,inputs=inputs,rows=[],fullCompilerBuild=True,networkTests=False)
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    try:
        for name,old,new,diagnostic in (
            ('receiver-type','void* pUser;','const void* pUser;',b'native callable receiver field abi'),
            ('function-type','int (*add)(void*,int);','int (*add)(ReceiverTable*,int);',b'native callable field abi'),
            ('missing-field','void* pUser;','void* absent;',b'puser')):
            folder=out/name;(folder/'test/cases').mkdir(parents=True)
            case=folder/'case.xl';case.write_bytes(source.read_bytes())
            h=folder/'test/cases/foreign_field_receiver.h';h.write_text(header.read_text().replace(old,new))
            inputs[str(case)]=sha(case);inputs[str(h)]=sha(h)
            command=list(map(str,[compiler,'run',case,native,'--inc-path',folder,'--inc-path',root,'--inc-path',sdk]))
            start=time.monotonic();r=subprocess.run(command,cwd=folder,env=env,capture_output=True,timeout=240)
            raw=r.stdout+r.stderr;log=out/(name+'.log');log.write_bytes(raw)
            report['rows'].append(dict(name=name,command=command,cwd=str(folder),exitCode=r.returncode,expectedFailure=True,logSha256=sha(log),seconds=time.monotonic()-start))
            save();print(name,r.returncode,flush=True)
            assert r.returncode and b'FIELD_RECEIVER_OK' not in raw and diagnostic in raw.lower(),log
        verify_full_compiler(build,compiler)
        report.update(complete=True,inputsUnchanged=all(sha(v)==h for v,h in inputs.items()))
        report['passed']=report['inputsUnchanged'];assert report['passed']
    except Exception as error:
        report.update(complete=True,passed=False,error=str(error));raise
    finally:save()


if __name__=='__main__':main()
