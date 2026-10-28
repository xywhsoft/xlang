"""Current input proofs and independent completion units over actual stdio LSP.

Every response is compared with a new service process on the exact same input
set. No CLI compiler is available. Includes widened/replaced prefixes, member
and Unicode/BOM/CRLF, other-body edits, overlays/identities and disk guards.
"""
from pathlib import Path
import argparse,hashlib,json
from check_lsp_automatic_diagnostics import AutomaticClient
from check_language_definitions import position

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--expected-version',default='0.97')
    parser.add_argument('--baseline',type=Path)
    parser.add_argument('--baseline-version',default='0.95')
    args=parser.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(server):sha(server),str(Path(__file__)):sha(Path(__file__))};cases=[];clients=[]
    baseline=args.baseline.resolve(strict=True) if args.baseline else None
    if baseline:protected[str(baseline)]=sha(baseline)
    report=dict(status='failed',scope=__doc__,cases=cases,protected=protected)
    def start(folder,reference=None):
        c=AutomaticClient(reference or server,folder/'absent/xl.exe',folder);clients.append(c)
        c.send('initialize',dict(capabilities={},initializationOptions=dict(automaticDiagnostics=False,checkOnSave=False)),1)
        value,_=c.reply(1);assert value['result']['serverInfo']['version']==(args.baseline_version if reference else args.expected_version),value
        c.send('initialized',{});return c
    try:
      for scenario in ['global','member','unicode','disk-import']:
        folder=root/scenario;folder.mkdir();path=folder/'main.xl';extra=folder/'unrelated.xl';dep=folder/'dep.xl'
        disk='export func prefixValue() as int\n return 7\nend\n';dep.write_bytes(disk.encode());path.write_bytes(b'// disk must stay untouched\n');extra.write_bytes(b'// disk must stay untouched\n')
        protected[str(path)]=sha(path);protected[str(extra)]=sha(extra)
        definitions='func otherChoice() as int\r\n return 7\r\nend\r\nfunc prefixOne(int value) as int\r\n return value\r\nend\r\nfunc prefixOther(int value) as int\r\n return value\r\nend\r\n'
        if scenario=='global':template='\ufeff// 中文 😀\r\n'+definitions+'func main(...) as int\r\n @word@\r\n return 0\r\nend\r\n';words=['prefixOne','p','other','o','pr','prefix','prefixO','prefixOne','pr','p','print','int','p','pr']
        elif scenario=='member':template='class Box\n int prefixField\n int otherField\n func prefixMethod(int value) as int\n return value\n end\nend\nfunc main(...) as int\n auto box=Box()\n return box.@word@\nend\n';words=['prefixField','p','other','o','pr','pre','prefixF','p','prefixM']
        elif scenario=='unicode':template='\ufeff// 😀\r\nfunc main(...) as int\r\n int 中文变量=1\r\n int 其他变量=2\r\n return @word@\r\nend\r\n';words=['中文变量','中','其他','其','中文','中文变','中','中文变量']
        else:template='import dep\nfunc main(...) as int\n return dep.@word@\nend\n';words=['prefixValue','p','other','o','pr','pre','prefixV','p','pre']
        c=start(folder);version=0;opened={};body=''
        def query(client,source,revision):
            result=client.call('textDocument/completion',dict(textDocument=dict(uri=path.as_uri()),version=revision,position=position(source,cursorOffset)))
            assert result['version']==revision and result['guardScope']=='background-poll' and result['scope']=='cursor-query' and not result['checkedCompilation'] and result['analysis']['workerThreadId']!=result['analysis']['captureThreadId'],result
            assert result['queryId']==client.request,result
            assert isinstance(result['durationMs'],int) and result['durationMs']>=0,result
            return result
        def checked(name,expected_projection=None):
            value=query(c,body,version);a=value['analysis'];word=a['lastBuildWordView'];assert word['projected'] in (0,1) and not word['astMaterialized'] and not word['astFailures'],word
            if expected_projection is not None:assert word['projected']==expected_projection,(name,word)
            if word['projected']:assert word['proofNodes']>0 and word['knownBytes']>0 and word['scopeComplete']==1 and word['scopeCandidates']>=len(value['items']) and a['lastBuildSemantics']['checked']==a['lastBuildSemantics']['reused']==0 and a['wordViews']>0, a
            oracle=start(folder)
            for p,(text,v) in opened.items():oracle.open(p,text,v)
            oracle.request=c.request-1 # same transport ID,not a semantic exclusion
            fresh=query(oracle,body,version);left={k:v for k,v in value.items() if k not in ('analysis','durationMs')};right={k:v for k,v in fresh.items() if k not in ('analysis','durationMs')}
            assert left==right,(name,left,right)
            assert not fresh['analysis']['lastBuildWordView']['projected']
            oracle.stop();record=dict(name=scenario+'-'+name,passed=True,result=value,fresh=fresh)
            if baseline:
                original=start(folder,baseline)
                for p,(text,v) in opened.items():original.open(p,text,v)
                original.request=c.request-1
                reference=query(original,body,version);assert left=={k:v for k,v in reference.items() if k not in ('analysis','durationMs')},(name,left,reference)
                original.stop();record['baseline']=reference
            cases.append(record)
        for i,word in enumerate(words):
            body=template.replace('@word@',word);cursorOffset=template.index('@word@')+len(word)
            version+=1
            if i:c.change(path,body,version)
            else:c.open(path,body,version)
            opened[path]=(body,version);checked('word-'+str(i),0 if i==0 else 1)
            expected={'global':['otherChoice','prefixOne','prefixOther'],'member':['prefixField','prefixMethod','otherField'],
                'unicode':['中文变量','其他变量'],'disk-import':['prefixValue']}[scenario]
            for label in expected:
                if label.startswith(word):assert any(item['label']==label for item in cases[-1]['result']['items']),(scenario,word,label)
        # Any edit outside the query token or in another overlay must return to
        # the original query. Preserve the complete source/semantic response.
        if scenario!='unicode':
            body=body.replace('return 0','return 1') if 'return 0' in body else '// outside query edit\n'+body
            cursorOffset=body.rfind(words[-1])+len(words[-1])
            version+=1;c.change(path,body,version);opened[path]=(body,version);checked('outside-word',0)
        c.open(extra,'// first overlay\n',1);opened[extra]=('// first overlay\n',1);checked('added-overlay')
        currentWord=words[-1];wordStart=cursorOffset-len(currentWord);body=body[:wordStart]+currentWord+'x'+body[cursorOffset:];cursorOffset+=1
        version+=1;c.change(path,body,version);opened[path]=(body,version);checked('word-with-added-overlay',0)
        c.change(extra,'// changed overlay\n',2);opened[extra]=('// changed overlay\n',2)
        body=body[:cursorOffset]+'x'+body[cursorOffset:];cursorOffset+=1
        version+=1;c.change(path,body,version);opened[path]=(body,version);checked('word-with-changed-overlay',0)
        if scenario=='disk-import':
            dep.write_bytes(disk.replace('return 7','return 8').encode());checked('disk-dependency',0)
        c.stop();protected[str(dep)]=sha(dep)
      assert all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
      report['processes']=[dict(returncode=c.process.poll(),stderr=c.stop(),transcript=c.transcript) for c in clients]
      (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),'utf8')
    print(json.dumps(dict(status='passed',cases=len(cases),processes=len(clients),result=str(root/'result.json'))))

if __name__=='__main__':main()
