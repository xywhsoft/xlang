"""Actual checked interface-version snapshots and current-source envelopes.

The native fixture proves version values, full fresh parity and an actual IR
dependency mismatch. This suite checks the stdio adapter with disk/opened
entry, exact reuse, real edits, errors and recovery; no new capability claim.
"""
from pathlib import Path
import argparse,hashlib,json,time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize
from check_language_definitions import position

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--server',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();server=a.server.resolve(strict=True);out=a.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.file_digest(Path(p).open('rb'),'sha256').hexdigest();protected={str(server):sha(server),str(Path(__file__)):sha(__file__)};clients=[];cases=[];report=dict(status='failed',scope=__doc__,cases=cases,protected=protected)
    try:
        for opened in [False,True]:
            folder=out/('opened' if opened else 'disk');folder.mkdir();entry,dep=[folder/n for n in ['main.xl','dep.xl']];main='\ufeffimport dep\r\nfunc main(...) as int\r\n return dep.compute(7)\r\nend\r\n';entry.write_bytes(main.encode());protected[str(entry)]=sha(entry)
            c=AutomaticClient(server,folder/'absent/xl.exe',folder);clients.append(c);initialize(c,entry,[entry,dep],folder)
            if opened:c.open(entry,main,1)
            first=None;version=0
            for phase in ['cold','exact','body','overload-default','broken','repaired']:
                source='\ufeff// 中文😀\r\nexport class Data\r\n int field\r\nend\r\nexport func compute(int value) as int\r\n return '+('missingName' if phase=='broken' else 'value+2' if phase=='body' else 'value+1')+'\r\nend\r\nexport func compute(string value,int pad='+('2' if phase=='overload-default' else '1')+') as int\r\n return pad\r\nend\r\n'
                if phase!='exact':
                    version+=1
                    if version==1:c.open(dep,source,version)
                    else:c.change(dep,source,version)
                def read_check():
                    until=time.monotonic()+30
                    while True:
                        c.request+=1;c.send('xlang/check',dict(textDocument=dict(uri=dep.as_uri()),version=version),c.request);response,_=c.reply(c.request)
                        if response.get('error',{}).get('code')!=-32001:break
                        assert time.monotonic()<until;time.sleep(.001)
                    assert 'error' not in response,response
                    return response['result']
                r=read_check();okay=phase!='broken';assert r['version']==version and r['analysisContext']['epoch']==1 and r['ok']==okay and bool(r['count'])!=okay,r
                interfaces=r['analysis']['entityInterfaces'];assert set(interfaces)=={'symbols','types','bytes','declined'} and all(isinstance(v,int) and v>=0 for v in interfaces.values()) and interfaces['bytes']<=16*1024*1024,interfaces
                if okay:
                    assert interfaces['symbols']>0 and interfaces['types']>0 and interfaces['bytes']>0 and r['semanticDefinitions'],r
                    assert next(d for d in r['documents'] if Path(d['path'])==dep)['version']==version,r
                else:assert interfaces==dict(symbols=0,types=0,bytes=0,declined=0) and not r['semanticDefinitions'],r
                if phase=='cold':first=r
                if phase=='exact':assert interfaces==first['analysis']['entityInterfaces'] and r['analysis']['builds']==first['analysis']['builds'],r
                if okay:
                    nav=r['analysis']['navigation'];assert nav['deferred'] and not nav['builds'] and not nav['cacheHits'] and nav['deferredBytes']>0,nav
                    params=dict(textDocument=dict(uri=dep.as_uri()),version=version,checkId=r['checkId'],position=position(source,source.index('return value')+len('return ')))
                    definition=c.call('textDocument/definition',params);assert definition['uri']==dep.as_uri() and definition['range']['start']==position(source,source.index('value)')),definition
                    hover=c.call('textDocument/hover',params);assert hover and hover['contents'],hover
                    current=read_check();after=current['analysis']['navigation'];expected_hit=int(phase=='exact')
                    assert current['analysis']['entityInterfaces']==interfaces and current['analysis']['builds']==r['analysis']['builds'],current
                    assert after['storeHits']==nav['storeHits']+expected_hit and after['published']==nav['published']+1-expected_hit and after['residentBytes']>0,(nav,after)
                cases.append(dict(name=('opened-' if opened else 'disk-')+phase,passed=True,interfaces=interfaces,version=version))
            c.stop();assert not c.process.returncode and not dep.exists()
        assert len(cases)==12 and all(sha(p)==h for p,h in protected.items());report['status']='passed'
    finally:
        for i,c in enumerate(clients):
            (out/f'stderr-{i}.log').write_text(c.stop(),encoding='utf8');(out/f'transcript-{i}.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
        (out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(out/'result.json'))))
if __name__=='__main__':main()
