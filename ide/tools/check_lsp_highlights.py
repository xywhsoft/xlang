"""Actual standard document highlights, current semantic inputs and lifecycle."""
import argparse
import hashlib
import json
from pathlib import Path
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [server,Path(__file__),*[Path(__file__).with_name(n) for n in ['check_lsp_automatic_diagnostics.py','check_language_snapshots.py','check_language_definitions.py']]]}
    clients=[];cases=[];report=dict(status='failed',scope='standard documentHighlight exact identities and UTF16 ranges, native cold/retained queries without executor, current input/lifecycle; no IDE highlight display or whole workspace references claim',protected=protected,cases=cases)
    good='func main(...) as int\n int result = 7\n return result\nend\n'
    corpus='\ufeff// 值 choose other\r\ntypedef Count = int\r\nfunc choose(int value) as int\r\n return value\r\nend\r\nfunc choose(string value) as int\r\n return 2\r\nend\r\nfunc main(...) as int\r\n Count 值 = 1; string 文本 = "😀"\r\n int other = choose(值)\r\n if true\r\n  int 值 = 2\r\n  other = other + 值\r\n end\r\n return choose(值) + choose(文本) + other\r\nend\r\n'
    heavy=''.join(f'func helper_{i}(int value) as int\n return value + 1\nend\n' for i in range(3000))+'func main(...) as int\n return helper_2999(7)\nend\n'

    def done(name,**data):cases.append(dict(name=name,passed=True,**data))
    def client(name,backend='native'):
        folder=root/name;folder.mkdir();c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c)
        c.send('initialize',dict(capabilities=dict(textDocument=dict(documentHighlight=dict(dynamicRegistration=False))),initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False,diagnosticsBackend=backend)),1)
        reply,_=c.reply(1);assert reply['result']['capabilities']['documentHighlightProvider'] is True,reply;c.send('initialized',{});return c,folder
    def opened(c,path,text,version=1):
        path.write_bytes(b'// actual analysis uses unsaved text\n');protected[str(path)]=sha(path);c.open(path,text,version)
    def params(path,text,marker,skip=0,**extra):
        return dict(textDocument=dict(uri=path.as_uri()),position=position(text,text.index(marker)+skip),**extra)
    def expected(text,tokens):
        return [dict(range=dict(start=position(text,text.index(marker)+skip),end=position(text,text.index(marker)+skip+size)),kind=1) for marker,skip,size in tokens]
    def query(c,path,text,token,tokens,**extra):
        out=c.call('textDocument/documentHighlight',params(path,text,token[0],token[1],**extra));assert out==expected(text,tokens),(out,expected(text,tokens));return out
    def error(c,p,code):
        c.request+=1;c.send('textDocument/documentHighlight',p,c.request);reply,_=c.reply(c.request);assert reply['error']['code']==code,reply

    try:
        c,folder=client('identities');entry=folder/'入口😀.xl';opened(c,entry,corpus,7)
        outer=[('Count 值',6,1),('other = choose(值)',15,1),('return choose(值)',14,1)]
        # Python offsets count code points; C/compiler byte offsets are converted
        # independently by the service, including BOM/CRLF and preceding emoji.
        first=query(c,entry,corpus,outer[1],outer,version=7);done('cold-unsaved-Unicode-BOM-CRLF-highlights-without-check-or-executor')
        for _ in range(15):assert query(c,entry,corpus,outer[0],outer,version=7)==first
        checked=c.call('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=7));assert checked['analysis']['builds']==1 and checked['analysis']['cacheHits']==1,checked
        done('repeated-queries-and-declaration-reuse-one-actual-checked-compilation')
        inner=[('int 值 = 2',4,1),('other + 值',8,1)];query(c,entry,corpus,inner[1],inner,version=7);done('lexical-shadowing-keeps-distinct-identities')
        integer=[('func choose(int',5,6),('other = choose(值)',8,6),('return choose(值)',7,6)]
        strings=[('func choose(string',5,6),('+ choose(文本)',2,6)]
        query(c,entry,corpus,integer[1],integer,version=7);query(c,entry,corpus,strings[1],strings,version=7);done('resolved-overload-callees-do-not-merge-by-name')
        aliases=[('typedef Count',8,5),('Count 值',0,5)];query(c,entry,corpus,aliases[1],aliases,version=7);done('type-identities-are-separated-from-value-symbols')
        for token in [('😀',0,1),('// 值',3,1),('func main',0,4)]:query(c,entry,corpus,token,[],version=7)
        done('comments-literals-and-keywords-have-no-fabricated-symbol-occurrences')
        invalid=params(entry,corpus,'😀');invalid['position']['character']+=1;error(c,invalid,-32602)
        error(c,params(entry,corpus,'Count 值',version=6),-32602);error(c,params(entry,corpus,'Count 值',checkId=99999),-32801)
        done('split-surrogate-stale-version-and-stale-check-rejected')
        changed=corpus.replace('Count 值 = 1','Count 外值 = 1').replace('choose(值)','choose(外值)')
        c.change(entry,changed,8);renamed=[('Count 外值',6,2),('other = choose(外值)',15,2),('return choose(外值)',14,2)]
        query(c,entry,changed,renamed[1],renamed,version=8);done('unsaved-edit-rebuilds-current-identities-and-ranges');c.stop()

        c,folder=client('imports');entry=folder/'entry.xl';dep=folder/'dep.xl';text='import dep\nfunc main(...) as int\n return dep.value() + dep.value()\nend\n';dependency='export func value() as int\n return 7\nend\n'
        opened(c,entry,text);opened(c,dep,dependency,3)
        calls=[('return dep.value',11,5),('+ dep.value',6,5)];query(c,entry,text,calls[0],calls)
        query(c,dep,dependency,('func value',5,5),[('func value',5,5)],version=3)
        done('imported-symbol-highlights-stay-within-requested-file')
        bad='export func value() as int\n return "bad"\nend\n';c.change(dep,bad,4)
        assert c.call('textDocument/documentHighlight',params(entry,text,'dep.value',4)) is None
        c.change(dep,dependency,5);query(c,entry,text,calls[0],calls);done('changed-dependency-errors-and-recovery-revalidate-facts');c.stop()

        c,folder=client('errors');entry=folder/'entry.xl';broken='func main(...) as int\n return missingName\nend\n';opened(c,entry,broken)
        assert c.call('textDocument/documentHighlight',params(entry,broken,'missingName')) is None
        assert c.latest[entry.as_uri()]['diagnostics'],c.latest
        c.change(entry,good,2);tokens=[('int result',4,6),('return result',7,6)];query(c,entry,good,tokens[1],tokens,version=2)
        done('source-errors-return-standard-null-with-diagnostics-and-recover');c.stop()

        for event in ['cancel','change','close','shutdown']:
            c,folder=client('pending-'+event);entry=folder/'entry.xl';opened(c,entry,heavy)
            c.send('textDocument/documentHighlight',params(entry,heavy,'return helper_2999',7),20)
            if event=='cancel':c.send('$/cancelRequest',dict(id=20))
            elif event=='change':c.change(entry,good,2)
            elif event=='close':c.send('textDocument/didClose',dict(textDocument=dict(uri=entry.as_uri())))
            else:c.send('shutdown',{},21)
            pending={20,21} if event=='shutdown' else {20};rows={}
            while pending:
                row=c.next()
                if 'id' in row:assert row['id'] in pending,row;pending.remove(row['id']);rows[row['id']]=row
            assert rows[20]['error']['code']==(-32801 if event=='change' else -32800),rows
            if event=='shutdown':assert rows[21].get('result','missing') is None,rows
            elif event=='close':c.open(entry,good,1);query(c,entry,good,tokens[1],tokens)
            else:
                if event=='cancel':c.change(entry,good,2)
                query(c,entry,good,tokens[1],tokens,version=2)
            done('pending-'+event+'-one-response-and-current-identity-recovery');c.stop()

        c,folder=client('cli','cli');entry=folder/'entry.xl';opened(c,entry,good);error(c,params(entry,good,'result'),-32801)
        done('explicit-cli-requires-successful-manual-index');c.stop()
        assert all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            c.stop();(root/f'transcript-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8')
        (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(root/'result.json')),ensure_ascii=False))


if __name__=='__main__':main()
