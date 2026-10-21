"""Exercise resident folding over real stdio, with no usable compiler executable."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import time

from check_language_snapshots import Client

SOURCE = ("class Box\n int value\n func getValue() as int\n  return self.value\n end\nend\n"
          "func main(...) as int\n if true\n  return 1\n else\n  return 0\n end\nend\n")
RANGES = [dict(startLine=a,endLine=b) for a,b in [(0,5),(2,4),(6,12),(7,11)]]
BETA = "func beta() as int\n return 7\nend\n"


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    server=args.server.resolve(strict=True);output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    missing=output/'no-compiler'/'xl.exe';assert not missing.exists()
    entry=output/'入口😀.xl';entry.write_text('// protected disk bytes\n',encoding='utf-8');saved=entry.read_bytes()
    beta=output/'virtual dependency.xl';large=output/'large.xl';protected={str(server):sha(server),str(entry):sha(entry)}
    cases=[];clients=[];sequence=10;times=[];status='failed'

    def request(client,method,params=None):
        nonlocal sequence
        sequence+=1;client.send(method,params,sequence);message,_=client.reply(sequence);return message
    def result(client,method,params=None):
        message=request(client,method,params);assert 'error' not in message,message;return message['result']
    def folding(client,path=entry,version=None):
        params=dict(textDocument=dict(uri=path.as_uri()))
        if version is not None:params['version']=version
        started=time.perf_counter_ns();value=result(client,'textDocument/foldingRange',params)
        elapsed=(time.perf_counter_ns()-started)/1e6;return value,elapsed
    def stats(client):return result(client,'xlang/analysisStats')
    def done(name,**extra):cases.append(dict(name=name,passed=True,**extra))
    def create(limit=None):
        client=Client(server,missing,output);clients.append(client)
        caps={} if limit is None else dict(textDocument=dict(foldingRange=dict(rangeLimit=limit,lineFoldingOnly=True)))
        client.send('initialize',dict(capabilities=caps,initializationOptions=dict(checkOnSave=False)),1)
        message,_=client.reply(1);assert 'error' not in message,message
        cap=message['result']['capabilities'];assert cap['foldingRangeProvider'] is True and cap['experimental']['analysisRuntime']=='compiler-xrt' and cap['experimental']['residentSyntax'] is True
        client.send('initialized',{});return client
    def running(client,active):
        until=time.monotonic()+5
        while time.monotonic()<until:
            sequence_before=len(client.transcript);value=stats(client)
            assert not any(x.get('receive',{}).get('id')==active for x in client.transcript[sequence_before:]),'query completed before cancellation gate'
            if value['running']==1:return
        raise AssertionError('no live native worker observed')
    def send_fold(client,path,id):client.send('textDocument/foldingRange',dict(textDocument=dict(uri=path.as_uri())),id)

    try:
        client=create();client.open(entry,SOURCE,1);client.open(beta,BETA,1)
        assert stats(client)['parses']==0
        value,first=folding(client);assert value==RANGES,value
        observed=stats(client);assert observed['parses']==1 and observed['documents']==2 and observed['workerThreadId']!=observed['ownerThreadId'] and observed['workerThreadId']
        done('native-compiler-free-analysis-and-worker-isolation',firstMs=first)
        for _ in range(15):
            value,elapsed=folding(client);assert value==RANGES;times.append(elapsed)
        observed=stats(client);assert observed['parses']==1 and observed['cacheHits']>=15
        done('repeated-standard-queries-reuse-real-parse',parses=observed['parses'])
        client.change(beta,BETA.replace('7','8'),2);assert folding(client)[0]==RANGES and stats(client)['parses']==1
        assert folding(client,beta)[0]==[dict(startLine=0,endLine=2)] and stats(client)['parses']==2
        done('independent-document-edits-preserve-other-cache')
        client.change(entry,SOURCE,2);assert folding(client,version=2)[0]==RANGES and stats(client)['parses']==2
        done('identical-new-revision-reuses-syntax')
        header='//😀\r\n'
        client.send('textDocument/didChange',dict(textDocument=dict(uri=entry.as_uri(),version=3),contentChanges=[dict(range=dict(start=dict(line=0,character=0),end=dict(line=0,character=0)),text=header)]))
        shifted=[dict(startLine=x['startLine']+1,endLine=x['endLine']+1) for x in RANGES]
        assert folding(client,version=3)[0]==shifted and stats(client)['parses']==3
        done('ranged-edit-updates-native-parser-ranges')
        client.send('textDocument/didChange',dict(textDocument=dict(uri=entry.as_uri(),version=4),contentChanges=[dict(text=SOURCE),dict(range=dict(start=dict(line=999,character=0),end=dict(line=999,character=0)),text='bad')]))
        assert folding(client,version=3)[0]==shifted and stats(client)['parses']==3
        done('invalid-edit-batch-preserves-resident-input')
        message=request(client,'textDocument/foldingRange',dict(textDocument=dict(uri=entry.as_uri()),version=4));assert message['error']['code']==-32602
        done('stale-supplied-version-rejected')
        client.send('textDocument/didClose',dict(textDocument=dict(uri=entry.as_uri())))
        assert folding(client,beta)[0]==[dict(startLine=0,endLine=2)] and stats(client)['documents']==1
        client.open(entry,'\ufeff'+SOURCE.replace('\n','\r\n'),1);assert folding(client)[0]==RANGES and stats(client)['parses']==4
        done('close-pruning-reopen-identity-bom-crlf')
        client.send('textDocument/didClose',dict(textDocument=dict(uri=beta.as_uri())))
        client.open(beta,BETA,-2147483648);assert folding(client,beta,version=-2147483648)[0]==[dict(startLine=0,endLine=2)]
        client.change(beta,BETA.replace('7','9'),-2147483647);assert folding(client,beta,version=-2147483647)[0]==[dict(startLine=0,endLine=2)]
        done('signed-wire-version-maps-monotonically')
        client.change(beta,'func broken(\n',0);assert folding(client,beta)[0]==[]
        client.change(beta,BETA,1);assert folding(client,beta)[0]==[dict(startLine=0,endLine=2)]
        done('syntax-error-result-and-recovery')
        heavy=''.join(f'func filler{i}() as int\n return {i}\nend\n' for i in range(25000))
        client.open(large,heavy,1);send_fold(client,large,100000);running(client,100000)
        began=time.perf_counter_ns();client.send('$/cancelRequest',dict(id=100000));message,_=client.reply(100000)
        cancel_ms=(time.perf_counter_ns()-began)/1e6;assert message['error']['code']==-32800 and cancel_ms<1000
        done('live-large-parse-keeps-protocol-cancellation-responsive',replyMs=cancel_ms)
        client.change(large,'// changed\n'+heavy,2);send_fold(client,large,100001)
        client.change(large,'// latest\n'+heavy,3);message,_=client.reply(100001);assert message['error']['code']==-32801
        done('edit-invalidates-queued-or-running-request')
        send_fold(client,large,100002);client.send('textDocument/didClose',dict(textDocument=dict(uri=large.as_uri())))
        message,_=client.reply(100002);assert message['error']['code']==-32800
        assert folding(client,beta)[0]==[dict(startLine=0,endLine=2)] and stats(client)['documents']==2
        for id in [100000,100001,100002]:assert sum(x.get('receive',{}).get('id')==id for x in client.transcript)==1
        done('close-clears-pending-and-no-late-success-is-published')
        limited=create(2);limited.open(entry,SOURCE,1);assert folding(limited)[0]==RANGES[:2]
        zero=create(0);zero.open(entry,SOURCE,1);assert folding(zero)[0]==[]
        done('client-range-limit-hint-including-zero')
        # An LSP exit notification must end the process without requiring the
        # transport client to close stdin first. Exercise a fresh idle worker.
        for _ in range(20):
            idle=create();result(idle,'shutdown',{});idle.send('exit');idle.process.wait(timeout=2)
            assert idle.process.returncode==0;idle.process.stdin.close();idle.stop()
        done('exit-with-open-stdin-and-idle-worker')
        assert entry.read_bytes()==saved and not beta.exists() and not large.exists() and not missing.exists()
        done('disk-inputs-preserved-and-no-compiler-created')
        status='passed'
    finally:
        for index,client in enumerate(clients):
            stderr=client.stop();(output/f'client-{index}-stderr.log').write_text(stderr,encoding='utf-8')
            (output/f'client-{index}-transcript.json').write_text(json.dumps(client.transcript,ensure_ascii=False,indent=2),encoding='utf-8')
            if status=='passed':assert client.process.returncode==0,client.process.returncode
        assert all(sha(Path(path))==digest for path,digest in protected.items())
        timing=dict(scope='resident structural folding stdio roundtrip; not semantic completion or IDE rendering',samples=times)
        if times:
            ordered=sorted(times);timing.update(p50Ms=ordered[len(ordered)//2],p95Ms=ordered[math.ceil(len(ordered)*.95)-1])
        proof=dict(status=status,cases=cases,protected=protected,toolSha256=sha(Path(__file__)),clientHelperSha256=sha(Path(__file__).with_name('check_language_snapshots.py')),exitCodes=[c.process.returncode for c in clients],timing=timing)
        (output/'result.json').write_text(json.dumps(proof,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=status,cases=len(cases),proof=str(output/'result.json')),ensure_ascii=False))


if __name__=='__main__':main()
