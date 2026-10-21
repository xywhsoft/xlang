"""Real compiler checks that ordered LSP edits update exactly the intended buffer."""
import argparse
import hashlib
import json
from pathlib import Path
from check_language_snapshots import Client
from check_language_definitions import position


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server',type=Path,required=True)
    parser.add_argument('--compiler',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    entry=output/'main.xl';disk=b'func main(...) as int\n return 0\nend\n';entry.write_bytes(disk)
    server=args.server.resolve(strict=True);compiler=args.compiler.resolve(strict=True)
    inputs=[dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest())
            for p in [server,server.with_name('xge.dll'),compiler,compiler.with_name('xlc.dll'),compiler.with_name('xl.dll')]]
    client=Client(server,compiler,output);rows=[];request_id=10
    version=1;text='func main(...) as int\n int result = 7\n return res\nend\n'
    def query(expected,body=None,revision=None):
        nonlocal request_id
        body=text if body is None else body
        revision=version if revision is None else revision
        caret=body.rindex('return ')+len('return ')+len(expected[:3])
        request_id+=1
        client.send('textDocument/completion',dict(textDocument=dict(uri=entry.as_uri()),
                    version=revision,position=position(body,caret)),request_id)
        message,_=client.reply(request_id)
        assert 'error' not in message,message
        result=message['result'];assert result['available'] and result['version']==revision,result
        assert [x['label'] for x in result['items']]==[expected],result['items']
        return result
    def ranged(body,start,end,replacement,**extra):
        return dict(range=dict(start=position(body,start),end=position(body,end)),text=replacement,**extra)
    def send(changes,revision):
        client.send('textDocument/didChange',dict(textDocument=dict(uri=entry.as_uri(),version=revision),contentChanges=changes))
    def done(name):rows.append(dict(name=name,passed=True))
    status='failed'
    try:
        client.initialize()
        initial=next(x['receive'] for x in client.transcript if 'receive' in x and x['receive'].get('id')==1)
        assert initial['result']['capabilities']['textDocumentSync']['change']==2
        client.open(entry,text,version);query('result');done('incremental-capability-and-full-open')

        first=text.index('result');second=text.rindex('res')
        edits=[ranged(text,first,first+6,'answer'),ranged(text,second,second+3,'ans')]
        send(edits,2);text=text.replace('result','answer').replace('return res','return ans');version=2
        query('answer');done('ordered-two-range-rename-and-query')

        # First edit changes line numbers; the second range is in that new state.
        header='// new line\r\n';intermediate=header+text
        first=intermediate.index('answer');second=intermediate.rindex('ans')
        edits=[ranged(text,0,0,header),ranged(intermediate,first,first+6,'result'),
               ranged(intermediate,second,second+3,'res')]
        send(edits,3);text=header+text.replace('answer','result').replace('return ans','return res');version=3
        query('result');done('later-ranges-use-preceding-edit-line-state')

        # A full replacement can precede a ranged change in the same notification.
        unicode='\ufefffunc main(...) as int\r\n string s = "😀"; int result = 7\r\n return res\r\nend\r\n'
        start=unicode.index('result')
        send([dict(text=unicode),ranged(unicode,start,start+6,'result',rangeLength=6)],4)
        text=unicode;version=4;query('result');done('mixed-full-and-range-bom-crlf-non-bmp')

        # Replace astral text using two UTF-16 units, then query at the shifted range.
        at=text.index('😀');send([ranged(text,at,at+1,'中文',rangeLength=2)],5)
        text=text.replace('😀','中文');version=5;query('result');done('utf16-range-length-and-non-ascii-edit')

        stable=text;stable_version=version
        at=text.index('result')
        rollback='// must roll back\n'
        bad=ranged(rollback+text,at+len(rollback),at+6+len(rollback),'broken',rangeLength=5)
        send([ranged(text,0,0,rollback),bad],6)
        query('result',stable,stable_version);done('invalid-later-change-does-not-commit-earlier-change-or-version')
        send([ranged(text,at,at+6,'broken')],stable_version)
        query('result');done('non-increasing-version-is-ignored')

        # Introduce an astral character then reject an edit in its surrogate pair.
        at=text.index('中文');send([ranged(text,at,at+2,'😀')],6)
        text=text.replace('中文','😀');version=6
        pos=position(text,text.index('😀'));pos['character']+=1
        send([dict(range=dict(start=pos,end=pos),text='broken')],7)
        query('result');done('surrogate-interior-edit-does-not-mutate-buffer')

        # Whole text remains a compatibility path for older IDE clients.
        text='func main(...) as int\n int answer = 9\n return ans\nend\n';version=7
        send([dict(text=text)],version);query('answer');done('whole-document-compatibility-after-deltas')
        send([],8);version=8;query('answer');done('empty-change-list-advances-version-without-text-change')

        # Oversized columns clamp to the actual end of the line per LSP Position.
        send([dict(range=dict(start=dict(line=1,character=999),end=dict(line=1,character=999)),text=' ')],9)
        text=text.replace('answer = 9\n','answer = 9 \n');version=9;query('answer');done('columns-beyond-eol-clamp')
        assert entry.read_bytes()==disk
        for row in inputs:assert hashlib.sha256(Path(row['path']).read_bytes()).hexdigest()==row['sha256']
        done('source-compiler-and-server-bytes-preserved');status='passed'
    finally:
        stderr=client.stop()
        (output/'server.stderr.log').write_text(stderr,encoding='utf8')
        (output/'result.json').write_text(json.dumps(dict(status=status,cases=rows,inputs=inputs),ensure_ascii=False,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(status=status,cases=len(rows),proof=str(output)),ensure_ascii=False))


if __name__=='__main__':main()
