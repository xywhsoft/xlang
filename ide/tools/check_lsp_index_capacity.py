"""Real typed-index capacity, navigation and bounded-failure recovery over stdio."""
import argparse
import hashlib
import json
from pathlib import Path
from check_language_snapshots import Client
from check_language_definitions import position
from check_lsp_module_semantics import source as mixed_source, initialize as configure, checked
from check_lsp_automatic_diagnostics import AutomaticClient
from lsp_resource_case import index_byte_limit_source


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True)
    entry=root/'capacity.xl';disk=b'// checked from open overlay\n';entry.write_bytes(disk)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [server,entry,Path(__file__),Path(__file__).with_name('check_language_snapshots.py'),Path(__file__).with_name('check_language_definitions.py')]}
    def source(count):return ''.join('func helper_%d(int value) as int\n return value + 1\nend\n'%i for i in range(count))+f'func main(...) as int\n return helper_{count-1}(7)\nend\n'
    text=source(3000);cases=[];report=dict(status='failed',scope='actual 9004/18004-line static checks with typed navigation beyond65536 facts; real byte-budget failure/recovery and15889-line two-member calls/flow with opened/closed entries; no latency/full-scale claim',protected=protected,cases=cases)
    c=Client(server,root/'absent/xl.exe',root)
    def done(name,**data):cases.append(dict(name=name,passed=True,**data))
    def call(method,params,id):
        c.send(method,params,id);r,_=c.reply(id);assert 'error' not in r,r;return r['result']
    try:
        c.initialize('native');c.open(entry,text,1);c.check(entry,1,2);r,_=c.reply(2)
        assert r['result']['ok'] and not r['result']['count'] and r['result']['semanticDefinitions'],r
        assert r['result']['analysis']['builds']==1 and not r['result']['analysis']['cacheHits'],r
        done('9004-line-typed-check-without-executor',lines=text.count('\n'))
        needle='helper_2999';declaration=text.index(needle);use=text.rindex(needle)
        params=dict(textDocument=dict(uri=entry.as_uri()),position=position(text,use))
        expected=dict(uri=entry.as_uri(),range=dict(start=position(text,declaration),end=position(text,declaration+len(needle))))
        assert call('textDocument/definition',params,3)==expected
        done('standard-definition-resolves-last-helper')
        hover=call('textDocument/hover',params,4);encoded=json.dumps(hover,ensure_ascii=False)
        assert 'helper_2999' in encoded and 'int' in encoded,hover
        done('hover-uses-current-large-typed-index')
        symbols=call('textDocument/documentSymbol',dict(textDocument=dict(uri=entry.as_uri())),5)
        names=json.dumps(symbols,ensure_ascii=False);assert 'helper_0' in names and 'helper_2999' in names and 'main' in names
        done('document-symbols-include-first-and-last-functions')
        expanded=source(6000);c.change(entry,expanded,2);c.check(entry,2,6);r,_=c.reply(6)
        assert r['result']['ok'] and not r['result']['count'] and r['result']['semanticDefinitions'],r
        done('18004-line-check-grows-beyond-old-arena',lines=expanded.count('\n'))
        target='helper_5999';params_large=dict(textDocument=dict(uri=entry.as_uri()),position=position(expanded,expanded.rindex(target)))
        expected_large=dict(uri=entry.as_uri(),range=dict(start=position(expanded,expanded.index(target)),end=position(expanded,expanded.index(target)+len(target))))
        assert call('textDocument/definition',params_large,7)==expected_large
        done('late-declaration-after-old-bound-resolves')
        # Few long,unique identifiers reach the real native byte bound while
        # staying below the8MiB source limit. This is valid language input.
        oversized=index_byte_limit_source()
        assert len(oversized.encode())<8*1024*1024
        c.change(entry,oversized,3);c.check(entry,3,8);r,_=c.reply(8)
        assert r['error']['code']==-32603 and 'index exceeds byte limit' in r['error']['message'],r
        done('real-byte-budget-remains-infrastructure-error',utf8Bytes=len(oversized.encode()))
        c.change(entry,expanded,4);c.check(entry,4,9);r,_=c.reply(9)
        assert r['result']['ok'] and r['result']['version']==4 and not r['result']['count'],r
        assert r['result']['analysis']['builds']==3 and r['result']['analysis']['cacheHits']==1,r
        assert call('textDocument/definition',params_large,10)==expected_large
        done('byte-failure-preserves-valid-expanded-retained-batch')
        c.stop()
        for opened in [False,True]:
            folder=root/('opened-project' if opened else 'disk-project');folder.mkdir()
            main,left,right=[folder/n for n in ['main.xl','member_left.xl','member_right.xl']]
            main_body='import member_left\nimport member_right\nfunc main(...) as int\n return member_left.compute(3)+member_right.compute(4)\nend\n'
            main.write_bytes(main_body.encode());protected[str(main)]=sha(main)
            client=AutomaticClient(server,folder/'absent/xl.exe',folder)
            try:
                configure(client,main,[main,left,right],folder)
                if opened:client.open(main,main_body,1)
                bodies=[mixed_source(256,0,False),mixed_source(256,1,True)]
                client.open(left,bodies[0],7);client.open(right,bodies[1],7)
                value=checked(client,left,7)
                assert value['ok'] and not value['count'] and value['semanticDefinitions'] and value['entryOpen']==opened,value
                at=bodies[1].index('func work_255(');definition=client.call('textDocument/definition',dict(textDocument=dict(uri=right.as_uri()),version=7,position=position(bodies[1],bodies[1].index('return local',at)+7)))
                assert definition['uri']==right.as_uri() and definition['range']['start']==position(bodies[1],bodies[1].index('local =',at)),definition
                done(('opened' if opened else 'disk')+'-15889-line-project-typed-navigation',lines=main_body.count('\n')+sum(b.count('\n') for b in bodies))
                before=value['analysis']['semanticFunctionsReused']
                for d,path in enumerate([left,right]):
                    bodies[d]=bodies[d].replace(f'identity(value)+{d}\r\nend\r\n',f'identity(value)+{d+2}\r\nend\r\n')
                    client.change(path,bodies[d],8)
                value=checked(client,left,8)
                assert value['ok'] and not value['count'] and value['analysis']['semanticFunctionsReused']>before,value
                documents={Path(row['path']):row for row in value['documents']}
                assert documents[left]['version']==documents[right]['version']==8,documents
                done(('opened' if opened else 'disk')+'-large-project-body-edits-retain-real-function-reuse',analysis=value['analysis'])
            finally:client.stop();assert client.process.returncode==0
        assert entry.read_bytes()==disk and all(sha(p)==h for p,h in protected.items())
        report['status']='passed'
    finally:
        c.stop();(root/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8');(root/'stderr.log').write_text(c.stderr,encoding='utf-8')
        (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(root/'result.json'))))


if __name__=='__main__':main()
