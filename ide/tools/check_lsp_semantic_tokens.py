"""Actual stdio semantic tokens, negotiated legends, UTF16, numeric deltas and guards.

An absent compiler proves that requests use the retained native backend. Normal
results and failures are recorded; timing is observation, not goal admission.
"""
from __future__ import annotations
import argparse,copy,hashlib,json,time
from pathlib import Path
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position
TYPES=['method','function','enumMember','property','variable','parameter','typeParameter','struct','enum','interface','class','type']
CAP=dict(requests=dict(full=dict(delta=True),range=True),tokenTypes=TYPES,tokenModifiers=['async','readonly','declaration'],formats=['relative'],augmentsSyntaxTokens=True)


def decode(data,legend):
    assert len(data)%5==0 and all(type(x)is int and 0<=x<=2147483647 for x in data),data
    rows=[];line=column=0
    for i in range(0,len(data),5):
        dl,dc,length,kind,mods=data[i:i+5];line+=dl;column=column+dc if not dl else dc
        assert length>0 and kind<len(legend['tokenTypes']) and mods<1<<len(legend['tokenModifiers'])
        rows.append((line,column,length,legend['tokenTypes'][kind],tuple(m for j,m in enumerate(legend['tokenModifiers'])if mods&(1<<j))))
    assert all(a[0]<b[0] or a[0]==b[0] and a[1]+a[2]<=b[1] for a,b in zip(rows,rows[1:])),rows
    return rows


def apply(previous,result):
    if 'data'in result:return result['data']
    out=previous.copy();edits=result['edits'];assert len(edits)<=1,edits
    for edit in sorted(edits,key=lambda x:x['start'],reverse=True):
        start,count=edit['start'],edit['deleteCount'];assert type(start)is int and type(count)is int and 0<=start<=len(previous) and 0<=count<=len(previous)-start
        out[start:start+count]=edit.get('data',[])
    return out


def checked(c,path,version):
    return c.call('xlang/check',dict(textDocument=dict(uri=path.as_uri()),version=version))


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--server',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    server=args.server.resolve(strict=True);out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();protected={str(p):sha(p)for p in (server,Path(__file__),Path(__file__).with_name('check_language_snapshots.py'))};clients=[];cases=[]
    report=dict(status='failed',scope=__doc__,protected=protected,cases=cases)
    def done(name,**fields):cases.append(dict(name=name,passed=True,**fields))
    def client(name,cap=CAP,options=None,expected=True):
        folder=out/name;folder.mkdir();c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c)
        options=dict(automaticDiagnostics=False,checkOnSave=False,backgroundWorkspaceSymbols=False,**(options or {}))
        capabilities={}if cap is None else dict(textDocument=dict(semanticTokens=cap))
        c.send('initialize',dict(capabilities=capabilities,initializationOptions=options),1);reply,_=c.reply(1)
        if expected:
            assert 'error'not in reply,reply;c.send('initialized',{})
        else:assert reply.get('error',{}).get('code')==-32602,reply
        return c,folder,reply
    def request(c,path,method='full',**extra):
        began=time.perf_counter();result=c.call('textDocument/semanticTokens/'+method,dict(textDocument=dict(uri=path.as_uri()),**extra));return result,(time.perf_counter()-began)*1000
    def error(c,method,params,code):
        c.request+=1;c.send(method,params,c.request);reply,_=c.reply(c.request);assert reply.get('error',{}).get('code')==code,reply
    source=('\ufefftypedef Count = int\r\nclass Box\r\n int field\r\n func read() as int\r\n  return self.field\r\n end\r\nend\r\n'
        'func pick(int 输入) as int\r\n return 输入\r\nend\r\nfunc pick(string 输入) as int\r\n return 1\r\nend\r\n'
        'func main(...) as int\r\n string 文本="😀"; Count 局部=pick(4)\r\n auto box=Box()\r\n return 局部+box.read()+pick(文本)\r\nend\r\n')
    try:
        c,folder,reply=client('full');provider=reply['result']['capabilities']['semanticTokensProvider'];assert provider['full']==dict(delta=True) and provider['range']is True;legend=provider['legend']
        path=folder/'中文😀.xl';path.write_bytes(b'// disk differs from overlay\n');protected[str(path)]=sha(path);c.open(path,source,1)
        initial,ms=request(c,path);assert initial['resultId'];rows=decode(initial['data'],legend);assert rows
        def at(text,marker,word,kind,mods=()):
            offset=text.index(word,text.index(marker));pos=position(text,offset);found=[r for r in rows if r[:2]==(pos['line'],pos['character'])]
            assert found and found[0][2:]==(len(word.encode('utf-16-le'))//2,kind,mods),(marker,word,found)
        at(source,'Count =','Count','type',('declaration',));at(source,'func read','read','method',('declaration',));at(source,'self.field','field','property');at(source,'pick(int','输入','parameter',('declaration',));at(source,'pick(4)','pick','function');at(source,'box.read()','read','method');at(source,'局部=','局部','variable',('declaration',))
        for row in rows:
            line=source.splitlines()[row[0]];assert row[1]+row[2]<=len(line.encode('utf-16-le'))//2,row
        done('native-full-without-private-check-or-compiler',tokens=len(rows),observedMs=ms)
        a=position(source,source.index('read()')+1);z=position(source,source.index('read()')+2);part,_=request(c,path,'range',range=dict(start=a,end=z));partial=decode(part['data'],legend);assert len(partial)==1 and partial[0][3]=='method' and partial[0][2]==4;assert 'resultId'not in part
        empty,_=request(c,path,'range',range=dict(start=a,end=a));assert empty==dict(data=[]);done('range-includes-whole-partial-overlap-and-document-relative-start')
        moved=source[:1]+'\n'+source[1:];c.change(path,moved,2)
        delta,ms=request(c,path,'full/delta',previousResultId=initial['resultId']);assert 'edits'in delta and delta['edits'],delta
        reconstructed=apply(initial['data'],delta);fresh,_=request(c,path);assert reconstructed==fresh['data'];done('numeric-array-delta-reconstructs-after-bom-newline',observedMs=ms,edits=delta['edits'])
        literals=moved.replace('return 1\r\n','return 2\r\n');c.change(path,literals,3)
        unchanged,_=request(c,path,'full/delta',previousResultId=delta['resultId']);assert unchanged.get('edits')==[],unchanged
        unknown,_=request(c,path,'full/delta',previousResultId='not-a-result');assert unknown['data']==reconstructed;done('delta-chain-empty-edit-and-unknown-base-full-fallback')
        other=folder/'other.xl';c.open(other,literals,1);cross,_=request(c,other,'full/delta',previousResultId=unknown['resultId']);assert 'data'in cross;done('previous-result-id-is-scoped-to-document')
        switched,_=request(c,path);assert switched['data']==reconstructed;done('switch-between-already-open-unrelated-entries')
        for value in (None,False,1,[],{}):error(c,'textDocument/semanticTokens/full/delta',dict(textDocument=dict(uri=path.as_uri()),previousResultId=value),-32602)
        bad_ranges=[dict(start=dict(line=-1,character=0),end=dict(line=0,character=0)),dict(start=dict(line=2,character=0),end=dict(line=1,character=0))]
        emoji=position(literals,literals.index('😀'));bad_ranges.append(dict(start=dict(line=emoji['line'],character=emoji['character']+1),end=dict(line=emoji['line'],character=emoji['character']+2)))
        for value in bad_ranges:error(c,'textDocument/semanticTokens/range',dict(textDocument=dict(uri=path.as_uri()),range=value),-32602)
        error(c,'textDocument/semanticTokens/full',dict(textDocument=dict(uri=path.as_uri()),version=2),-32602);done('malformed-delta-range-surrogate-and-stale-version-rejected')
        broken='func main(...) as int\n return missing\nend\n';c.change(path,broken,4);rejected,_=request(c,path);assert rejected is None;again,_=request(c,path);assert again is None
        unaffected,_=request(c,other);assert unaffected['data']==reconstructed;done('source-rejection-does-not-hide-another-open-entry')
        c.change(path,literals,5);recovered,_=request(c,path);assert recovered['data']==reconstructed;done('source-rejection-never-replays-old-data-and-recovers')
        c.send('textDocument/didClose',dict(textDocument=dict(uri=other.as_uri())));c.open(other,literals,2);reopened,_=request(c,other,'full/delta',previousResultId=cross['resultId']);assert 'data'in reopened;done('close-reopen-discards-history');c.stop()
        for name,cap in [('missing',None),('unsupported-format',dict(CAP,formats=['future'])),('non-augment',dict(CAP,augmentsSyntaxTokens=False)),('no-request',dict(CAP,requests={})),('unknown-types',dict(CAP,tokenTypes=['future']))]:
            t,folder,reply=client(name,cap);assert 'semanticTokensProvider'not in reply['result']['capabilities'];p=folder/'main.xl';t.open(p,source,1);error(t,'textDocument/semanticTokens/full',dict(textDocument=dict(uri=p.as_uri())),-32601);t.stop()
        done('absent-or-unsupported-client-keeps-lexical-fallback')
        partial_cap=dict(CAP,requests=dict(full=True),tokenTypes=['function','variable','type'],tokenModifiers=[])
        t,folder,reply=client('fallback-types',partial_cap);provider=reply['result']['capabilities']['semanticTokensProvider'];assert provider.get('full')is True and 'range'not in provider;leg=provider['legend'];assert leg['tokenTypes']==['type','variable','function'] and leg['tokenModifiers']==[]
        p=folder/'main.xl';t.open(p,source,1);result,_=request(t,p);decoded=decode(result['data'],leg);assert any(row[3]=='function' and row[2]==4 for row in decoded) and all(not row[4]for row in decoded);t.stop();done('limited-type-and-modifier-legend-fallback')
        t,folder,reply=client('range-only',dict(CAP,requests=dict(range={})));assert 'full'not in reply['result']['capabilities']['semanticTokensProvider'];p=folder/'main.xl';t.open(p,source,1);result,_=request(t,p,'range',range=dict(start=dict(line=0,character=0),end=position(source,len(source))));assert result['data'];error(t,'textDocument/semanticTokens/full',dict(textDocument=dict(uri=p.as_uri())),-32601);t.stop();done('range-only-negotiation')
        for index,cap in enumerate([None,False,[],dict(CAP,requests=[]),dict(CAP,tokenTypes='type'),dict(CAP,tokenModifiers=[None]),dict(CAP,formats=[] ,requests=dict(full=dict(delta='true')))]):
            # The missing capability is valid; explicit JSON null is invalid.
            t,folder,reply=client('invalid-'+str(index),cap if cap is not None else {},expected=False)
            t.send('initialize',dict(capabilities=dict(textDocument=dict(semanticTokens=CAP)),initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False,backgroundWorkspaceSymbols=False)),2);reply,_=t.reply(2);assert 'error'not in reply,reply;t.send('initialized',{});t.stop()
        done('invalid-capabilities-and-reinitialize')
        t,folder,reply=client('dependency');p=folder/'main.xl';dep=folder/'dep.xl';original='export func value() as int\n return 7\nend\n';dep.write_text(original,encoding='utf8');entry='import dep\nfunc main(...) as int\n return dep.value()\nend\n';t.open(p,entry,1);first,_=request(t,p);assert first['data'];dep.write_text('export func value() as string\n return "bad"\nend\n',encoding='utf8');time.sleep(1.1);stale,_=request(t,p,'full/delta',previousResultId=first['resultId']);assert stale is None;dep.write_text(original,encoding='utf8');time.sleep(1.1);good,_=request(t,p);assert good['data']==first['data'];t.stop();done('real-disk-dependency-revalidation')
        report['status']='passed'
    finally:
        for index,c in enumerate(clients):
            c.stop();(out/('transcript-'+str(index)+'.json')).write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
        for p,h in protected.items():assert sha(p)==h,p
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print('PASS semantic token stdio',len(cases),'cases',flush=True)

if __name__=='__main__':main()
