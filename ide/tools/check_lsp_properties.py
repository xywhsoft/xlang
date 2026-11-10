"""Native selected property get/let origins over stdio, without name rebinding."""
from pathlib import Path
import argparse,hashlib,json
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    root=out/'属性😀';root.mkdir();entry,dep=root/'main.xl',root/'dep.xl';entry.write_bytes(b'// overlays only\n')
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(p):sha(p)for p in (server,entry,Path(__file__))}
    c=AutomaticClient(server,out/'absent/xl.exe',root);cases=[];report=dict(status='failed',cases=cases,protected=protected)
    name='条目😀';version=1
    lib=('\ufeffexport class Box\r\n int data\r\n get item() as int\r\n return self.data\r\n end\r\n get item(int index) as int\r\n return self.data+index\r\n end\r\n let item(int value)\r\n self.data=value\r\n end\r\n let item(int index,int value)\r\n self.data=value+index\r\n end\r\nend\r\nexport class Other\r\n get item() as int\r\n return 4\r\n end\r\nend\r\nexport class Sink\r\n int data\r\n let output(int value)\r\n self.data=value\r\n end\r\nend\r\n').replace('item',name)
    source=('\ufeffimport dep\r\nfunc main(...) as int\r\n auto box=dep.Box()\r\n auto other=dep.Other()\r\n int index=1\r\n int value=3\r\n box.item=1\r\n box.item[index]=value\r\n box.item[index]+=value\r\n auto sink=dep.Sink()\r\n sink.output=value\r\n return box.item+box.item[index]+other.item\r\nend\r\n').replace('item',name)
    def done(label):cases.append(dict(name=label,passed=True))
    def loc(path,text,at,word):return dict(uri=path.as_uri(),range=dict(start=position(text,at),end=position(text,at+len(word))))
    def params(needle,word=name):return dict(textDocument=dict(uri=entry.as_uri()),position=position(source,source.index(needle)+needle.index(word)))
    def check():return c.call('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=version))
    def definition(needle,expected,word=name):
        actual=c.call('textDocument/definition',params(needle,word));assert actual==expected,(needle,actual,expected)
    def references(needle,expected):
        p=params(needle);p['context']=dict(includeDeclaration=True);actual=c.call('textDocument/references',p)
        key=lambda x:(x['uri'],x['range']['start']['line'],x['range']['start']['character'])
        assert sorted(actual,key=key)==sorted(expected,key=key),(needle,actual,expected)
    try:
        c.send('initialize',dict(capabilities={},initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False,backgroundWorkspaceSymbols=False)),1);reply,_=c.reply(1);assert 'error'not in reply;assert 'renameProvider'not in reply['result']['capabilities'];c.send('initialized',{})
        c.open(entry,source,version);c.open(dep,lib,1);assert check()['ok']
        get=loc(dep,lib,lib.index('get '+name+'()')+4,name);indexGet=loc(dep,lib,lib.index('get '+name+'(int index)')+4,name)
        let=loc(dep,lib,lib.index('let '+name+'(int value)')+4,name);indexLet=loc(dep,lib,lib.index('let '+name+'(int index,')+4,name)
        definition(name+'=1',let);definition(name+'[index]=value',indexLet);definition(name+'[index]+=value',indexGet);definition(name+'+box',get);definition(name+'[index]+other',indexGet)
        done('native-simple-and-indexed-get-let-origins-and-compound-read')
        references(name+'=1',[let,loc(entry,source,source.index(name+'=1'),name)])
        references(name+'[index]=value',[indexLet,loc(entry,source,source.index(name+'[index]=value'),name)])
        references(name+'[index]+other',[indexGet,loc(entry,source,source.index(name+'[index]+=value'),name),loc(entry,source,source.index(name+'[index]+other'),name)])
        done('exact-selected-accessor-references-exclude-other-owners-and-overloads')
        definition('box.'+name+'=1',loc(entry,source,source.index('box=dep.'),'box'),'box')
        definition('index]=value',loc(entry,source,source.index('index=1'),'index'),'index')
        definition('value\r\n box.',loc(entry,source,source.index('value=3'),'value'),'value')
        definition('output=value',loc(dep,lib,lib.index('output(int value)'),'output'),'output')
        done('receivers-index-arguments-rhs-and-write-only-properties-keep-native-binding')
        source='\ufeff// 移动😀\r\n'+source.removeprefix('\ufeff');version+=1;c.change(entry,source,version);assert check()['ok'];definition(name+'=1',let);definition(name+'[index]+other',indexGet)
        done('unsaved-coordinate-edit-rechecks-property-origin')
        c.change(dep,lib.replace(name,'renamed'),2);assert not check()['ok'];assert c.call('textDocument/definition',params(name+'=1'))is None
        c.change(dep,lib,3);assert check()['ok'];definition(name+'=1',let)
        done('changed-unsaved-property-invalidates-and-recovers-without-stale-results')
        assert not dep.exists() and not (out/'absent/xl.exe').exists();report['status']='passed'
    finally:
        c.stop();(out/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
        for p,h in protected.items():assert sha(p)==h,p
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('PASS native property stdio',len(cases),'cases',flush=True)

if __name__=='__main__':main()
