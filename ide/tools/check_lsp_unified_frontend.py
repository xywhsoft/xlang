"""Compare incremental and fresh native stdio results on compiler regressions.

Candidate/fresh process isolation protects the existing release during frontend
alignment. No compiler executable,program execution,performance or GUI admission.
"""
from pathlib import Path
from contextlib import contextmanager
import argparse,hashlib,json,re,sys
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize,checked
from check_language_definitions import position
from native_current_unified_profile import CORPUS
from native_c_interop_profile import CORPUS as C_INTEROP_CORPUS, NEGATIVE_CORPUS


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def facts(value):return {k:v for k,v in value.items() if k not in ('analysis','durationMs','checkId','queryId')}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server',type=Path,required=True);p.add_argument('--corpus',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--c-interop',action='store_true',help='also verify the captured C-interop positive and negative corpus')
    a=p.parse_args();server=a.server.resolve(strict=True);corpus=a.corpus.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    cases=CORPUS+(C_INTEROP_CORPUS+tuple(('bad_c_interop_'+name,0) for name in NEGATIVE_CORPUS) if a.c_interop else ())
    protected={str(x):sha(x) for x in [server,Path(__file__).resolve(),*[corpus/(name+'.xl') for name,_ in cases],*[Path(__file__).with_name(x) for x in ['check_lsp_automatic_diagnostics.py','check_lsp_module_semantics.py','check_language_definitions.py','check_language_snapshots.py','native_current_unified_profile.py','native_c_interop_profile.py']]]}
    report=dict(status='running',scope=__doc__,protected=protected,processes=[],checks=[],navigation=[],completions=[])
    def save():(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8',newline='\n')
    @contextmanager
    def client(tag,folder,entry):
        c=AutomaticClient(server,folder/'absent/xl.exe',folder);row=dict(name=tag,pid=c.process.pid);report['processes'].append(row);save()
        try:initialize(c,entry,[entry],folder);yield c
        finally:
            primary=sys.exc_info()[1];errors=[];stderr=''
            try:stderr=c.stop()
            except BaseException as error:
                errors.append(repr(error))
                if c.process.poll() is None:
                    c.process.kill();c.process.wait(timeout=5)
            row.update(exit=c.process.returncode,stderr=stderr,cleanupErrors=errors)
            transcript=out/(tag+'-transcript.json');transcript.write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf8');row['transcript']=str(transcript);row['transcriptSha256']=sha(transcript);save()
            if primary is None:assert not errors and not stderr and row['exit']==0,row
    def query(c,method,entry,body,offset,version,batch=None):
        params=dict(textDocument=dict(uri=entry.as_uri()),version=version,position=position(body,offset))
        if batch is not None:params['checkId']=batch
        return c.call(method,params)
    try:
        for name,accepted in cases:
            folder=out/name;folder.mkdir();entry=folder/'源码😀.xl';entry.write_text('// immutable overlays\n',encoding='utf8');protected[str(entry)]=sha(entry)
            original=(corpus/(name+'.xl')).read_bytes().decode('utf-8-sig');previous=''
            with client(name+'-incremental',folder,entry) as live:
                for generation in range(4):
                    version=generation+1;body=original if not generation else '\ufeff// 中文 😀\r\n'+original
                    if generation>=2:body+='\nfunc xls_unified_anchor() as int\n return 3\nend\nfunc xls_unified_probe() as int\n return '+str(17 if generation==2 else 18)+'\nend\n'
                    if not generation:live.open(entry,body,version)
                    else:live.change(entry,body,version)
                    actual=checked(live,entry,version);assert actual['ok']==bool(accepted),actual
                    assert (actual['count']==0 if accepted else actual['count']>0),actual
                    with client(name+'-fresh-'+str(generation),folder,entry) as fresh:
                        fresh.open(entry,body,version);reference=checked(fresh,entry,version);assert facts(actual)==facts(reference),(name,generation,actual,reference)
                        report['checks'].append(dict(case=name,generation=generation,actual=actual,reference=reference));save()
                        if accepted:
                            declarations=list(re.finditer(r'(?m)^[ \t]*(?:auto|(?:c?string|c?bytes)\??)\s+([A-Za-z_]\w*)\s*=',body))[:3]
                            for match in declarations:
                                at=match.start(1)
                                for method in ('textDocument/hover','textDocument/definition'):
                                    x=query(live,method,entry,body,at,version,actual['checkId']);y=query(fresh,method,entry,body,at,version,reference['checkId']);assert x==y,(name,generation,method,match[1],x,y)
                                    if method=='textDocument/hover':assert x,(name,match[1],'missing hover')
                                    report['navigation'].append(dict(case=name,generation=generation,name=match[1],method=method,value=x))
                            record=re.search(r'(?m)^extern "[^"\r\n]+" struct (\w+)',body)
                            if a.c_interop and record:
                                for method in ('textDocument/documentSymbol','textDocument/foldingRange'):
                                    params=dict(textDocument=dict(uri=entry.as_uri()),version=version)
                                    x=live.call(method,params);y=fresh.call(method,params);assert x==y,(name,generation,method,x,y)
                                    if method.endswith('documentSymbol'):assert any(s['name']==record[1] and s['kind']==23 for s in x),(name,record[1],x)
                                    else:assert any(s['startLine']==position(body,record.start())['line'] for s in x),(name,record[1],x)
                                    report['navigation'].append(dict(case=name,generation=generation,name=record[1],method=method,value=x))
                                # Verify the C type use,including standard identity queries.
                                occurrence=re.search(r'\b'+re.escape(record[1])+r'\b',body[record.end():])
                                assert occurrence,(name,record[1])
                                use=record.end()+occurrence.start()
                                for method in ('textDocument/definition','textDocument/hover','textDocument/documentHighlight','textDocument/references'):
                                    params=dict(textDocument=dict(uri=entry.as_uri()),position=position(body,use),version=version)
                                    if method.endswith('references'):params['context']=dict(includeDeclaration=True)
                                    x=live.call(method,params);y=fresh.call(method,params);assert x and x==y,(name,generation,method,x,y)
                                    report['navigation'].append(dict(case=name,generation=generation,name=record[1],method=method,value=x))
                    hit=checked(live,entry,version);assert facts(hit)==facts(actual) and hit['analysis']['builds']==actual['analysis']['builds'] and hit['analysis']['cacheHits']==actual['analysis']['cacheHits']+1,(name,generation,actual,hit)
                    report['checks'].append(dict(case=name,generation=generation,exactHit=hit));save();previous=body
                if accepted:
                    suffix='\nclass XlsQueryBox\n func Seen() as int\n  return 1\n end\nend\nfunc xls_query() as int\n XlsQueryBox box=XlsQueryBox()\n return box.Se\nend\n'
                    for step,prefix in enumerate(('Se','See')):
                        version=5+step;body=previous+suffix.replace('box.Se\n','box.'+prefix+'\n');live.change(entry,body,version);at=body.rindex('box.'+prefix)+len('box.'+prefix)
                        actual=query(live,'textDocument/completion',entry,body,at,version);assert actual['queryId']==live.request and actual['available'] and actual['version']==version and [x['label'] for x in actual['items']]==['Seen'],actual
                        with client(name+'-fresh-completion-'+str(step),folder,entry) as fresh:
                            fresh.open(entry,body,version);reference=query(fresh,'textDocument/completion',entry,body,at,version);assert reference['queryId']==fresh.request and facts(actual)==facts(reference),(name,prefix,actual,reference)
                        report['completions'].append(dict(case=name,prefix=prefix,actual=actual,reference=reference));save()
            print(name,'passed',flush=True)
        for path,digest in protected.items():assert sha(Path(path))==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],processes=len(report['processes']),checks=len(report['checks']),navigation=len(report['navigation']),completions=len(report['completions']))))


if __name__=='__main__':main()
