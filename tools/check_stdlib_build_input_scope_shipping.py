"""Concurrent compiler contexts must not share their active input record.

Four native calling threads request independent real AOT builds and checked
receipts through the actual compiler library. The compiler API lock must keep
the disk reader, working directory and borrowed active record in one scope.
No replacement providers or full53/parallel-TCC/OOM acceptance is claimed.
"""
from pathlib import Path
import argparse,concurrent.futures,ctypes,hashlib,json,os,shutil,subprocess,sys,threading


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','build-report','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();assert not out.exists()
    windows=os.name=='nt';build=json.loads(a.build_report.read_text(encoding='utf-8-sig'))
    assert build['Complete'] and build['InputsUnchanged'] and build['Exit']==0
    names=('xl.exe','xl.dll','xlc.dll') if windows else ('xl','libxl.so','libxlc.so')
    products=[root/'release'/n for n in names]
    recorded={Path(r['Path']).resolve():r['Sha256'] for r in build['Products']}
    assert all(recorded.get(f)==sha(f) for f in products)
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in [Path(__file__).resolve(),Path(sys.executable),a.build_report.resolve(),*products]]
    out.mkdir();isolated=out/'isolated';isolated.mkdir()
    for f in products:shutil.copy2(f,isolated/f.name)
    library=ctypes.CDLL(str(isolated/names[2]))
    for name,args,result in (
        ('xlcCreateContext',[ctypes.c_void_p],ctypes.c_void_p),
        ('xlcSetWorkDir',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),
        ('xlcBeginInputReport',[ctypes.c_void_p],ctypes.c_int),
        ('xlcAddSourceFile',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),
        ('xlcAddCSourceFile',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),
        ('xlcEmitExe',[ctypes.c_void_p,ctypes.c_char_p],ctypes.c_int),
        ('xlcWriteInputReport',[ctypes.c_void_p,ctypes.c_char_p,ctypes.c_char_p],ctypes.c_int),
        ('xlcGetLastError',[ctypes.c_void_p],ctypes.c_char_p),
        ('xlcDestroyContext',[ctypes.c_void_p],None)):
        fn=getattr(library,name);fn.argtypes=args;fn.restype=result
    env={k:v for k,v in os.environ.items() if not k.upper().startswith('XL6_') and k.upper() not in
         ('CPATH','C_INCLUDE_PATH','LIBRARY_PATH','LD_LIBRARY_PATH','LD_PRELOAD','LD_AUDIT')}
    proof=[];failures=[];rows=[];artifacts=list(isolated.glob('*'));guard=threading.Lock()
    def worker(number,barrier):
        directory=out/('context-'+str(number));directory.mkdir()
        source=directory/'entry.xl';native=directory/'helper.c';header=directory/'value.h'
        source.write_text('extern "scope_value" func value() as int\nfunc main(...) as int\n assert value() == '+str(number)+'\n return 0\nend\n',encoding='utf-8')
        native.write_text('#include <stdint.h>\n#include "value.h"\nint64_t scope_value(void) { return SCOPE_VALUE; }\n',encoding='utf-8')
        header.write_text('#define SCOPE_VALUE '+str(number)+'\n',encoding='utf-8')
        expected={f.resolve():sha(f) for f in (source,native,header)}
        ctx=library.xlcCreateContext(None);assert ctx
        encode=lambda path:str(path).encode('utf-8')
        def checked(code):assert code==1,(number,library.xlcGetLastError(ctx))
        try:
            checked(library.xlcSetWorkDir(ctx,encode(directory)))
            checked(library.xlcBeginInputReport(ctx))
            checked(library.xlcAddSourceFile(ctx,encode(source)))
            checked(library.xlcAddCSourceFile(ctx,encode(native)))
            barrier.wait(timeout=60)
            image=directory/('program.exe' if windows else 'program');receipt=directory/'inputs.json'
            checked(library.xlcEmitExe(ctx,encode(image)))
            checked(library.xlcWriteInputReport(ctx,encode(image),encode(receipt)))
            value=json.loads(receipt.read_text(encoding='utf-8'));assert value['complete']
            files={Path(f['path']).resolve():f['sha256'] for f in value['files']}
            assert all(files.get(f)==digest for f,digest in expected.items())
            assert all('context-' not in str(f) or f.is_relative_to(directory) for f in files)
            result=subprocess.run([str(image)],cwd=directory,env=env,capture_output=True,timeout=120)
            log=directory/'run.log';log.write_bytes(result.stdout+result.stderr);assert result.returncode==0
            with guard:
                artifacts.extend([source,native,header,image,receipt])
                rows.append(dict(Name='context-'+str(number),Exit=result.returncode,Log=str(log),LogSha256=sha(log)))
                proof.append(dict(Context=number,ExactOwnInputs=True,OtherContextsExcluded=True,ActualAotRan=True,Receipt=str(receipt)))
        finally:library.xlcDestroyContext(ctx)
    for round_number in range(2):
        barrier=threading.Barrier(4)
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            futures=[pool.submit(worker,1+round_number*4+i,barrier) for i in range(4)]
            for future in futures:
                try:future.result(timeout=600)
                except Exception as error:failures.append(repr(error))
    unchanged=all(sha(r['Path'])==r['Sha256'] for r in inputs)
    complete=unchanged and not failures and len(proof)==8
    result=dict(Complete=complete,InputsUnchanged=unchanged,Inputs=inputs,Rows=rows,Proofs=proof,Failures=failures,
                Products=[dict(Path=str(f),Sha256=sha(f)) for f in sorted(artifacts)],Scope=__doc__,
                CallingThreads=4,Rounds=2,ActiveInputScopeAcceptance=complete,Full53Acceptance=False)
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=complete,Proofs=len(proof),Failures=failures)),flush=True)
    return 0 if complete else 1


if __name__=='__main__':raise SystemExit(main())
