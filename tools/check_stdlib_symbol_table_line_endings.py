"""Windows generated symbol-list EOL acceptance and semantic rejection.
Only isolated copies are modified. This does not relax XRT header/metadata
byte hashes and is not a DLL/runtime execution test.
"""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,sys

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();root,out=a.root.resolve(),a.out.resolve();assert os.name=='nt' and not out.exists()
    ps=shutil.which('powershell');assert ps
    header=root/'lib/xruntime/xruntime.h';generator=root/'tools/gen_xrt_symbol_table.py';checker=root/'tools/gen_xrt_symbol_table.ps1'
    inputs=[dict(Path=str(f),Sha256=sha(f)) for f in (Path(__file__).resolve(),Path(sys.executable),Path(ps),header,generator,checker)]
    spec=importlib.util.spec_from_file_location('symbol_render',generator);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    expected=module._render(header);count=len(module._api_names(header));assert count>1000
    first='XL_XRT_SYMBOL('+module._api_names(header)[0]+')';assert expected.count(first)==1
    cases=(('lf',expected.encode(),True),('crlf',expected.replace('\n','\r\n').encode(),True),
        ('renamed',expected.replace(first,'XL_XRT_SYMBOL(xrtMissingSymbolForTest)').encode(),False),
        ('duplicate',(expected+first+'\n').encode(),False),('lone-cr',expected.replace('\n','\r').encode(),False))
    out.mkdir();isolated=out/'source';(isolated/'lib/xruntime').mkdir(parents=True);(isolated/'src/api').mkdir(parents=True)
    shutil.copy2(header,isolated/'lib/xruntime/xruntime.h');rows=[];failures=[]
    try:
        for name,data,accepted in cases:
            (isolated/'src/api/xl_xrt_symbols.inc').write_bytes(data)
            command=[ps,'-NoProfile','-ExecutionPolicy','Bypass','-File',str(checker),'-Root',str(isolated),'-Check']
            result=subprocess.run(command,capture_output=True,timeout=60)
            log=out/(name+'.log');log.write_bytes(result.stdout+result.stderr)
            assert (result.returncode==0)==accepted,(name,result.returncode)
            if not accepted:assert b'stale' in result.stderr
            rows.append(dict(Name=name,ExpectedAccepted=accepted,Exit=result.returncode,Log=str(log),LogSha256=sha(log)))
    except Exception as e:failures.append(repr(e))
    unchanged=all(sha(f['Path'])==f['Sha256'] for f in inputs)
    report=dict(Complete=unchanged and len(rows)==5 and not failures,InputsUnchanged=unchanged,Inputs=inputs,Rows=rows,
        Failures=failures,ApiCount=count,Scope=__doc__,HeaderMetadataHashRelaxed=False,RuntimeAcceptance=False,Full53Acceptance=False)
    (out/'results.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(Complete=report['Complete'],Cases=len(rows),ApiCount=count,Failures=failures)),flush=True)
    return 0 if report['Complete'] else 1

if __name__=='__main__':raise SystemExit(main())
