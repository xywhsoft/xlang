"""Actual function semantic reuse across unrelated header edits over stdio.

Recorded readsets are creation provenance of an immutable accepted result.
Repeating a cached request must preserve them,not report additional work.
Full compiler AST/index/report/IR parity and resource/lifetime guards live in
analysis_semantic_reads_test;this suite exercises real unsaved editor inputs.
"""
from pathlib import Path
import argparse,hashlib,json,time
from check_lsp_automatic_diagnostics import AutomaticClient


def source(helpers,version):
    argument='input' if version>=2 else 'value'
    text=f'\ufeff// 中文😀\r\nfunc identity({"string" if version==7 else "int"} {argument}) as int\r\n return {"0" if version==7 else argument}\r\nend\r\n'
    if version>=3:text+=f'func identity(string value,int pad={2 if version>=4 else 1}) as int\r\n return pad\r\nend\r\n'
    for i in range(helpers):
        extra=' int extra=value+1\r\n' if version>=6 and i==5 else ''
        text+=f'func work_{i}(int value) as int\r\n int local=identity(value)\r\n{extra} if local > 0 then\r\n return local\r\n end\r\n return value\r\nend\r\n'
    unused='longer_input' if version>=9 else 'input' if version else 'value'
    return text+f'func {"spare" if version>=5 else "unused"}(int {unused}) as int\r\n return {unused}\r\nend\r\nfunc main(...) as int\r\n return work_0(7)\r\nend\r\n'


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    args=p.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(server):sha(server)};cases=[];commands=[]
    report=dict(status='failed',scope=__doc__,cases=cases,commands=commands,protected=protected)
    try:
        for helpers in [16,96]:
            folder=out/str(helpers);folder.mkdir();path=folder/'未保存😀.xl';other=folder/'independent.xl'
            for f in [path,other]:f.write_bytes(b'// immutable disk;the actual source is an editor overlay\n');protected[str(f)]=sha(f)
            c=AutomaticClient(server,folder/'absent/xl.exe',folder)
            def check(target,version):
                until=time.monotonic()+30
                while True:
                    c.request+=1;c.send('xlang/check',dict(textDocument=dict(uri=target.as_uri()),version=version),c.request);r,_=c.reply(c.request)
                    if r.get('error',{}).get('code')!=-32001:break
                    assert time.monotonic()<until;time.sleep(.002)
                assert 'error' not in r,r
                r=r['result'];assert r['version']==version and r['analysis']['workerThreadId']!=r['analysis']['captureThreadId'],r
                return r
            try:
                c.send('initialize',dict(capabilities={},initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False)),1)
                init,_=c.reply(1);assert init['result']['serverInfo']['version']=='0.80';c.send('initialized',{})
                for v in range(10):
                    text=source(helpers,v)
                    if not v:c.open(path,text,1)
                    else:c.change(path,text,v+1)
                    r=check(path,v+1);a=r['analysis'];reads=a['semanticReads'];assert r['ok']==(v!=7) and (r['count']==0)==(v!=7),r
                    if v==0:assert reads['observations']>1000 and reads['typedComparisons']>1000 and reads['scratchReuses']>0 and 0<reads['scratchBytes']<=8*1024*1024 and 0<reads['valueBytes']<=1024*1024,reads
                    if v in [1,5,9]:assert reads['contextHits'] and reads['reused']>=helpers and reads['matched'],reads
                    if v in [2,4]:assert reads['contextHits'] and reads['rejected']>=helpers,reads
                    if v==3:assert not reads['contextHits'] and not reads['reused'],reads
                    if v==6:assert not reads['reused'] and reads['contextHits'],reads
                    cases.append(dict(helpers=helpers,phase='edited',generation=v,analysis=a))
                    repeated=check(path,v+1);assert repeated['analysis']['builds']==a['builds'] and repeated['analysis']['semanticReads']==reads
                    cases.append(dict(helpers=helpers,phase='exact',generation=v,analysis=repeated['analysis']))
                c.open(other,source(helpers,0),1);independent=check(other,1);assert not independent['analysis']['semanticReads']['contextHits']
                cases.append(dict(helpers=helpers,phase='independent-root',analysis=independent['analysis']))
                original=check(path,10);assert original['analysis']['semanticReads']==reads
                cases.append(dict(helpers=helpers,phase='held-root',analysis=original['analysis']))
            finally:
                stderr=c.stop();(folder/'stderr.log').write_text(stderr,encoding='utf8');(folder/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
                commands.append(dict(server=str(server),exit=c.process.returncode));assert not c.process.returncode
        assert len(cases)==44 and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(out/'result.json'))))


if __name__=='__main__':main()
