"""Exact native generic origins over stdio, including unsaved dependencies.

Check class construction, specialized function values/calls, final overload
selection, type-argument isolation, source coordinates and stale-result gates.
"""
from pathlib import Path
import argparse,hashlib,json
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    root=out/'泛型😀';root.mkdir();entry,dep=root/'main.xl',root/'dep.xl';entry.write_bytes(b'// live overlays deliberately differ\n')
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p)for p in (server,entry,Path(__file__))};c=AutomaticClient(server,out/'absent/xl.exe',root);cases=[]
    report=dict(status='failed',cases=cases,protected=protected,scope=__doc__)
    def done(name):cases.append(dict(name=name,passed=True))
    def span(text,offset,name):return dict(start=position(text,offset),end=position(text,offset+len(name)))
    def location(path,text,name,offset=None):return dict(uri=path.as_uri(),range=span(text,text.index(name)if offset is None else offset,name))
    def params(text,needle):return dict(textDocument=dict(uri=entry.as_uri()),position=position(text,text.index(needle)+(4 if needle.startswith('dep.')else 0)))
    def check():return c.call('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=1))
    def definition(text,needle,expected):
        actual=c.call('textDocument/definition',params(text,needle));assert actual==expected,(needle,actual,expected)
    def references(text,needle,expected):
        p=params(text,needle);p['context']=dict(includeDeclaration=True);actual=c.call('textDocument/references',p)
        key=lambda x:(x['uri'],x['range']['start']['line'],x['range']['start']['character'])
        assert sorted(actual,key=key)==sorted(expected,key=key),(needle,actual,expected)
        p['context']['includeDeclaration']=False
        actual=c.call('textDocument/references',p);wanted=expected[1:]
        assert sorted(actual,key=key)==sorted(wanted,key=key),(actual,wanted)
    try:
        c.send('initialize',dict(capabilities={},initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False,backgroundWorkspaceSymbols=False)),1)
        reply,_=c.reply(1);assert 'error'not in reply,reply;assert 'renameProvider'not in reply['result']['capabilities'];c.send('initialized',{})
        lib='\ufeffexport class 😀盒<T>\r\n T value\r\n create(T value)\r\n self.value=value\r\n end\r\nend\r\nexport class Marker\r\nend\r\n'
        source='\ufeffimport dep\r\nfunc identity<T>(T value) as T\r\n return value\r\nend\r\nfunc choose<T>(T value) as int\r\n return 1\r\nend\r\nfunc choose<T>(T first,T second) as int\r\n return 2\r\nend\r\nfunc main(...) as int\r\n auto box=dep.😀盒::<int>(3)\r\n auto other=dep.😀盒::<dep.Marker>(dep.Marker())\r\n auto callback=identity::<int>\r\n return identity::<int>(box.value)+callback(3)+choose::<int>(1)+choose::<int>(2,3)\r\nend\r\n'
        c.open(entry,source,1);c.open(dep,lib,1);result=check();assert result['ok'],result
        box=location(dep,lib,'😀盒');identity=location(entry,source,'identity');marker=location(dep,lib,'Marker')
        definition(source,'dep.😀盒::<int>',box);definition(source,'dep.😀盒::<dep.Marker>',box)
        references(source,'dep.😀盒::<int>',[box,location(entry,source,'😀盒'),location(entry,source,'😀盒',source.index('😀盒',source.index('😀盒')+1))])
        done('generic-class-origin-with-explicit-create-and-distinct-specializations')
        definition(source,'identity::<int>\r',identity);definition(source,'identity::<int>(',identity)
        references(source,'identity::<int>(',[identity,location(entry,source,'identity',source.index('identity::<int>')),location(entry,source,'identity',source.index('identity::<int>(',source.index('identity::<int>')+1))])
        done('generic-function-values-and-calls-share-the-source-origin')
        assert c.call('textDocument/definition',params(source,'dep.Marker>'))is None
        definition(source,'dep.Marker()',marker)
        done('qualified-type-argument-terminal-token-gap-does-not-redirect-to-outer-class')
        first=source.index('choose');second=source.index('choose',first+1)
        definition(source,'choose::<int>(1)',location(entry,source,'choose',first));definition(source,'choose::<int>(2,3)',location(entry,source,'choose',second))
        references(source,'choose::<int>(1)',[location(entry,source,'choose',first),location(entry,source,'choose',source.index('choose::<int>(1)'))])
        references(source,'choose::<int>(2,3)',[location(entry,source,'choose',second),location(entry,source,'choose',source.index('choose::<int>(2,3)'))])
        done('final-native-overload-selection-and-separate-reference-sets')
        assert c.call('textDocument/hover',params(source,'dep.😀盒::<int>'))
        highlights=c.call('textDocument/documentHighlight',params(source,'dep.😀盒::<int>'));assert len(highlights)==2,highlights
        done('generic-name-hover-and-highlights')
        broken=lib.replace('😀盒','Other');c.change(dep,broken,2);rejected=check();assert not rejected['ok'],rejected
        assert c.call('textDocument/definition',params(source,'dep.😀盒::<int>'))is None
        done('changed-unsaved-origin-never-publishes-stale-binding')
        c.change(dep,lib,3);assert check()['ok'];definition(source,'dep.😀盒::<int>',box)
        done('restored-dependency-rebinds-generic-origin')
        assert not dep.exists() and not (out/'absent/xl.exe').exists();report['status']='passed'
    finally:
        c.stop();(out/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
        for p,h in protected.items():assert sha(p)==h,p
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('PASS generic navigation stdio',len(cases),'cases',flush=True)

if __name__=='__main__':main()
