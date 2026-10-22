"""Real typed-index capacity, navigation and bounded-failure recovery over stdio."""
import argparse
import hashlib
import json
from pathlib import Path
from check_language_snapshots import Client
from check_language_definitions import position


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();server=args.server.resolve(strict=True);root=args.output.resolve();root.mkdir(parents=True)
    entry=root/'capacity.xl';disk=b'// checked from open overlay\n';entry.write_bytes(disk)
    sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
    protected={str(p):sha(p) for p in [server,entry,Path(__file__),Path(__file__).with_name('check_language_snapshots.py'),Path(__file__).with_name('check_language_definitions.py')]}
    def source(count):return ''.join('func helper_%d(int value) as int\n return value + 1\nend\n'%i for i in range(count))+'func main(...) as int\n return helper_2999(7)\nend\n'
    text=source(3000);cases=[];report=dict(status='failed',scope='actual 9004-line native typed check/navigation and retained 65536-node limit recovery; no large-workspace latency claim',protected=protected,cases=cases)
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
        oversized=source(6000);c.change(entry,oversized,2);c.check(entry,2,6);r,_=c.reply(6)
        assert r['error']['code']==-32603 and 'index exceeds tree limit' in r['error']['message'],r
        done('existing-node-limit-remains-infrastructure-error',lines=oversized.count('\n'))
        c.change(entry,text,3);c.check(entry,3,7);r,_=c.reply(7)
        assert r['result']['ok'] and r['result']['version']==3 and not r['result']['count'],r
        assert r['result']['analysis']['builds']==2 and r['result']['analysis']['cacheHits']==1,r
        assert call('textDocument/definition',params,8)==expected
        done('capacity-failure-preserves-valid-retained-batch')
        assert entry.read_bytes()==disk and all(sha(p)==h for p,h in protected.items())
        report['status']='passed'
    finally:
        c.stop();(root/'transcript.json').write_text(json.dumps(c.transcript,ensure_ascii=False,indent=2),encoding='utf-8');(root/'stderr.log').write_text(c.stderr,encoding='utf-8')
        (root/'result.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(status=report['status'],cases=len(cases),proof=str(root/'result.json'))))


if __name__=='__main__':main()
