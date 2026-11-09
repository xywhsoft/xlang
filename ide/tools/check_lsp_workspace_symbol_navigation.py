"""Actual fresh workspace-selection proof protocol,without a debug/runtime service."""
from __future__ import annotations
import argparse,copy,hashlib,json
from pathlib import Path
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_analysis_configuration import LISTS,PROFILE

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    server=a.server.resolve(strict=True);root=a.output.resolve();root.mkdir(parents=True,exist_ok=False)
    entry=root/'main.xl';helper=root/'中文😀'/'helper.xl';helper.parent.mkdir()
    body='import helper\nfunc main(...) as int\n return helper.value()\nend\n'
    # The module import resolves beside the real entry;the parent path still exercises Unicode URI escaping.
    entry=helper.parent/'main.xl';entry.write_text(body,encoding='utf-8')
    library='\ufeff/* 中文😀 */ class Payload\r\n int number\r\nend\r\nexport func value() as int\r\n return 1\r\nend\r\n';helper.write_bytes(library.encode())
    lists={name:[] for name in LISTS};lists['members']=[entry.as_posix(),helper.as_posix()]
    project=dict(schema=1,kind='project',project=(root/'A.xlp').as_posix(),configuration='Debug',entry=entry.as_posix(),directory=entry.parent.as_posix(),analysisProfile=PROFILE,targetKind='exe',packagePath=None,lists=lists)
    packet=lambda epoch,projects:dict(epoch=epoch,catalog=dict(schema=1,projects=copy.deepcopy(projects),selectedProject='',packagePath=None))
    sha=lambda value:hashlib.sha256(value).hexdigest();protected={str(q):sha(q.read_bytes()) for q in (server,Path(__file__))};clients=[];cases=[];report=dict(status='failed',scope='Actual native stdio fresh requery/selection/source proof. No IDE UI or global latency claim.',protected=protected,cases=cases)
    def done(name):cases.append(dict(name=name,passed=True))
    def client(cli=False,kinds=None):
        c=AutomaticClient(server,root/'absent/xl.exe',root);clients.append(c)
        opts=dict(automaticDiagnostics=False,checkOnSave=False)
        if cli:opts['diagnosticsBackend']='cli'
        else:opts['analysisConfiguration']=packet(1,[project])
        capabilities={} if kinds is None else dict(workspace=dict(symbol=dict(symbolKind=dict(valueSet=kinds))))
        c.send('initialize',dict(capabilities=capabilities,initializationOptions=opts),1);reply,_=c.reply(1);assert 'error' not in reply,reply
        assert reply['result']['capabilities'].get('experimental',{}).get('workspaceSymbolInputProof') is (None if cli else True),reply
        c.send('initialized',{});return c
    def symbols(c):return c.call('workspace/symbol',dict(query='Payload number'))
    def navigate(c,row):return c.call('xlang/workspaceSymbolNavigation',dict(query='Payload number',symbol=row))
    def error(c,params,code):
        c.request+=1;c.send('xlang/workspaceSymbolNavigation',params,c.request);reply,_=c.reply(c.request);assert reply.get('error',{}).get('code')==code,reply
    def proof(value,row,origin,text,version=None):
        assert set(value)=={'symbol','document'} and value['symbol']==row,value
        d=value['document'];normal=text.replace('\r\n','\n').replace('\r','\n')
        if origin=='disk':normal=normal.removeprefix('\ufeff')
        assert d['origin']==origin and Path(d['path']).resolve()==helper.resolve() and d['normalizedSize']==len(normal.encode()) and d['normalizedSha256']==sha(normal.encode()),d
        if origin=='disk':
            assert 'version' not in d and d['input']['rawSize']==len(text.encode()) and d['input']['rawSha256']==sha(text.encode()),d
        else:assert 'input' not in d and d['version']==version,d
        assert 'diagnostics' not in d,d
    try:
        c=client();rows=symbols(c);assert len(rows)==1;row=rows[0];proof(navigate(c,row),row,'disk',library)
        done('zero-open-fresh-actual-disk-identity-bom-crlf-unicode-proof')
        dirty=library.replace('return 1','return 2');helper.write_bytes(dirty.encode());proof(navigate(c,row),row,'disk',dirty)
        done('same-declaration-after-equal-length-body-edit-uses-new-proof')
        c.open(helper,library,-2147483648);proof(navigate(c,row),row,'overlay',library,-2147483648)
        for version in (-2147483647,2147483647):
            c.send('textDocument/didChange',dict(textDocument=dict(uri=helper.as_uri(),version=version),contentChanges=[dict(text=dirty)]));proof(navigate(c,row),row,'overlay',dirty,version)
        c.send('textDocument/didClose',dict(textDocument=dict(uri=helper.as_uri())));proof(navigate(c,row),row,'disk',dirty)
        done('overlay-bom-normalized-proof-full-int32-versions-and-close')
        altered=[]
        for field,value in [('name','amount'),('kind',12),('containerName','Other')]:
            v=copy.deepcopy(row);v[field]=value;altered.append(v)
        v=copy.deepcopy(row);v['location']['range']['start']['character']=0;altered.append(v)
        v=copy.deepcopy(row);v['location']['uri']=entry.as_uri();altered.append(v)
        for v in altered:error(c,dict(query='Payload number',symbol=v),-32803)
        error(c,dict(query='main',symbol=row),-32803)
        done('forged-name-kind-container-range-uri-and-different-query-rejected')
        generation=c.call('xlang/workspaceSymbolStats')['index.generation']
        invalid=[None,{},dict(row,kind=True),dict(row,extra=[]),dict(row,name='x'*262145),dict(row,containerName=7)]
        for field,value in [('line',-1),('character',True)]:
            v=copy.deepcopy(row);v['location']['range']['start'][field]=value;invalid.append(v)
        v=copy.deepcopy(row);v['location']['range']['end']=copy.deepcopy(v['location']['range']['start']);invalid.append(v)
        for v in invalid:error(c,dict(query='Payload number',symbol=v),-32602)
        assert c.call('xlang/workspaceSymbolStats')['index.generation']==generation
        done('invalid-selection-schema-types-and-label-bound-rejected-before-analysis')
        helper.write_bytes(dirty.replace('number','amount').encode());error(c,dict(query='Payload number',symbol=row),-32803);helper.write_bytes(dirty.encode());proof(navigate(c,row),row,'disk',dirty)
        helper.write_bytes(('\n'+dirty.removeprefix('\ufeff')).encode());error(c,dict(query='Payload number',symbol=row),-32803);helper.write_bytes(dirty.encode());proof(navigate(c,row),row,'disk',dirty)
        done('renamed-and-moved-declaration-do-not-navigate-stale-row-repair-recovers')
        helper.write_text('class Payload\n int number\nend\nexport func value() as int\n return "wrong"\nend\n',encoding='utf-8');error(c,dict(query='Payload number',symbol=row),-32803);helper.write_bytes(dirty.encode());proof(navigate(c,row),row,'disk',dirty)
        done('uncovered-real-source-error-rejected-and-repair-recovers')
        second=copy.deepcopy(project);second['project']=(root/'B.xlp').as_posix();second['configuration']='Release';c.call('xlang/configureAnalysis',packet(2,[project,second]));assert len(symbols(c))==2;proof(navigate(c,row),row,'disk',dirty)
        c.call('xlang/configureAnalysis',packet(3,[project]));done('same-source-independent-full-project-scopes-can-prove-selection')
        for method,params,code in [('workspace/didChangeWatchedFiles',dict(changes=[dict(uri=helper.as_uri(),type=2)]),-32801),('$/cancelRequest',None,-32800),('workspace/didChangeConfiguration',dict(settings=dict(xlang=dict(analysisConfiguration=packet(4,[project])))),-32801)]:
            c.request+=1;rid=c.request
            messages=[dict(jsonrpc='2.0',method='xlang/workspaceSymbolNavigation',params=dict(query='Payload number',symbol=row),id=rid),dict(jsonrpc='2.0',method=method,params=dict(id=rid) if params is None else params)]
            if params is None:messages.append(dict(jsonrpc='2.0',method='workspace/didChangeWatchedFiles',params=dict(changes=[dict(uri=helper.as_uri(),type=2)])))
            c.batch(messages)
            reply,_=c.reply(rid);assert reply.get('error',{}).get('code')==code,reply
            proof(navigate(c,row),row,'disk',dirty)
        done('watcher-config-and-cancellation-fences-with-recovery');c.stop()
        c=client(kinds=[26]);row=symbols(c)[0];assert row['kind']==26;proof(navigate(c,row),row,'disk',dirty);done('selection-matches-client-negotiated-kind-mapping');c.stop()
        c=client(cli=True);error(c,dict(query='Payload number',symbol=row),-32601);done('compatibility-backend-omits-proof-capability-and-rejects-request');c.stop()
        for path,h in protected.items():assert sha(Path(path).read_bytes())==h,path
        report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            stderr=c.stop();(root/f'client-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf-8');(root/f'client-{i}.stderr.txt').write_text(stderr,encoding='utf-8')
        (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=report['status'],cases=len(cases))),flush=True)
if __name__=='__main__':main()
