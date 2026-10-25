"""Private pooled check copies preserve original full wire reports and inputs.

Compare a baseline service at identical paths/versions across real unsaved
header/body/error/repair inputs. Creation work stays immutable on exact hits.
Native fixtures independently prove AST/index/IR fields and parser stack release.
"""
from pathlib import Path
import argparse
import hashlib
import json
import time
from check_lsp_automatic_diagnostics import AutomaticClient
from check_lsp_module_semantics import initialize
from check_lsp_header_syntax import program


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--baseline',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();out=args.output.resolve();out.mkdir(parents=True,exist_ok=False)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    cases=[];protected={};report=dict(status='failed',scope=__doc__,cases=cases,protected=protected)
    try:
        for helpers in [16,96]:
            folder=out/str(helpers);folder.mkdir();entry,dep=[folder/name for name in ['main.xl','dep.xl']]
            main='import dep\r\nfunc main(...) as int\r\n return dep.compute(7)\r\nend\r\n'
            for path in [entry,dep]:path.write_bytes(b'// immutable disk;unsaved inputs only\n');protected[str(path)]=sha(path)
            expected=[]
            for variant,server in [('before',args.baseline.resolve(strict=True)),('after',args.server.resolve(strict=True))]:
                protected[str(server)]=sha(server);c=AutomaticClient(server,folder/'absent/xl.exe',folder)
                try:
                    initialize(c,entry,[entry,dep],folder);c.open(entry,main,1)
                    for generation in range(7):
                        text=program(helpers,generation)
                        if not generation:c.open(dep,text,1)
                        else:c.change(dep,text,generation+1)
                        work=None
                        for repeat in range(2):
                            until=time.monotonic()+30
                            while True:
                                c.request+=1;c.send('xlang/check',dict(textDocument=dict(uri=entry.as_uri()),version=1),c.request);reply,_=c.reply(c.request)
                                if reply.get('error',{}).get('code')!=-32001:break
                                assert time.monotonic()<until;time.sleep(.002)
                            assert 'error' not in reply,reply
                            result=reply['result'];assert result['version']==1 and result['ok']==(generation!=5),result
                            assert result['checkId']==c.request and result['durationMs']>=0,result
                            actual={k:v for k,v in result.items() if k not in ['analysis','durationMs','checkId']}
                            if variant=='before':expected.append(actual)
                            else:
                                assert actual==expected[generation*2+repeat],(helpers,generation,repeat,actual,expected[generation*2+repeat])
                                copied=result['analysis']['lastBuildMaterialization']
                                assert copied['calls']>0 and copied['pooledCopies']==copied['calls'] and copied['heapCopies']==copied['poolFallbacks']==0,copied
                                assert 0<copied['chargedBytes']<=64*1024*1024 and copied['nodes']>0 and copied['pages']>0,copied
                                if repeat:assert copied==work,(copied,work)
                                work=copied
                            cases.append(dict(helpers=helpers,variant=variant,generation=generation,repeat=repeat,accepted=result['ok'],diagnostics=result['count'],materialization=work,fullWireSha256=hashlib.sha256(json.dumps(actual,sort_keys=True,ensure_ascii=False).encode()).hexdigest()))
                finally:
                    stderr=c.stop();(folder/(variant+'-stderr.log')).write_text(stderr,encoding='utf8')
                    (folder/(variant+'-transcript.json')).write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf8')
                    assert not c.process.returncode
        assert all(sha(path)==value for path,value in protected.items())
        report['status']='passed'
    finally:(out/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),evidence=str(out/'result.json'))))


if __name__=='__main__':main()
