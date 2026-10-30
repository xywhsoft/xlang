"""Default configured LSP actor must consume proved raw bodies before copies.

Completion plus closed/incomplete signature projections; includes mixed
configured/parsed path spellings,ordinary imports and closed
entry/imported queries. Compare the full wire result against independent fresh
processes;only analysis and nonnegative duration instrumentation are excluded.
"""
from pathlib import Path
import argparse,json,hashlib
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize,source
from check_language_definitions import position

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--baseline',type=Path);p.add_argument('--baseline-version',default='0.98');p.add_argument('--expected-version',default='0.100');p.add_argument('--query-kind',choices=['completion','signature','signature-open'],default='completion');args=p.parse_args()
    signature=args.query_kind!='completion'
    method='textDocument/signatureHelp' if signature else 'textDocument/completion'
    server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False);cases=[];clients=[];processes=[];protected={}
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected[str(server)]=sha(server);protected[str(Path(__file__))]=sha(__file__)
    baseline=args.baseline.resolve(strict=True) if args.baseline else None
    if baseline:protected[str(baseline)]=sha(baseline)
    report=dict(status='failed',scope=__doc__,cases=cases,processes=processes,protected=protected)
    def start(folder,entry,paths,opened,exe=None):
        c=AutomaticClient(exe or server,folder/'absent/xl.exe',folder);clients.append(c);initialize(c,entry,paths,folder)
        reply=next(row['receive'] for row in c.transcript if row.get('receive',{}).get('id')==1)
        assert reply['result']['serverInfo']['version']==(args.baseline_version if exe else args.expected_version),reply
        for path,body,version in opened:c.open(path,body,version)
        return c
    def finish(c):
        stderr=c.stop();assert c.process.returncode==0,c.process.returncode
        label=f'process-{len(processes)+1:02}'
        transcript=root/(label+'-transcript.json');transcript.write_text(json.dumps(c.transcript,indent=2,ensure_ascii=False),'utf8')
        log=root/(label+'-stderr.log');log.write_text(stderr,'utf8')
        processes.append(dict(server=c.process.args[0],exit=c.process.returncode,transcript=str(transcript),transcriptSha256=sha(transcript),stderr=str(log),stderrSha256=sha(log)))
    def query(c,path,body,version):
        c.request+=1;marker=queryLine.strip() if signature else 'return completionL';at=body.index(marker)+(marker.index('(')+1 if signature else len(marker));c.send(method,dict(textDocument=dict(uri=path.as_uri()),position=position(body,at),version=version),c.request)
        reply,_=c.reply(c.request);assert 'error' not in reply,reply;v=reply['result'];assert v['version']==version and v['analysisContext']['epoch']==1,v
        if signature:assert v['signatures'] and 'compute(' in v['signatures'][v['activeSignature']]['label'] and v['activeParameter']==0,v
        else:assert any(item['label']=='completionLocal' for item in v['items']),v
        if not signature or 'durationMs' in v:assert isinstance(v['durationMs'],int) and v['durationMs']>=0
        return v
    def facts(v):return {k:w for k,w in v.items() if k not in ('analysis','durationMs')}
    try:
        for scenario in ['single','imported-module','closed-entry-imported-query']:
            folder=root/(scenario+'-配置 中文😀');folder.mkdir();entry,dep=folder/'main.xl',folder/'dep.xl';paths=[entry] if scenario=='single' else [entry,dep]
            queryLine=(' return '+('dep.' if scenario=='imported-module' else '')+'compute(completionLocal'+(')' if args.query_kind=='signature' else '')+'\r\n') if signature else ' return completionL\r\n'
            tail='func queryLocal() as int\r\n int completionLocal=7\r\n'+queryLine+'end\r\n'
            if scenario=='single':mainBody=source(32,0,True)+tail;depBody=None;target=entry
            elif scenario=='imported-module':mainBody='import dep\r\nfunc main(...) as int\r\n int completionLocal=dep.compute(7)\r\n'+queryLine+'end\r\n';depBody=source(32,0,True);target=entry
            else:mainBody='import dep\r\nfunc main(...) as int\r\n return dep.compute(7)\r\nend\r\n';depBody=source(32,0,True)+tail;target=dep
            entry.write_bytes(mainBody.encode('utf8') if target==dep else b'// immutable disk placeholder\n');dep.write_bytes(b'// immutable disk placeholder\n');protected[str(entry)]=sha(entry);protected[str(dep)]=sha(dep)
            client=None
            for generation in range(3):
                main=mainBody;dependency=depBody
                if generation:
                    if scenario=='imported-module':dependency=depBody.replace(' if local > 0 then\r\n',f' int inserted=local+{generation}\r\n if local > 0 then\r\n',1)
                    elif scenario=='single':main=mainBody.replace(queryLine,f' int inserted=completionLocal+{generation}\r\n'+queryLine)
                    else:dependency=depBody.replace(queryLine,f' int inserted=completionLocal+{generation}\r\n'+queryLine)
                opened=[]
                if target==entry:opened.append((entry,main,generation+1 if scenario=='single' else 1))
                if dependency is not None:opened.append((dep,dependency,generation+1))
                if client is None:client=start(folder,entry,paths,opened)
                else:
                    changed=dep if scenario!='single' else entry;client.change(changed,dependency if changed==dep else main,generation+1)
                queryBody=dependency if target==dep else main;version=generation+1 if target==dep or scenario=='single' else 1
                value=query(client,target,queryBody,version);raw=value['analysis']['lastBuildRawBodies'];sem=(dict(proofHits=value['analysis']['signatureProofHits'],proofNodes=value['analysis']['signatureProofNodes']) if signature else value['analysis']['lastBuildSemantics']);minimum=31 if scenario=='imported-module' else 32
                if generation:assert raw['deferred']>=minimum and raw['restored']==raw['deferred'] and not raw['retries'] and not raw['fullMaterialized'] and not raw['astFailures'] and sem['proofHits']>=minimum and sem['proofNodes']>sem['proofHits'],(scenario,generation,raw,sem)
                else:assert not raw['deferred'] and raw['memoBytes']>0,(scenario,raw)
                oracle=start(folder,entry,paths,opened);oracle.request=client.request-1;fresh=query(oracle,target,queryBody,version);assert facts(value)==facts(fresh),(scenario,generation,value,fresh);finish(oracle);clients.remove(oracle)
                if baseline:
                    old=start(folder,entry,paths,opened,baseline);old.request=client.request-1;previous=query(old,target,queryBody,version);assert facts(value)==facts(previous),(scenario,generation,value,previous);finish(old);clients.remove(old)
                cases.append(dict(scenario=scenario,queryKind=args.query_kind,generation=generation,passed=True,raw=raw,semantics=sem))
            finish(client);clients.remove(client)
        assert len(cases)==9 and len(processes)==(21 if baseline else 12) and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for c in clients:
            try:finish(c)
            except Exception:pass
        (root/'result.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),'utf8')
    print(json.dumps(dict(status='passed',cases=len(cases))),flush=True)
if __name__=='__main__':main()
