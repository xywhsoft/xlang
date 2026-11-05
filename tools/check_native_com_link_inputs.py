"""Check Win64 OLE import inputs by compilation and PE inspection only.

No generated executable, COM, UI or clipboard operation is run.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,time

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','tcc','tcc-dist','out'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();root=a.root.resolve();out=a.out.resolve();out.mkdir(exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    fixture=root/'test/cases/native_com_ole32_import.c';definition=root/'res/tcc/win64/x86_64/lib/ole32.def'
    assert definition.read_text().split()==['LIBRARY','OLE32.dll','EXPORTS','ReleaseStgMedium']
    inputs={str(p):sha(p)for p in [Path(__file__).resolve(),fixture,definition,root/'src/runtime/xl6_native_com_win_abi.h',a.tcc,root/'build_dll.bat']}
    assert '-lOle32' in (root/'build_dll.bat').read_text()
    report=dict(complete=False,passed=False,shippingDelivery=False,scope=__doc__,generatedProgramExecuted=False,inputs=inputs,rows=[])
    def save():(out/'results.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    def run(name,command):
        start=time.monotonic();result=subprocess.run(list(map(str,command)),capture_output=True,timeout=180);log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr);report['rows'].append(dict(name=name,exitCode=result.returncode,command=list(map(str,command)),seconds=round(time.monotonic()-start,3),logSha256=sha(log)));save();print(name,result.returncode,flush=True);assert not result.returncode,log
    try:
        target=out/'ole32-link-only.exe'
        run('tcc-link',[a.tcc,'-B'+str(a.tcc_dist),'-std=c11','-I'+str(root),fixture,'-L'+str(definition.parent),'-lole32','-o',target])
        run('pe-import-inspection',[shutil.which('objdump'),'-p',target])
        text=(out/'pe-import-inspection.log').read_text();assert 'DLL Name: OLE32.dll' in text and 'ReleaseStgMedium' in text
        report['generatedPeSha256']=sha(target);report.update(complete=True,passed=True)
    except Exception as error:report['error']=str(error);raise
    finally:report['inputsUnchanged']=all(sha(p)==v for p,v in inputs.items());save()
if __name__=='__main__':main()
