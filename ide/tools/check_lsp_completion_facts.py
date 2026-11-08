"""Check expected public completion facts through the real stdio service.

Expected labels, callable types, UTF-16 edits and declaration locations are
independent user-visible contracts. Incremental/fresh parity alone would allow
both paths to expose the same private compiler workers.
"""
from pathlib import Path
from contextlib import contextmanager
import argparse,json,re,sys
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize
from check_language_definitions import position
from build_lsp_lifecycle_profile import sha

COMMON='extern "native_text" @no_raise func nativeText() as point\nextern "native_free" @no_raise func nativeFree(point value)\nfunc text() as cstring\n return cstring.take(nativeText(),nativeFree)\nend\n'
MEMBER=COMMON+'''class Reader
 func Read<T>(T value) as cstring
  return text()
 end
 async func Factory<T>(T value) as func(T) as cstring
  return self.Read
 end
 func __xl_c_owned_async_user() as int
  return 7
 end
end
func bind(Reader reader) as func(c_int) as future<func(c_int) as string>
 return reader.Factory
end
func query(Reader reader) as int
 return reader.QUERY
end
'''
GLOBAL=COMMON+'''async func factory<T>(T witness) as func() as cstring
 return text
end
func bind<U>(U witness) as func(U) as future<func() as string>
 return factory::<U>
end
func __xl_c_owned_async_user() as int
 return 7
end
func query() as int
 return QUERY
end
'''
CASES=(
 ('member',MEMBER,('', '', 'F', 'Fa'),{'Read':'func(T) as cstring','Factory':'func(T) as future<func(T) as cstring>','__xl_c_owned_async_user':'func() as int'},2),
 ('global',GLOBAL,('__xl_c_owned_async_','__xl_c_owned_async_','__xl_c_owned_async_u','__xl_c_owned_async_'),{'__xl_c_owned_async_user':'func() as int'},3),
 ('signature',GLOBAL,('fact','fact','facto','factor'),{'factory':'func(T) as future<func() as cstring>'},3),
 ('void','async func empty()\nend\nfunc query()\n return QUERY\nend\n',('emp',)*4,{'empty':'func() as future<null>'},3),
 ('static','class Base\n static async func Static<T>(T value) as T\n return value\n end\nend\nclass Child extends Base\nend\nfunc query()\n return Child.QUERY\nend\n',('Sta',)*4,{'Static':'func(T) as future<T>'},2),
 ('inherited','class Base\n async func Read() as int\n return 7\n end\nend\nclass Child extends Base\nend\nfunc query(Child child)\n return child.QUERY\nend\n',('Rea',)*4,{'Read':'func() as future<int>'},2),
 ('generic','class Box<T>\n async func Echo(T value) as T\n return value\n end\nend\nfunc query(Box<string> box)\n return box.QUERY\nend\n',('Ec',)*4,{'Echo':'func(string) as future<string>'},2),
 ('generator','func values() as generator<int>\n yield 7\nend\nfunc query()\n return QUERY\nend\n',('val',)*4,{'values':'func() as generator<int>'},3),
 ('async-generator','async func stream() as generator<int>\n yield 7\nend\nfunc query()\n return QUERY\nend\n',('stre',)*4,{'stream':'func() as generator<int>'},3),
)


def facts(value):return {k:v for k,v in value.items() if k not in ('analysis','durationMs','checkId','queryId')}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();server=a.server.resolve(strict=True);O=a.output.resolve();O.mkdir(parents=True,exist_ok=False)
    protected={str(path):sha(path) for path in [server,Path(__file__).resolve(),*[Path(__file__).with_name(n) for n in ['check_lsp_automatic_diagnostics.py','check_lsp_module_semantics.py','check_language_definitions.py','check_language_snapshots.py','build_lsp_lifecycle_profile.py']]]}
    report=dict(status='running',scope=__doc__,protected=protected,processes=[],queries=[])
    def save():(O/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    @contextmanager
    def client(name,folder,entry):
        c=AutomaticClient(server,folder/'absent/xl.exe',folder);row=dict(name=name,pid=c.process.pid);report['processes'].append(row);save()
        try:initialize(c,entry,[entry],folder);yield c
        finally:
            primary=sys.exc_info()[1];errors=[];stderr=''
            try:stderr=c.stop()
            except BaseException as error:
                errors.append(repr(error))
                if c.process.poll() is None:c.process.kill();c.process.wait(timeout=5)
            row.update(exit=c.process.returncode,stderr=stderr,cleanupErrors=errors)
            transcript=O/(name+'-transcript.json');transcript.write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf8');row.update(transcript=str(transcript),transcriptSha256=sha(transcript));save()
            if primary is None:assert not errors and not stderr and row['exit']==0,row
    def query(c,entry,body,at,version):
        return c.call('textDocument/completion',dict(textDocument=dict(uri=entry.as_uri()),version=version,position=position(body,at)))
    def expected(result,entry,body,at,prefix,items,kind):
        assert result['available'] and result['ok'] and result['prefix']==prefix,result
        filtered={name:detail for name,detail in items.items() if name.startswith(prefix)}
        assert set(x['label'] for x in result['items'])==set(filtered) and len(result['items'])==len(filtered),('expected labels',filtered,result)
        replacement=dict(start=position(body,at-len(prefix)),end=position(body,at));assert result['replacementRange']==replacement,result
        for item in result['items']:
            name=item['label'];assert item['detail']==filtered[name] and item['kind']==kind,(name,item,filtered[name])
            assert item['textEdit']==dict(range=replacement,newText=name) and item['insertTextFormat']==1,item
            match=re.search(r'\bfunc\s+('+re.escape(name)+r')\b',body);assert match,name
            declaration=dict(uri=entry.as_uri(),range=dict(start=position(body,match.start(1)),end=position(body,match.end(1))))
            assert item['data']['declaration']==declaration and item['data']['symbolId']>0,(name,item,declaration)
    try:
        for name,template,prefixes,items,kind in CASES:
            folder=O/name;folder.mkdir();entry=folder/'源码😀.xl';entry.write_text('// immutable disk placeholder\n',encoding='utf8');protected[str(entry)]=sha(entry)
            with client(name+'-incremental',folder,entry) as live:
                for generation,prefix in enumerate(prefixes):
                    offset=template.index('QUERY');body=template.replace('QUERY',prefix);at=offset+len(prefix)
                    if generation:
                        preamble='\ufeff// moved 中文😀\r\n';body=preamble+body;at+=len(preamble)
                    if generation>=2:body+='\nfunc xls_added() as int\n return '+str(17 if generation==2 else 18)+'\nend\n'
                    version=generation+1
                    if generation:live.change(entry,body,version)
                    else:live.open(entry,body,version)
                    actual=query(live,entry,body,at,version);expected(actual,entry,body,at,prefix,items,kind);assert actual['version']==version and actual['queryId']==live.request
                    with client(name+'-fresh-'+str(generation),folder,entry) as fresh:
                        fresh.open(entry,body,version);reference=query(fresh,entry,body,at,version);expected(reference,entry,body,at,prefix,items,kind);assert facts(actual)==facts(reference),(name,generation,actual,reference)
                    hit=query(live,entry,body,at,version);expected(hit,entry,body,at,prefix,items,kind)
                    assert facts(hit)==facts(actual) and hit['analysis']['builds']==actual['analysis']['builds'] and hit['analysis']['cacheHits']==actual['analysis']['cacheHits']+1,(name,generation,actual,hit)
                    report['queries'].append(dict(case=name,generation=generation,actual=actual,reference=reference,exactHit=hit));save()
            print(name,'passed',flush=True)
        for path,digest in protected.items():assert sha(path)==digest,path
        report['status']='passed'
    except BaseException as error:report.update(status='failed',error=repr(error));raise
    finally:save()
    print(json.dumps(dict(status=report['status'],processes=len(report['processes']),queries=len(report['queries']),report=str(O/'result.json'))))


if __name__=='__main__':main()
