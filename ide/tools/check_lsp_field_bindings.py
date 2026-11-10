"""Native field definition/reference/highlight/hover over real stdio.

The compiler is absent. Check qualified Unicode fields with the same spelling
in different classes, unsaved imports, changed dependencies and recovery.
"""
from pathlib import Path
import argparse,hashlib,json
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve()
    out.mkdir(parents=True,exist_ok=False);root=out/'字段😀';root.mkdir()
    entry,dep=root/'main.xl',root/'dep.xl'
    entry.write_bytes(b'// disk differs from live overlay\n')
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p)for p in (server,entry,Path(__file__))}
    c=AutomaticClient(server,out/'absent/xl.exe',root);cases=[]
    report=dict(status='failed',cases=cases,protected=protected,scope=__doc__)
    def done(name):cases.append(dict(name=name,passed=True))
    def span(text,at,name='😀值'):
        return dict(start=position(text,at),end=position(text,at+len(name)))
    def params(text,needle,inside=0):
        return dict(textDocument=dict(uri=entry.as_uri()),position=position(text,text.index(needle)+inside))
    def check():
        return c.call('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=1))
    def definition(text,needle,expected):
        actual=c.call('textDocument/definition',params(text,needle));assert actual==expected,actual
    try:
        caps=dict(textDocument=dict(documentSymbol=dict(hierarchicalDocumentSymbolSupport=True)))
        c.send('initialize',dict(capabilities=caps,initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False,backgroundWorkspaceSymbols=False)),1)
        reply,_=c.reply(1);assert 'error'not in reply,reply
        assert 'renameProvider'not in reply['result']['capabilities']
        c.send('initialized',{})
        lib='\ufeffexport class Box\r\n int 😀值\r\nend\r\nexport class Other\r\n int 😀值\r\nend\r\n'
        source='\ufeffimport dep\r\nfunc main(...) as int\r\n auto box=dep.Box()\r\n auto other=dep.Other()\r\n box.😀值=3\r\n string text="😀"; return box.😀值+other.😀值\r\nend\r\n'
        c.open(entry,source,1);c.open(dep,lib,1);result=check();assert result['ok'],result
        first=lib.index('😀值');second=lib.index('😀值',first+1)
        box=dict(uri=dep.as_uri(),range=span(lib,first));other=dict(uri=dep.as_uri(),range=span(lib,second))
        definition(source,'😀值=3',box);definition(source,'😀值+other',box);definition(source,'😀值\r\nend',other)
        done('distinct-native-class-bindings-with-identical-unicode-spellings')
        request=params(source,'😀值+other');request['context']=dict(includeDeclaration=True)
        refs=c.call('textDocument/references',request)
        wanted=[box,dict(uri=entry.as_uri(),range=span(source,source.index('😀值=3'))),dict(uri=entry.as_uri(),range=span(source,source.index('😀值+other')))]
        key=lambda x:(x['uri'],x['range']['start']['line'],x['range']['start']['character'])
        assert sorted(refs,key=key)==sorted(wanted,key=key),(refs,wanted)
        request['context']['includeDeclaration']=False
        assert sorted(c.call('textDocument/references',request),key=key)==sorted(wanted[1:],key=key)
        done('complete-native-field-reference-set-with-declaration-filter')
        highlights=c.call('textDocument/documentHighlight',params(source,'😀值+other'))
        assert sorted((x['range']for x in highlights),key=lambda x:x['start']['line'])==[wanted[1]['range'],wanted[2]['range']],highlights
        hover=c.call('textDocument/hover',params(source,'😀值+other'));assert hover and hover['contents']['value'],hover
        done('field-highlight-and-hover-use-exact-source-binding')
        symbols=c.call('textDocument/documentSymbol',dict(textDocument=dict(uri=dep.as_uri())))
        fields=[child for parent in symbols for child in parent.get('children',[])if child['name']=='😀值']
        assert len(fields)==2 and all(x['kind']==8 for x in fields),symbols
        done('both-source-field-outlines-use-property-kind')
        broken=lib.replace('int 😀值','string 😀值',1);c.change(dep,broken,2)
        rejected=check();assert not rejected['ok'] and rejected['count'],rejected
        assert c.call('textDocument/definition',params(source,'😀值+other'))is None
        done('changed-field-type-never-publishes-old-binding')
        c.change(dep,lib,3);recovered=check();assert recovered['ok'],recovered
        definition(source,'😀值+other',box)
        request['context']['includeDeclaration']=True
        assert sorted(c.call('textDocument/references',request),key=key)==sorted(wanted,key=key)
        done('restored-unsaved-dependency-rebinds-field-and-references')
        assert not dep.exists() and not (out/'absent/xl.exe').exists()
        report['status']='passed'
    finally:
        c.stop();(out/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
        for p,h in protected.items():assert sha(p)==h,p
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('PASS field binding stdio',len(cases),'cases',flush=True)

if __name__=='__main__':main()
