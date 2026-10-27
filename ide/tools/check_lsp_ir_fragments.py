"""Raw IR fragment provenance on the real retained static-check wire path.

Exercise exact/root/input-scope hits,real literal edits,nonlocal region
renumbering,compile errors/recovery andunchanged foreground methods. Counters
describe model creation,not work done by an exact hit or whole-model memory.
"""
from pathlib import Path
import argparse,hashlib,json,time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position

def source(edit=0):
    body='\ufeff// 中文😀\r\nfunc identity(int value) as int\r\n return value\r\nend\r\n'
    for i in range(8):
        value='99999' if i==3 and edit==1 else str(i)
        body+=f'func helper_{i}(int value) as int\r\n int local = value + {value}\r\n if local > 0 then\r\n'
        if i==3 and edit==2:body+=' if local > 2\r\n local = local + 5\r\n end\r\n'
        if i==3 and edit==3:body+=' int inserted = 3\r\n local = local + inserted\r\n'
        body+=' local = identity(local)+1\r\n'*12+' end\r\n return local\r\nend\r\n'
    body+='func choose(int left,string right) as int\r\n return left\r\nend\r\nfunc main(...) as int\r\n int doneLocal=choose(7,"hello")\r\n return doneLocal\r\nend\r\n'
    return body

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();paths=[out/'根A😀.xl',out/'根B.xl'];cases=[]
    for p in paths:p.write_bytes(b'// unsaved overlays only\n')
    protected={str(p):sha(p) for p in [server,Path(__file__),*paths]};c=AutomaticClient(server,out/'absent/xl.exe',out);report=dict(status='failed',scope=__doc__,cases=cases,protected=protected)
    def call(index,body,version,ok=True,method='xlang/check'):
        params=dict(textDocument=dict(uri=paths[index].as_uri()),version=version)
        if method.endswith('completion'):params['position']=position(body,body.index('return done')+len('return done'))
        if method.endswith('signatureHelp'):params.update(position=position(body,body.index('"hello"')+2),context=dict(triggerKind=1,isRetrigger=False))
        until=time.monotonic()+30
        while True:
            c.request+=1;c.send(method,params,c.request);reply,_=c.reply(c.request)
            if reply.get('error',{}).get('code')!=-32001:break
            assert time.monotonic()<until;time.sleep(.002)
        assert 'error' not in reply,reply
        value=reply['result'];assert value['version']==version;analysis=value['analysis']
        if method=='xlang/check':
            assert value['ok']==ok and (value['count']==0)==ok,value
            ir=analysis['irFragments'];assert set(ir)=={'checked','reused','captured','nodesReused','bytes','declined','textNodes','textBytes','ownerBytes'}
            assert all(isinstance(v,int) and v>=0 for v in ir.values()) and ir['bytes']<=32*1024*1024 and ir['ownerBytes']<=128*1024,ir
            if not ok:assert not any(ir.values()),ir
        else:
            assert 'irFragments' not in analysis,analysis
            if method.endswith('completion'):assert any(x['label']=='doneLocal' for x in value['items'])
            else:assert value['activeParameter']==1 and value['signatures'][value['activeSignature']]['label']=='choose(int left, string right) as int'
        cases.append(dict(method=method,target=index,version=version,analysis=analysis));return analysis
    try:
        c.send('initialize',dict(capabilities={},initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False)),1);init,_=c.reply(1);assert init['result']['serverInfo']['version']=='0.90';c.send('initialized',{})
        original=source()
        for p in paths:c.open(p,original,1)
        a=call(0,original,1);cold=a['irFragments'];assert cold['captured']>0 and not cold['reused'] and not cold['textNodes']
        exact=call(0,original,1);assert exact['irFragments']==cold and exact['builds']==1 and exact['cacheHits']>0
        b=call(1,original,1);assert b['builds']==2 and not b['irFragments']['reused']
        back=call(0,original,1);assert back['builds']==2 and back['irFragments']==cold
        literal=source(1);c.change(paths[0],literal,2)
        unrelated=call(1,original,1);assert unrelated['builds']==2 and unrelated['inputScopeHits']>0 and unrelated['irFragments']==b['irFragments']
        edited=call(0,literal,2);ir=edited['irFragments'];assert edited['builds']==3 and ir['reused']>0 and ir['nodesReused']>100 and ir['textNodes']>0 and ir['textBytes']>0
        assert call(0,literal,2)['irFragments']==ir
        regions=source(2);c.change(paths[0],regions,3);region=call(0,regions,3)
        assert region['irFragments']['reused']==ir['reused'] and region['irFragments']['nodesReused']==ir['nodesReused'],region
        assert region['semanticFunctionsReused']-edited['semanticFunctionsReused']==8,region
        assert call(1,original,1)['irFragments']==b['irFragments']
        c.change(paths[0],original,4);removed=call(0,original,4);assert removed['irFragments']['reused']==ir['reused']
        c.change(paths[0],regions,5);again=call(0,regions,5);assert again['irFragments']['reused']==ir['reused']
        broken=regions.replace('int local = value + 3','int local = "wrong"',1);assert broken!=regions;c.change(paths[0],broken,6);call(0,broken,6,False)
        c.change(paths[0],literal,7);recovered=call(0,literal,7);assert recovered['irFragments']['captured']>0
        c.change(paths[0],original,8);restored=call(0,original,8);assert restored['irFragments']['reused']>0
        local=source(3);c.change(paths[0],local,9);inserted=call(0,local,9)
        assert inserted['irFragments']['reused']>=6 and inserted['irFragments']['textNodes']>0,inserted
        assert inserted['semanticFunctionsReused']-restored['semanticFunctionsReused']>=6,inserted
        c.change(paths[0],original,10);removed=call(0,original,10)
        assert removed['irFragments']['reused']>=6 and removed['irFragments']['textNodes']>0,removed
        assert removed['semanticFunctionsReused']-inserted['semanticFunctionsReused']>=6,removed
        for method in ['textDocument/completion','textDocument/signatureHelp']:
            call(0,original,10,method=method);call(0,original,10,method=method)
        c.send('textDocument/didClose',dict(textDocument=dict(uri=paths[0].as_uri())))
        current=call(1,original,1);assert current['irFragments']['bytes']>0
        assert all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        stderr=c.stop();(out/'stderr.log').write_text(stderr,encoding='utf8');(out/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8');report['exit']=c.process.returncode
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    assert not c.process.returncode;print(json.dumps(dict(status='passed',cases=len(cases),proof=str(out/'result.json'))))
if __name__=='__main__':main()
